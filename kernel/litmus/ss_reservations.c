#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <litmus/litmus.h>
#include <litmus/ss_reservations.h>
#include <asm/siginfo.h>

#include <litmus/sched_trace.h>
#include <litmus/jobs.h>

#include <litmus/trace.h>

/** 
 * @def ONE_MS
 * defines the time length of one millisecond
 * totally 6 zeros to represent 1 millisecond
 * This is multiplied with slot length that 
 * arrives from user space offline table.
*/
#define ONE_MS 1000000

/**
 * @def HP_REPETITIONS
 * defines number of repetitions hyper period should
 * repeat for the desired task to complete its execution
 */
#define HP_REPETITIONS 1

void rt_job_print(struct rt_job *job);

/**
 * @defgroup inline_function
 * ABSTRACT helper functions for accessing structure
 * Makes core logic independent of data structures.
 * @{
 */

inline int get_intr_id(interval *i)
{	return i->id; }

inline void set_intr_id(interval *i, unsigned int value)
{	i->id = value; }

inline int get_intr_sc(interval *i)
{
	if(!i)
		return 0;

	return i->sc;
}

inline void set_intr_sc(interval *i, int value)
{
	if(i)
		i->sc = value;
}

inline unsigned int get_intr_start(interval *i)
{	return i->start; }

inline void set_intr_start(interval *i, int value)
{	i->start = value; }

inline unsigned int get_intr_end(interval *i)
{	return i->end; }

inline void set_intr_end(interval *i, int value)
{	i->end = value; }

inline unsigned int get_intr_repetitions(interval *i)
{	return i->repetitions; }

inline void set_intr_repetitions(interval *i, signed int value)
{	i->repetitions = value; }

inline unsigned int get_task_dl(task *tsk)
{	return tsk->curr_job.dl; }

inline unsigned int get_task_wcet(task *tsk)
{	return tsk->curr_job.wcet; }

inline unsigned int get_task_interval(task *tsk)
{	return tsk->curr_job.job->intr_id; }

inline unsigned int get_task_id(task *tsk)
{	return tsk->s_task->major_id; }

inline unsigned int get_job_id(task *tsk)
{	return tsk->curr_job.minor_id->minor_id; }

/** @} */ 

static enum hrtimer_restart ss_timer_callback(struct hrtimer *timer)
{
	struct slot_shift_reservation *ssres =
		container_of(timer, struct slot_shift_reservation, timer);

	hrtimer_forward_now(timer,
			//ns_to_ktime(ssres->slot_quantum));
			ns_to_ktime(5 * ONE_MS));
	TRACE("SS_TRACE: %s: TIMER CALLBACK AT %llu\n", __FUNCTION__,
							litmus_clock());
	ssres->in_slot_boundary = 1;
	litmus_reschedule_local();

	return HRTIMER_RESTART;
}

void ss_update_sc(struct slot_shift *ss, task* next)
{
	interval *i; 
	interval *curr_intr = ss->dh->get_curr_intr(ss->dh);
	unsigned int intr_id;
	unsigned int is_negative;

	if (!next) {
		TRACE("SS_TRACE: task param(NULL)\n");
		goto UPDATE;
	}

	if(NOT_GUARANTEED == next->q_type) {
		TRACE("SS_TRACE: task is !guaranteed");
		goto UPDATE;
	}

	intr_id = get_task_interval(next);
	i = ss->dh->get_intr_with_id(ss->dh, &intr_id);
	if(!i) {
		TRACE("SS_TRACE: FATAL ERROR: interval id not found:%d\n",
			intr_id);
		goto RET;
	}
	TRACE("SS_TRACE: task belong to interval %d(curr_intr: %d)\n",
						intr_id, get_intr_id(curr_intr) );
       do {
               is_negative = (get_intr_sc(i) >= 0)? 0:1;
               set_intr_sc(i, get_intr_sc(i)+1);
               if(i == curr_intr) {
                       break;
               }
               if(!is_negative) {
                       break;
               }
               i = ss->dh->get_prev_intr(ss->dh, i);
       } while(i);

UPDATE:
       TRACE("SS_TRACE: Decreasing sc of int %d, %d->%d\n",
               get_intr_id(ss->dh->get_curr_intr(ss->dh)),
               get_intr_sc(ss->dh->get_curr_intr(ss->dh)),
               get_intr_sc(ss->dh->get_curr_intr(ss->dh))-1);

       set_intr_sc(ss->dh->get_curr_intr(ss->dh),
               get_intr_sc(ss->dh->get_curr_intr(ss->dh))-1);
RET:
       TRACE("SS_TRACE: Result: intr %d has sc %d\n",
               get_intr_id(ss->dh->get_curr_intr(ss->dh)),
               get_intr_sc(ss->dh->get_curr_intr(ss->dh)));
       TRACE("SS_TRACE: Ending sc update\n");
}

void ss_calculate_sc(struct slot_shift *ss, interval *intv, task *aperiodic)
{
       unsigned int delta = get_task_wcet(aperiodic);
       interval *prev = intv;
       signed int sc = 0;

       do {
               if(!prev) {
                       TRACE("SS_TRACE: APER: WARNING: REACHED BEGINNING\n");
                       break;
               }
               sc = get_intr_sc(prev);
               if(sc > 0) {
                       if(sc >= delta) {
                               sc = sc - delta;
                               set_intr_sc(prev, sc);
                               delta = 0;
                               break;
                       }
                       else {
                               delta = delta - sc;
                               set_intr_sc(prev, -delta);
                       }
               }
               else {
                       sc += -delta;
                       set_intr_sc(prev, sc);
               }
               prev = ss->dh->get_prev_intr(ss->dh, prev);
       } while(delta > 0);
}

unsigned int ss_guarantee_algorithm(struct slot_shift *ss,
                               interval *intr,
                               task *aperiodic)
{
       interval *new_intv = NULL;

       if(get_task_dl(aperiodic) < get_intr_end(intr) ) {

               new_intv = ss->dh->split_interval(ss->dh, intr,
                       get_task_dl(aperiodic),
                       ss->curr_slot);
               if(!new_intv) {
                       TRACE("SS_TRACE: ERROR: Splitting intr failure\n");
                       goto ERR;
               }
               TRACE("SS_TRACE: new intr:: id: %d start:%d end:%d sc:%d\n",
                       get_intr_id(new_intv), get_intr_start(new_intv),
                       get_intr_end(new_intv), get_intr_sc(new_intv) );
               if(get_intr_start(new_intv) <= ss->curr_slot &&
                  get_intr_end(new_intv)   >= ss->curr_slot) {
                       TRACE("SS_TRACE: updating curr interval to:%d\n",
                               get_intr_id(new_intv) );

                       ss->dh->update_curr_intr(ss->dh, new_intv);
               }
       }
       aperiodic->curr_job.job->intr_id = new_intv?
                       get_intr_id(new_intv) :
                       get_intr_id(intr);
	ss->algo->calculate_sc(ss, (new_intv? new_intv: intr), aperiodic);
	return 0;
	ERR:
		return -1;
}

#if 1 /// original Version of guarantee Algorithm.

static unsigned int _calculate_wcet(interval *intr)
{
	struct ss_container *prsptve;
	struct ss_task_struct *ss_tsk;
	struct minor_id_bond *mid;
	unsigned int wcet_sum = 0;

	list_for_each_entry(prsptve, &intr->tasks, list) {
		ss_tsk = (struct ss_task_struct*)prsptve->perspective;
		mid = ss_tsk->curr_job.minor_id;
		wcet_sum += mid->wcet;
	}
	return wcet_sum;
}

static unsigned int _get_intr_len(interval *intr)
{
	return (get_intr_end(intr) - get_intr_start(intr));
}

static void _make_intr_prspective(interval *intr, task *aper)
{
	struct ss_container *intr_pesptve;

	intr_pesptve = kmalloc(sizeof(*intr_pesptve),
					 GFP_ATOMIC);
	intr_pesptve->perspective = aper;
	list_add(&intr_pesptve->list, &intr->tasks);
}

static void ss_calculate_sc_orig(struct slot_shift *ss,
			interval *intv,
			task *aperiodic)
{
	interval *intr;
	interval *next_intv;
	int sc_nxt_intv;
	unsigned int wcet_sum;
	unsigned int intr_len;

	intr = intv;
	do {
		if(!intr) {
			TRACE("SS_TRACE: ERROR: Reached Begininng\n");
			break;
		}
		wcet_sum = _calculate_wcet(intr);
		//wcet_sum = 0;
		next_intv = ss->dh->get_nxt_intr(ss->dh, intr);
		sc_nxt_intv = get_intr_sc(next_intv);
		intr_len = _get_intr_len(intr);

		TRACE("SS_TRACE: SC update: intr:%d prevSC: %d",
			get_intr_id(intr), get_intr_sc(intr));

		set_intr_sc(intr,
			( intr_len - (wcet_sum + min(0, sc_nxt_intv) )));
		TRACE(" updatedSC: %d\n", get_intr_sc(intr));
		if(intr == ss->dh->get_curr_intr(ss->dh))
			break;

		intr = ss->dh->get_prev_intr(ss->dh, intr);
	} while(1);
}

unsigned int ss_guarantee_algorithm_orig(struct slot_shift *ss,
					interval *intr,
					task *aperiodic)
{
	interval *new_intv = NULL;

	if(get_task_dl(aperiodic) < get_intr_end(intr) ) {

		TRACE("SS_TRACE: APER: NEEDS SPLIT\n");

		new_intv = ss->dh->split_interval(ss->dh, intr,
				get_task_dl(aperiodic),
				ss->curr_slot);

		if(get_intr_start(new_intv) <= ss->curr_slot &&
		   get_intr_end(new_intv)   >= ss->curr_slot) {
			TRACE("SS_TRACE: updating curr interval to:%d\n",
				get_intr_id(new_intv) );

			ss->dh->update_curr_intr(ss->dh, new_intv);
		}
	}
	_make_intr_prspective((new_intv? new_intv: intr), aperiodic);

	aperiodic->curr_job.job->intr_id = new_intv?
			get_intr_id(new_intv) :
			get_intr_id(intr);

	ss->algo->calculate_sc(ss, (new_intv? new_intv: intr), aperiodic);

	return 0;
}
#endif


unsigned int ss_acceptance_test(struct slot_shift *ss)
{
	unsigned int num_accepted = 0;
	task *aperiodic = NULL;
	unsigned int loop_cnt = 0;

	while ((aperiodic = ss->dh->get_unConcluded_task(ss->dh,
			&ss->curr_slot))) {

		unsigned int dl = get_task_dl(aperiodic);
		unsigned int dl_intr_sc;
		interval* i =  NULL;
		interval *curr = ss->dh->get_curr_intr(ss->dh);
		int sum = 0;
		int last_intr_delta = 0;
 
		TRACE("SS_TRACE: ACCEPTANCE TEST RUNNING FROM INTR: %d\n",
		get_intr_id(curr) );

		do {
			sum +=  max(0, get_intr_sc(i));
			i = (i == NULL)? curr : 
					ss->dh->get_nxt_intr(ss->dh, i);
		} while(get_intr_end(i) < dl);

		last_intr_delta = (i == curr)? ss->curr_slot :
						get_intr_start(i);

		dl_intr_sc = min(get_intr_sc(i),
				(int)(dl - last_intr_delta) );

		sum += max(0, dl_intr_sc);

		TRACE("SS_TRACE: ACC: wcet: %d, sum:%d "
			"intr_id: %d[start: %d, end: %d]\n",
			 get_task_wcet(aperiodic), sum,
			get_intr_id(i),get_intr_start(i),
			get_intr_end(i) );

		/* FIXME: This expects the aperiodic to have only one job. If
		 * it has more, I will need to iterate through all its jobs. */
		if (sum >= get_task_wcet(aperiodic)) {
TS_GUARANTEE_START;
			if(!ss->algo->guarantee_algo(ss, i, aperiodic) ) {
				ss->dh->update_tsk_q_state(ss->dh,
						 aperiodic, GUARANTEED);
TS_GUARANTEE_END;
				num_accepted++;
				TRACE("SS_TRACE: APERIODIC IS GUARANTEED\n");
			}
			else {
				TRACE("SS_TRACE: APER: ERROR in GUARANTEE\n");
				list_del_init(&aperiodic->list);
			}

		} else {
			TRACE("SS_TRACE: APERIODIC !GUARANTEED: sum:%d wcet:%d\n"
						, sum, get_task_wcet(aperiodic) );
			ss->dh->update_tsk_q_state(ss->dh,
					aperiodic, NOT_GUARANTEED);
		}
		loop_cnt++;
	}
	return num_accepted;
}

task* ss_core(struct slot_shift *ss)
{
	interval *i = ss->dh->get_curr_intr(ss->dh);
	task *next = NULL, *curr;
	int job_completed = 0;
	unsigned int aper_accepted_count;

	if (!i) {
		TRACE("SS_TRACE: WARNING: INTERVAL NULL curr_slot: %d\n", 
								ss->curr_slot);
		return NULL;
	}
	ss->curr_slot++;
	TRACE("SS_TRACE:  interval: %d(SC: %d), ss->curr_slot: %d, \n",
			 get_intr_id(i), get_intr_sc(i), ss->curr_slot);

	if (ss->curr_slot >= get_intr_end(i)) {
		interval *old_i = i;

		i = ss->dh->update_curr_intr(ss->dh,
				ss->dh->get_nxt_intr(ss->dh, i));
		if (!i)
			return NULL;
		if (get_intr_repetitions(old_i) < 0 ) {
			ss->dh->remove_interval(ss->dh, old_i);
		}
		else {
		 	ss->hp_ext->reinsert_past_interval(ss, old_i);
		}
	}
	curr = ss->dh->get_curr_task(ss->dh);
	if(curr) {
		ss->dh->update_tsk_quantum(ss->dh, curr, &job_completed);
		if (job_completed)
			ss->os->krnl_side_job_complete(ss, curr);
	}
	aper_accepted_count = ss->algo->acceptance_test(ss);
	ss->algo->ready_queue_update(ss);
	next = ss->dh->get_nxt_rdy_task(ss->dh, &ss->curr_slot);
	ss->dh->update_tsk_state(ss->dh, next, RUNNING);
	ss->algo->update_sc(ss, next);
	ss->os->on_job_decn(ss);
	return next;
}

static void ss_ready_queue_update(struct slot_shift *ss)
{
	struct ss_task_struct *new;

	while((new = ss->dh->release_task_with_curr_est(
				ss->dh, &ss->curr_slot)) ) {
		if(!ss->dh->update_tsk_state(ss->dh, new, READY))
			continue;
		break;
	}
}

static struct task_struct* ss_dispatch_client(
	struct reservation *res, lt_t *for_at_most)
{
	struct slot_shift_reservation *ssres = get_slot_shift_reservation(res);
	struct ss_task_struct* t;
	struct task_struct *os_task = NULL;

	if(ssres->in_slot_boundary) {
		ssres->in_slot_boundary = 0;
		t = (struct ss_task_struct*) ssres->ss.algo->
					slot_shift_core(&ssres->ss);
		os_task = t ? t->os_task : NULL;
	} else {
		struct slot_shift *ss = &ssres->ss;

		if((t = ss->dh->get_curr_task(ss->dh))) {
			os_task = t->os_task;
			if(is_blocked(os_task) &&
			(PERIODIC == t->type)) {
				TRACE("SS_TRACE: task is blocked\n");
				ss->dh->update_tsk_state(ss->dh, 
							os_task, BLOCKED);
				os_task = NULL;
			}
		}
		TRACE("SS_TRACE: DISPATCH IS CALLED, !in_slot_boundry\n");
	}
	return os_task;
}

/*---------------------- DATA HANDLING FUNCTIONS------------------------- */

void print_queue_job(struct ss_queue_job *job) {

	TRACE("SS_TRACE: curr_job updated is:\n");
	TRACE("SS_TRACE: \test: %d\n", job->est);
	TRACE("SS_TRACE: \tdl: %d\n", job->dl);
	TRACE("SS_TRACE: \twcet: %d\n", job->wcet);
	TRACE("SS_TRACE: \tjindex: %d\n", job->jindex);
}

void print_job(struct job_bond *job) {

	TRACE("SS_TRACE: \t: intr_id: %d\n", job->intr_id);
	TRACE("SS_TRACE: \t: est: %d\n", job->est);
	TRACE("SS_TRACE: \t: exec: %d\n", job->exec_count);
}
void print_tsk(struct ss_task_struct *tsk) {

	printk("=========TASK ADDED:========\n");
	printk("task:%p\n", tsk);
	printk("task type: %d\n", tsk->type);
	printk("task state: %d\n", tsk->state);
	printk("task q_type: %d\n", tsk->q_type);
	printk("task ss_task: %p\n", tsk->s_task);
	printk("task curr_job: %p\n", &tsk->curr_job);
	printk("\tcurr_job.est: %d\n", tsk->curr_job.est);
	printk("\tcurr_job.dl: %d\n", tsk->curr_job.dl);
	printk("\tcurr_job.wcet: %d\n", tsk->curr_job.wcet);
	printk("\tcurr_job.index: %d\n", tsk->curr_job.jindex);
	printk("\tcurr_job.minor_id: %p\n", tsk->curr_job.minor_id);
	printk("\tcurr_job.job: %p\n", tsk->curr_job.job);
	printk("\t\tjob.intr_id: %d\n", tsk->curr_job.job->intr_id);
	printk("\t\tjob.est: %d\n", tsk->curr_job.job->est);
	printk("\t\tjob.exec: %d\n", tsk->curr_job.job->exec_count);
	printk("task os_task: %p\n", tsk->os_task);

}

#define OFFSET(absl_addr, rel_addr) \
	((void*)((long)absl_addr) \
	+ ((long)rel_addr))
static struct minor_id_bond* fetch_minor_id(struct ss_task *task,
					const unsigned int idx) {
	if(idx >= task->minor_id_count)
		return NULL;

	return OFFSET(task, task->minor_id_bonds[idx]);
}

static struct job_bond* fetch_nxt_job(
			struct ss_task_struct *task,
			const unsigned int jidx) {

	struct ss_queue_job *curr_job;
	struct minor_id_bond *mid;
	struct job_bond *job = NULL;

	if(!task)
		return NULL;
	curr_job = &task->curr_job;
	mid = curr_job->minor_id;
	if(!mid)
		return NULL;
	if(jidx < mid->intr_count) {
		job = &mid->intr_bond[jidx];
	}
	return job;
}

static void update_curr_job(struct ss_task_struct *task,
			struct minor_id_bond *mid,
			struct job_bond *job,
			const unsigned int jidx) { 

	task->curr_job.job = job;
	task->curr_job.minor_id = mid;
	task->curr_job.est = job->est;
	task->curr_job.dl = mid->dl + job->est;
	task->curr_job.wcet = mid->wcet;
	task->curr_job.jindex = jidx;
	print_queue_job(&task->curr_job);
}

struct ss_task_struct* ss_task_struct_init(
				enum ss_task_type type,
				struct ss_task *s_tsk,
				pid_t pid) 
{
	struct ss_task_struct *tsk;
	struct job_bond *job = NULL;
	struct task_struct *os_tsk;

	printk("SS_TRACE: major_id: %d, minor_id count: %d\n",
			s_tsk->major_id, s_tsk->minor_id_count);
	tsk = kmalloc(sizeof(*tsk), GFP_KERNEL);
	memset(tsk,0x00, sizeof(*tsk));
	tsk->type = type;
	tsk->state = JUST_ARRIVED;
	tsk->s_task = s_tsk;
	tsk->curr_job.minor_id = fetch_minor_id(tsk->s_task,0);
	tsk->curr_job.jindex = 0;
	job = fetch_nxt_job(tsk, tsk->curr_job.jindex);
	if(!job) 
		goto ERROR;
	update_curr_job(tsk, 
			tsk->curr_job.minor_id,
			job,
			tsk->curr_job.jindex);
	INIT_LIST_HEAD(&tsk->curr_job.list);
	read_lock_irq(&tasklist_lock);	
	if(!(os_tsk = find_task_by_vpid(pid) )) {
		printk("failed to init linux task\n");
		goto UNLOCK_ERR;
	}
	read_unlock_irq(&tasklist_lock);
	tsk->os_task = os_tsk;
	INIT_LIST_HEAD(&tsk->intr_list);
	INIT_LIST_HEAD(&tsk->list); 
	tsk_rt(os_tsk)->slot_shift_task = tsk;
	print_tsk(tsk);

	return tsk;

	UNLOCK_ERR:
		read_unlock_irq(&tasklist_lock);
	ERROR:
		kfree(tsk);
		return NULL;
}

void ss_initialize_list(struct ss_data_hndl_fn *data_class) {

	INIT_LIST_HEAD(&data_class->ready_list);
	INIT_LIST_HEAD(&data_class->blkd_list);
	INIT_LIST_HEAD(&data_class->unConcluded_list);
	INIT_LIST_HEAD(&data_class->sft_aper_list);
	INIT_LIST_HEAD(&data_class->interval_list);
	INIT_LIST_HEAD(&data_class->task_list);
	data_class->curr_intr = NULL;
	data_class->curr_task = NULL;
}

void add_interval(
	struct ss_data_hndl_fn *data_class,
	void *intr) {

	struct ss_intr_struct *t_intr;
	struct ss_task_struct *task;
	struct ss_container *intr_pesptve;
	struct ss_container *tsk_pesptve;
	int i;
	unsigned int tsk_cnt;

	if(!intr || !data_class)
		return;

	t_intr = kmalloc(sizeof(*t_intr), GFP_KERNEL);

 	t_intr->intr_struct = intr;
	INIT_LIST_HEAD(&t_intr->tasks);
	INIT_LIST_HEAD(&t_intr->list);

	tsk_cnt = t_intr->intr_struct->intr.no_of_tsk;
	t_intr->id = t_intr->intr_struct->intr.intr_id;
	t_intr->sc = t_intr->intr_struct->intr.sc;
	t_intr->start = t_intr->intr_struct->intr.start;
	t_intr->end = t_intr->intr_struct->intr.end;
	t_intr->repetitions = 1;
	for(i = 0; i< tsk_cnt; i++) {
		task = data_class->get_task(data_class,
				&t_intr->intr_struct->tsks[i].major_id);

		TRACE("SS_TRACE Interval %d got a major id: %d\n",
				t_intr->id,
				t_intr->intr_struct->tsks[i].major_id);
		/* TODO : TOO Much thinking always messes up,
			this is a very good example of that,
			NEEDS TO BE REMOVED :|
		*/
		if(!task)
			break;
		intr_pesptve = kmalloc(sizeof(*intr_pesptve),
						 GFP_KERNEL);
		intr_pesptve->perspective = task;
		list_add(&intr_pesptve->list, &t_intr->tasks);
		tsk_pesptve = kmalloc(sizeof(*tsk_pesptve), 
						GFP_KERNEL);
		tsk_pesptve->perspective = t_intr;
		list_add(&tsk_pesptve->list, 
				&task->intr_list);
	}
	list_add(&t_intr->list,
		&data_class->interval_list);
	data_class->intr_cnt++;

	return;
}

int add_task(
	struct ss_data_hndl_fn *data_class,
	struct ss_task_struct *task)
{
	return data_class->update_tsk_state(data_class,
					task, JUST_ARRIVED);
}

void add_guaranteed_task(
		struct ss_data_hndl_fn *data_class,
		void *_task)
{
	struct ss_task_struct *tsk = _task;

	list_add(&tsk->list, &data_class->task_list);
	if(HARD_APERIODIC == tsk->type)
		data_class->tsk_cnt.h_aper_tasks--;
	data_class->tsk_cnt.guaranteed_tasks++;

	return;
}

void add_unConcluded_task(
		struct ss_data_hndl_fn *data_class,
		void *_task) {
	
	struct ss_task_struct *task;

	task = (struct ss_task_struct*)_task;
	task->q_type = NOT_DECIDED;
	list_add(&task->list, &data_class->unConcluded_list);
	data_class->tsk_cnt.h_aper_tasks++;
	return;
}

void add_notGuaranteed_task(
		struct ss_data_hndl_fn *data_class,
		void *_task) {
	
	struct ss_task_struct *task;

	task = (struct ss_task_struct*)_task;
	task->q_type = NOT_GUARANTEED;
	list_add(&task->list, &data_class->sft_aper_list);
	data_class->tsk_cnt.not_guaranteed_tasks++;
	
	return;
}

void add_to_block_list(
		struct ss_data_hndl_fn *data_class,
		void *_task)
{
	struct ss_task_struct *tsk = _task;
	struct ss_queue_job *job = &tsk->curr_job;

	list_add_tail(&job->list, &data_class->blkd_list);

	return;
}

void* get_nxt_intr(struct ss_data_hndl_fn *data_class,
			void *curr_intr)
{
	struct ss_intr_struct *intr = curr_intr;

	if (intr) {
		intr = list_next_entry(intr, list);
	}
	else {
		intr = list_first_entry(&data_class->interval_list,
			struct ss_intr_struct, list);
	}
	return intr;
}

void* get_prev_intr(
		struct ss_data_hndl_fn *data_class,
		void *curr_intr)
{
	struct ss_intr_struct *intr = NULL;
	struct ss_intr_struct *curr = curr_intr;

	if (curr) {
		intr = list_prev_entry(curr, list);
		if(&intr->list == &data_class->interval_list) {
			intr = NULL;
		}
	}
	return intr;
}

void* get_unConcluded_task(
		struct ss_data_hndl_fn *data_class,
		void *nxt_slot) {

	if(list_empty(&data_class->unConcluded_list))
		return NULL;

	return list_first_entry_or_null(&data_class->unConcluded_list,
				struct ss_task_struct,
				list);
}

void* get_notGuaranteed_task(
		struct ss_data_hndl_fn *data_class,
		void *nxt_slot) {

	if(list_empty(&data_class->sft_aper_list))
		return NULL;
 
	return list_first_entry_or_null(&data_class->sft_aper_list,
				struct ss_task_struct,
				list);
}

void* release_task_with_curr_est( 
		struct ss_data_hndl_fn *data_class,
		void *_nxt_slot_no) {

	struct ss_task_struct *tsk_pos = NULL;
	struct ss_task_struct *nxt;
	lt_t *nxt_slot_no = (lt_t *)_nxt_slot_no;

	if(list_empty(&data_class->task_list)) {
		goto RET;
	}
	list_for_each_entry_safe(tsk_pos, nxt, 
			&data_class->task_list, list) {

		if(DORMANT != tsk_pos->state)
			continue;
		if(*nxt_slot_no == tsk_pos->curr_job.est) {
			goto RET;
		}
	}
	tsk_pos = NULL;

	RET:
		return tsk_pos;
}

void add_to_ready_list(
		struct ss_data_hndl_fn *data_class,
		void *_task) {

	struct list_head *pos, *pos_nxt;
	struct ss_task_struct *task = _task;
	struct ss_queue_job *q_job;
	struct ss_queue_job *job = &task->curr_job;

	if(!task) {
		TRACE("SS_TRACE: ERROR: task is empty in rdy q update\n");
		return;
	}
	if(list_empty(&data_class->ready_list)) {
		pos = &data_class->ready_list;
		goto add_to_list;
	}
	//pos = &data_class->ready_list;
	list_for_each_safe(pos, pos_nxt, &data_class->ready_list) {
		q_job = list_entry(pos,struct ss_queue_job, list);
		if(q_job->dl <= job->dl) {
			 //pos = pos_nxt;
			continue;
		}
		break;
	}

	add_to_list:
		list_add_tail(&job->list, pos);
	return;
}

void* get_nxt_rdy_task(struct ss_data_hndl_fn *data_class, void *nxt_slot) 
{
	struct ss_queue_job *job;
	struct ss_task_struct *tsk;

	if(list_empty(&data_class->ready_list) ) {
		return NULL;
	}
	job = list_first_entry(&data_class->ready_list,
				struct ss_queue_job,
				list);
	tsk = container_of(job,
			struct ss_task_struct,
			curr_job);
	return tsk;
}

void* get_intr_with_id(struct ss_data_hndl_fn *data_class, void *_id)
{
	struct ss_intr_struct *curr, *i = NULL;
	unsigned int *id = _id;

	list_for_each_entry(curr, &data_class->interval_list, list) {
		if (get_intr_id(curr) == (*id)) {
			i = curr;
			break;
		}
	}
	return i;
}

void* update_task_quantum(struct ss_data_hndl_fn *data_class,
			void *_task, int* job_completed) {
	
	struct ss_task_struct *task = _task;
	unsigned int *executed;
	enum ss_task_state state = UNKNOWN;

	*job_completed = 0;

	if(task->state != RUNNING) {
		TRACE(" SS_TRACE: quantum: error in task state: %d\n",
			task->state);
		goto STATE_UPDATE;
	}
	executed = &task->curr_job.job->exec_count;
	(*executed)++;
	print_job(task->curr_job.job);

	if(*executed >= task->curr_job.wcet) {

		struct job_bond *nxt_job;
		unsigned int j_idx;

		if(PERIODIC != task->type) {
			TRACE("SS_TRACE: APERIODIC FINISHED\n");
			 state = FINISHED;
			 goto STATE_UPDATE;
		}
		state = DORMANT;
		j_idx = task->curr_job.jindex;
		j_idx++;
		nxt_job = fetch_nxt_job(task, j_idx);

#if 1 // TODO: THIS NEEDS A BETTER ABSTRACTION AND SHOULD BE DISJOINT.
		if (!nxt_job) {
			j_idx = 0;
			nxt_job = fetch_nxt_job(task, j_idx);
		}

#endif
		if(!nxt_job) {
			state = FINISHED;
		} else {
			update_curr_job(task,
				task->curr_job.minor_id,
				nxt_job,
				j_idx);

			*job_completed = 1;
		}
	} else if(GUARANTEED == task->q_type){
		state = READY;
	} else if(NOT_GUARANTEED == task->q_type){
		state = DORMANT;
	} else {
		TRACE("SS_TRACE: QUANTUM UPDATE: QSTATE ERROR\n");
		task = NULL;
	}

	STATE_UPDATE:
		data_class->update_tsk_state(data_class, 
						task, state);
	return task;
}

static void remove_curr_queue(struct ss_data_hndl_fn *data_class,
		 struct ss_task_struct *task) {
	
	switch(task->state) {

		case INDEFINITE:
			if(!list_empty(
				&data_class->unConcluded_list)) {
				list_del_init(&task->list);
				break;
			}

		goto ERR;

		case READY:
		case BLOCKED:
			if(!list_empty(&data_class->ready_list)) {
				list_del_init(&task->curr_job.list);
				break;
			}
		goto ERR;

		case RUNNING:
			if(data_class->curr_task) {
				data_class->curr_task = NULL;
				break;
			}

		goto ERR;

		default:
			goto ERR;
		break;
	}
	return;

	ERR: 
		TRACE("SS_TRACE: PREV LIST ALREADY EMPTY/ Default called\n");
}

static int update_nxt_queue(struct ss_data_hndl_fn *data_class,
			struct ss_task_struct  *_task, 
			enum ss_task_state nxt_state) {

	struct ss_task_struct *task = _task;

	switch(nxt_state) {
		case DORMANT:
			if(GUARANTEED == task->q_type)
				data_class->add_guaranteed_task(data_class,
							task);
			else if(NOT_GUARANTEED == task->q_type)
				data_class->add_notGuaranteed_task(data_class,
								task);
			else
				goto ERROR;
		break;

		case INDEFINITE:
			if(HARD_APERIODIC != task->type)
				goto ERROR;

			data_class->add_unConcluded_task(
					data_class,
					task);
		break;

		case RUNNING:
			data_class->update_curr_tsk(
					data_class,
					task);
		break;

		case BLOCKED:
			data_class->add_block_list(
					data_class,
					task );
		break;

		case READY:
			data_class->add_ready_list(
					data_class,
					task );
		break;

		case FINISHED:
			if(PERIODIC == task)
				task->os_task = NULL;
			else {
				list_del_init(&task->list);
			}

		break;

		default:
			goto ERROR;
	}
	return 0;

	ERROR:	
		TRACE("SS_TRACE: ERROR: %s: task state machine Failure\n",
			__FUNCTION__);
		return -1;
}

static int update_tsk_state(struct ss_data_hndl_fn *data_class,
			void *_task, 
			enum ss_task_state nxt_state) 
{
	struct ss_task_struct *task = _task;
	int ret = 0;

	if(!task)
		goto RETURN;

	switch(nxt_state) {
		case DORMANT:
			if(INDEFINITE == task->state) {
				goto UPDATE_BOTH_QUEUE;
			} 
			else if(RUNNING == task->state)
				goto UPDATE_CURR_QUEUE;
			
			goto ERROR;
		break;

		case RUNNING:
			if(READY == task->state) {
				goto UPDATE_BOTH_QUEUE;
			} else if(DORMANT == task->state &&
			NOT_GUARANTEED == task->q_type) {
				goto UPDATE_NXT_QUEUE;
			}

			goto ERROR;
		break;

		case FINISHED:
			if(RUNNING != task->state)
				goto ERROR;

			goto UPDATE_BOTH_QUEUE;
		break;

		case JUST_ARRIVED:
			if(SOFT_APERIODIC == task->type) {
				nxt_state = DORMANT;
				task->q_type = NOT_GUARANTEED;
			} else if(PERIODIC == task->type) {
				nxt_state = DORMANT;
				task->q_type = GUARANTEED;
			} else if(HARD_APERIODIC == task->type) {
				nxt_state = INDEFINITE;
				task->q_type = NOT_DECIDED;
			} else {
				goto ERROR;
			}
			goto UPDATE_NXT_QUEUE;
		break;

		case READY:
			if(DORMANT == task->state
			    && GUARANTEED == task->q_type) {
				goto UPDATE_NXT_QUEUE;
			} else if(BLOCKED == task->state
			    || RUNNING == task->state) {
				goto UPDATE_BOTH_QUEUE;
			}
			goto ERROR;
		break;

		case BLOCKED:
			if(RUNNING != task->state) {
				TRACE("SS_TRACE: Error: %dstate to BLK\n", task->state);
				goto ERROR;
			}
			goto UPDATE_BOTH_QUEUE;
		break;

		case INDEFINITE: // this is internal state
		default:
			TRACE("SS_TRACE: DEFAULT ERROR");
			goto ERROR;
	}

	UPDATE_BOTH_QUEUE:
		remove_curr_queue(data_class,task);
	UPDATE_NXT_QUEUE:
		task->state = nxt_state;
		TRACE("SS_TRACE: task->state: %d\n", task->state);
		return update_nxt_queue(data_class, task, nxt_state);
	UPDATE_CURR_QUEUE:
		remove_curr_queue(data_class,task);
		task->state = nxt_state;
		TRACE("SS_TRACE: task->state: %d\n", task->state);
	RETURN:
		return ret;
	ERROR:
		TRACE("SS_TRACE: update_task_state task state machine Failure\n");
		return -1;
}

static void* ss_split_intr(
		struct ss_data_hndl_fn *data_class,
		void *intr,
		int split_point,
		lt_t curr_slot) {

	struct ss_intr_struct *curr_intr = intr;
	struct ss_intr_struct *new_intr;
	unsigned int start;
	unsigned int end;
	signed int new_intr_len;

	new_intr = kmalloc(sizeof(*new_intr), GFP_ATOMIC);	
	new_intr->intr_struct = kmalloc(
			sizeof(*new_intr->intr_struct),
			GFP_ATOMIC);
	INIT_LIST_HEAD(&new_intr->tasks);
	INIT_LIST_HEAD(&new_intr->list);
	data_class->intr_cnt++;
	set_intr_id(new_intr, data_class->intr_cnt);
	start = curr_intr->intr_struct->intr.start;
	set_intr_start(new_intr, start);
	end = split_point;
	set_intr_end(new_intr, end);
	set_intr_repetitions(new_intr, -1);
	set_intr_start(curr_intr, end);

	if(intr == data_class->get_curr_intr(data_class)) {
		if((split_point) >= curr_slot) {
			new_intr_len = (split_point) - curr_slot;
		}
		else {
			goto ERR;
		}
	}
	else {
		new_intr_len = (split_point) - start;
	}
	if(new_intr_len < 0) {
		goto ERR;
	}
	set_intr_sc(curr_intr, (get_intr_sc(curr_intr) - new_intr_len) );
	set_intr_sc(new_intr, (new_intr_len + 
				min(0, get_intr_sc(curr_intr)) ) );
	list_add_tail(&new_intr->list,&curr_intr->list);

	return new_intr;

	ERR:
		kfree(new_intr->intr_struct);
		kfree(new_intr);
		return NULL;
}

static void* get_task(
	struct ss_data_hndl_fn *data_class,
	void *_major_id) {

	struct ss_task_struct *tsk = NULL;
	struct ss_task *ss_tsk;
	int *major_id = (int *)_major_id;

	if(!(&data_class->task_list) )
		goto RET;

	list_for_each_entry(tsk, 
			&data_class->task_list, list) {
		ss_tsk = tsk->s_task;
		if(*major_id == ss_tsk->major_id)
			return tsk;
	}
	RET:
		return NULL;
}

static void* get_curr_task(struct ss_data_hndl_fn *data_class) {

	struct ss_queue_job *job;

	if (!data_class->curr_task)
		return NULL;
	job = list_entry(data_class->curr_task,
			struct ss_queue_job, list);

	return container_of(job, struct ss_task_struct, curr_job);
}

void update_curr_task(struct ss_data_hndl_fn *data_class,
			void *_task) {

	struct ss_task_struct *task = _task;

	task->state = RUNNING;
	data_class->curr_task = &task->curr_job.list;
}

void* get_curr_intr(struct ss_data_hndl_fn *data_class)
{
	if (!data_class->curr_intr) {
		struct ss_intr_struct *i = data_class->get_nxt_intr(
						data_class, NULL);
		data_class->curr_intr = &i->list;
	}
	if (data_class->curr_intr == &data_class->interval_list) {
		return list_first_entry(data_class->curr_intr,
			struct ss_intr_struct, list);
	}

	return list_entry(data_class->curr_intr,
			struct ss_intr_struct, list);
}

static void* update_curr_interval(
		struct ss_data_hndl_fn *data_class,
		void *_intr)
{
	struct ss_intr_struct *intr = _intr;

	if (&intr->list == &data_class->interval_list) {
		return NULL;
	}
	data_class->curr_intr = &intr->list;
	return list_entry(data_class->curr_intr, struct ss_intr_struct, list);
}

static void remove_interval(
		struct ss_data_hndl_fn *data_class,
		void *_intr )
{
	struct ss_intr_struct *intr = _intr;
	list_del_init(&intr->list);
	kfree(intr->intr_struct);
	kfree(intr);
}

static int update_tsk_q_state(
		struct ss_data_hndl_fn* data_class,
		void *_task,
		enum ss_task_queue_type q_type) {
	
	struct ss_task_struct *task = _task;

	if(SOFT_APERIODIC == task->type 
	&& GUARANTEED == q_type)
		goto ERROR;

	if(PERIODIC == task->type
	&& NOT_GUARANTEED == q_type)
		goto ERROR;
	
	task->q_type = q_type;
	data_class->update_tsk_state(data_class, 
				task, DORMANT);

	return 0;	
	ERROR:
		return -1;

}

void ss_reinsert_past_interval (struct slot_shift *ss, void *_intr)
{
	struct ss_intr_struct *tail_intr, *intr = _intr;
	int intr_length = (intr->intr_struct->intr.end -
				intr->intr_struct->intr.start);
	int hp_length, old_id = get_intr_id(intr);
	struct ss_container *it;


	if( get_intr_repetitions(intr) > HP_REPETITIONS)
		return;

	list_move_tail(&intr->list, &ss->dh->interval_list);

	/* FIXME: This won't work if there is only one interval in the list */
	tail_intr = list_entry(intr->list.prev, struct ss_intr_struct, list);
	hp_length = tail_intr->end - intr->start;
	set_intr_id(intr, get_intr_id(tail_intr) + 1);
	set_intr_sc(intr, intr->intr_struct->intr.sc);
	set_intr_end(intr, get_intr_end(tail_intr) + intr_length);
	set_intr_start(intr, get_intr_end(tail_intr));
	set_intr_repetitions(intr, get_intr_repetitions(intr) + 1);
	list_for_each_entry(it, &intr->tasks, list) {

		struct ss_task_struct *task = it->perspective;
		int idx = 0;
		struct job_bond *job = fetch_nxt_job(task, idx);

		while (job != NULL) {
			if (job->intr_id == old_id) {
				job->intr_id = get_intr_id(intr);
				job->est = job->est + hp_length;
				job->exec_count = 0;
			}
			idx++;
			job = fetch_nxt_job(task, idx);
		}
	}
}

#if 1 //aperiodic initialisation

void* get_nxt_aper(struct ss_init_phase *diese) {

	struct ss_task_struct *tsk; //diese->aperiodic_tasks;

	tsk = list_first_entry_or_null(&diese->aperiodic_tsks,
					struct ss_task_struct,
					list);
	if(tsk) {
		list_del_init(&tsk->list);
	}
	return tsk;
}

void add_aper(struct ss_init_phase *diese, void *_tsk) {

	struct ss_task_struct *tsk = _tsk;

	list_add_tail(&tsk->list, &diese->aperiodic_tsks);
}

void* init_per(struct ss_init_phase *diese, 
		struct ss_data_hndl_fn *data_obj, 
		void *_time) 
{
	__u64 *time = _time;
	struct ss_task_struct *tsk_pos = NULL;
	struct ss_task_struct *nxt;

	if(list_empty(&data_obj->task_list)) {
		TRACE("SS_TRACE: task list is empty");
		return NULL;
	}
	list_for_each_entry_safe(tsk_pos, nxt, 
			&data_obj->task_list, list) {

			struct task_struct *task = tsk_pos->os_task;			

			tsk_rt(task)->sporadic_release = 1;
			tsk_rt(task)->sporadic_release_time = *time;
			prepare_for_next_period(task);
			tsk_rt(task)->job_params.job_no = 0;

			TRACE("SS_TRACE: (%d) tsk_client->rel_time: %lld, "
				"\n",
				task->pid,
				tsk_rt(task)->sporadic_release_time);
	}
	return NULL;
}

static void set_aper_count(struct ss_init_phase *diese,unsigned int count) {
	diese->aper_cnt = count; 
}

static struct ss_init_phase init_aper = {
	.aperiodic_tsks = LIST_HEAD_INIT(init_aper.aperiodic_tsks),
	.add_aper_tsk = add_aper,
	.get_nxt_aper = get_nxt_aper,
	.init_per_tsks = init_per,
	.set_aper_cnt = set_aper_count,
	.aper_cnt = 0,
	.tick_cnt = 0,
};

static task*  ss_init_core(struct slot_shift *ss)
{
	struct slot_shift_reservation *ssres;
	struct ss_task_struct* t;
	struct ss_init_phase *init = ss->init;
	struct ss_data_hndl_fn *dh = ss->dh;

	ssres = container_of(ss, struct slot_shift_reservation, ss);

	if(init->aper_cnt) {
		TRACE("SS_TRACE: STILL ALL APER NOT ARRIVED\n");
		ssres->in_slot_boundary = 0;
		goto RET;
	}
	if(!(t = init->get_nxt_aper(init))) {
		
		if(init->tick_cnt <= 22) {
			TRACE("SS_TRACE: TICK CNT %d\n", init->tick_cnt);
			init->tick_cnt++;
			ssres->in_slot_boundary = 0;
			goto RET;		
		}
		else {
			__u64 expiry_ns;

			hrtimer_try_to_cancel(&ssres->timer); /*>= 0) { */
			TRACE("SS_TRACE: APERIODICS ARE NULL SO MOVING TO SLOT SHIFT\n");
			ss->algo->slot_shift_core = ss_core;
			TRACE("SS_TRACE: Initing periodics and Timer before moving to SS\n");
			__hrtimer_start_range_ns(&ssres->timer,
				ns_to_ktime(5 * ONE_MS),
				0 /* timer coalescing slack */,
				HRTIMER_MODE_REL,
				0 /* wakeup */);
			if(ssres->in_slot_boundary)
				ssres->in_slot_boundary = 0;

			ssres->ss.curr_slot = -1;
			expiry_ns = ktime_to_ns( 
				hrtimer_get_expires(&ssres->timer) );
			init->init_per_tsks(init, dh, &expiry_ns);
		}
		goto RET;
	}
	if(t->os_task) {
		return t;
	}
	else {
		TRACE("SS_TRACE: ERROR: OS_TASK is NULL\n");
	}
RET:
	return NULL;
}

#endif
/* ======================Linux/ litmus rt specific functions===================*/

void linux_on_job_decn (struct slot_shift *ss)
{
	struct ss_task_struct *tsk = ss->dh->get_curr_task(ss->dh);
	struct ss_queue_job *curr;
	struct task_struct *t;

	if(!tsk) 
		return;

	curr = &tsk->curr_job;
	t = tsk->os_task;
	tsk_rt(t)->job_params.slot_no = curr->job->exec_count;
}

void krnl_side_job_complete(struct slot_shift *ss, struct ss_task_struct *task)
{
	struct task_struct *t = task->os_task;

	tsk_rt(t)->completed = 1;
	prepare_for_next_period(t);
}

struct ss_data_hndl_fn ss_data = {

	.init_list = ss_initialize_list,
	.add_guaranteed_task = add_guaranteed_task,
	.add_unConcluded_task = add_unConcluded_task,
	.add_notGuaranteed_task = add_notGuaranteed_task,
	.add_block_list = add_to_block_list,
	.add_ready_list = add_to_ready_list,
	.get_unConcluded_task = get_unConcluded_task,
	.get_notGuaranteed_task = get_notGuaranteed_task,
	.get_task = get_task,
	.get_curr_task = get_curr_task,
	.update_curr_tsk = update_curr_task,
	.update_tsk_state = update_tsk_state,
	.update_tsk_quantum = update_task_quantum,
	.update_tsk_q_state = update_tsk_q_state,			
	.get_nxt_rdy_task = get_nxt_rdy_task,
	.release_task_with_curr_est = release_task_with_curr_est,
	.update_curr_intr = update_curr_interval,
	.split_interval = ss_split_intr,
	.get_nxt_intr = get_nxt_intr,
	.get_prev_intr = get_prev_intr,
	.add_interval = add_interval,
	.get_intr_with_id = get_intr_with_id,
	.get_curr_intr = get_curr_intr,
	.remove_interval = remove_interval,
};

static struct ss_algo_fn ss_algo_fn = {
	.slot_shift_core = ss_init_core,
	.acceptance_test = ss_acceptance_test,
	.guarantee_algo = ss_guarantee_algorithm_orig,
	.calculate_sc = ss_calculate_sc_orig,
	.update_sc = ss_update_sc,
	.ready_queue_update = ss_ready_queue_update,
};

static struct ss_os_fn ss_linux_fn = {
	.on_job_decn = linux_on_job_decn,
	.krnl_side_job_complete = krnl_side_job_complete,
};

static struct ss_hp_extend ss_hp_extend = {
	.reinsert_past_interval = ss_reinsert_past_interval,
};

lt_t td_next_major_cycle_start(struct table_driven_reservation *tdres);
void td_replenish(struct reservation *res);
void td_drain_budget(struct reservation *res, lt_t how_much);

void rt_job_print(struct rt_job *job)
{
	TRACE ("SS_TRACE: "
		"job %u, rel: %llu, dl: %llu, exec_time: %llu, lateness: %lld\n",
			job->job_no, job->release, job->deadline,
			job->exec_time, job->lateness);
}

static void ss_client_arrives(
	struct reservation* res,
	struct reservation_client *client
)
{
	struct table_driven_reservation *tdres =
		container_of(res, struct table_driven_reservation, res);
	struct task_client *tsk_client = container_of(client,
				struct task_client, client);
	struct slot_shift_reservation *ssres = get_slot_shift_reservation(res);
	struct ss_task_struct *tsk = tsk_rt(tsk_client->task)->slot_shift_task;

	TRACE("SS_TRACE: (%d) CLIENT ARRIVES\n", tsk_client->task->pid);

	if((tsk->type != PERIODIC) && 
	!(tsk_rt(tsk_client->task)->task_params.synced)) {
		struct ss_init_phase *init = ssres->ss.init;
		TRACE("SS_TRACE: initializing aperiodics\n");
		if(init->aper_cnt > 0) {
			init->add_aper_tsk(init, tsk);
			tsk_rt(tsk_client->task)->task_params.synced = 1;
			init->aper_cnt--;
		}
		else {
			TRACE("SS_TRACE: ERROR: APERIODIC COUNT MISMATCH\n");
		}
		return;
	}
	else if(tsk->type != PERIODIC) {

		struct ss_data_hndl_fn *dh = ssres->ss.dh;

		tsk->curr_job.job->est  = ssres->ss.curr_slot + 1;
		update_curr_job(tsk, 
				tsk->curr_job.minor_id, 
				tsk->curr_job.job,
				0);

		dh->update_tsk_state(dh, tsk, JUST_ARRIVED);
	}
	list_add_tail(&client->list, &res->clients);
	tsk_rt(tsk_client->task)->job_params.timer = &ssres->timer;
	tsk_rt(tsk_client->task)->job_params.slot_no = 0;
	switch (res->state) {
		case RESERVATION_INACTIVE:
			TRACE("SS_TRACE: RESERVATION_INACTIVE\n");
			tdres->major_cycle_start = td_next_major_cycle_start(tdres);
			res->next_replenishment  = tdres->major_cycle_start;
			res->next_replenishment += tdres->intervals[0].start;
			tdres->next_interval = 0;
			res->env->change_state(res->env, res,
				RESERVATION_DEPLETED);
			hrtimer_start_range_ns(&ssres->timer,
				ns_to_ktime(res->next_replenishment),
				0 /* timer coalescing slack */,
				HRTIMER_MODE_ABS);
			break;

		case RESERVATION_ACTIVE:
		case RESERVATION_DEPLETED:
			TRACE("SS_TRACE: WE are in RESERVATION_ACTIVE\n");
			break;

		case RESERVATION_ACTIVE_IDLE:
			res->env->change_state(res->env, res,
				RESERVATION_ACTIVE);
			break;
	}

	if (!(tsk_rt(tsk_client->task)->task_params.synced)) {
		
		if(RESERVATION_INACTIVE == res->state || 
		RESERVATION_DEPLETED == res->state ) {
			tsk_rt(tsk_client->task)->sporadic_release_time =
						res->next_replenishment;
			prepare_for_next_period(tsk_client->task);
			TRACE("SS_TRACE: (%d) tsk_client->rel_time: %lld, "
				"res->next_replenishment: %lld\n",
				tsk_client->task->pid,
				tsk_rt(tsk_client->task)->sporadic_release_time,
				res->next_replenishment);
			release_at(tsk_client->task, res->next_replenishment);
		}
		tsk_rt(tsk_client->task)->job_params.job_no = 0;
		rt_job_print(&tsk_rt(tsk_client->task)->job_params);
		tsk_rt(tsk_client->task)->task_params.synced = 1;
	}
}

static void ss_client_departs(
			struct reservation *res,
			struct reservation_client *client,
			int did_signal_job_completion
		)
{
	struct task_client *tsk_client = container_of(client, struct task_client, client);
	struct task_struct *t = tsk_client->task;
	struct slot_shift_reservation *ssres = get_slot_shift_reservation(res);
	struct ss_data_hndl_fn *dh = ssres->ss.dh;
	struct ss_task_struct *curr;
	int job_completed = 0; /* Here, this variable is useless */
	struct ss_task_struct *tsk = tsk_rt(t)->slot_shift_task;
	
	curr = dh->get_curr_task(dh);
	TRACE("SS_TRACE: (%d) CLIENT DEPARTS\n", tsk_client->task->pid);
	if(tsk->type != PERIODIC) {
		TRACE("SS_TRACE: APERIODIC DEPARTS\n");
		return;
	}
	TRACE("SS_TRACE: client departs called on job COMPLETION\n");
	dh->update_tsk_quantum(dh,curr, &job_completed);
	list_del(&client->list);

	switch (res->state) {
		case RESERVATION_INACTIVE:
		case RESERVATION_ACTIVE_IDLE:
		case RESERVATION_DEPLETED:
			BUG();
			break;

		case RESERVATION_ACTIVE:
			/* do nothing */
			break;
	}
}

static void ss_replenish(struct reservation *res)
{
	td_replenish(res);
	res->state = RESERVATION_ACTIVE;
}

static void ss_drain_budget(
		struct reservation *res,
		lt_t how_much)
{
	td_drain_budget(res, how_much);
	res->state = RESERVATION_ACTIVE;
}

static struct reservation_ops ss_ops = {
	.dispatch_client = ss_dispatch_client,
	.client_arrives = ss_client_arrives,
	.client_departs = ss_client_departs,
	.replenish = ss_replenish,
	.drain_budget = ss_drain_budget,
};

void slot_shift_reservation_init(
	struct slot_shift_reservation *ssres,
	lt_t major_cycle,
	struct lt_interval *intervals,
	unsigned int num_intervals,
	lt_t slot_quantum)
{
	struct reservation_ops *td_ops;

	table_driven_reservation_init( &ssres->td_res, major_cycle, intervals,
					num_intervals);
	td_ops = ssres->td_res.res.ops;
	ssres->td_res.res.ops = &ss_ops;
	hrtimer_init(&ssres->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	ssres->timer.function = ss_timer_callback;
	ssres->ss.algo = &ss_algo_fn;
	ssres->ss.dh = &ss_data;
	ssres->ss.os = &ss_linux_fn;
	ssres->ss.hp_ext = &ss_hp_extend;
	ssres->ss.init = &init_aper;
	ssres->ss.dh->init_list(ssres->ss.dh);
	ssres->ss.curr_slot = -1;
	ssres->slot_quantum = slot_quantum * ONE_MS;
	TRACE ("SS_TRACE: slot_shift_reservation_init finished\n");
}

void print_interval(struct ss_intr_struct *i)
{
	printk("Interval %d (%p): core: %d, start: %d, end: %d, sc: %d\n",
		i->intr_struct->intr.intr_id,
		i,
		i->intr_struct->intr.core,
		i->intr_struct->intr.start,
		i->intr_struct->intr.end,
		i->intr_struct->intr.sc);
}
