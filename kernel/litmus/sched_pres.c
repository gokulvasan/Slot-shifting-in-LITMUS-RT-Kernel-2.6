#include <linux/percpu.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <litmus/sched_plugin.h>
#include <litmus/preempt.h>
#include <litmus/debug_trace.h>

#include <litmus/litmus.h>
#include <litmus/jobs.h>
#include <litmus/budget.h>
#include <litmus/litmus_proc.h>

#include <litmus/reservation.h>
#include <litmus/polling_reservations.h>
#include <litmus/ss_reservations.h>

struct pres_task_state {
	struct task_client res_info;
	int cpu;
	bool has_departed;
};

struct pres_cpu_state {
	raw_spinlock_t lock;

	struct sup_reservation_environment sup_env;
	struct hrtimer timer;

	int cpu;
	struct task_struct* scheduled;
};

static DEFINE_PER_CPU(struct pres_cpu_state, pres_cpu_state);

#define cpu_state_for(cpu_id)	(&per_cpu(pres_cpu_state, cpu_id))
#define local_cpu_state()	(&__get_cpu_var(pres_cpu_state))

static struct pres_task_state* get_pres_state(struct task_struct *tsk)
{
	return (struct pres_task_state*) tsk_rt(tsk)->plugin_state;
}

static void task_departs(struct task_struct *tsk, int job_complete)
{
	struct pres_task_state* state = get_pres_state(tsk);
	struct reservation* res;
	struct reservation_client *client;

	res    = state->res_info.client.reservation;
	client = &state->res_info.client;

	res->ops->client_departs(res, client, job_complete);
	state->has_departed = true;
}

static void task_arrives(struct task_struct *tsk)
{
	struct pres_task_state* state = get_pres_state(tsk);
	struct reservation* res;
	struct reservation_client *client;

	res    = state->res_info.client.reservation;
	client = &state->res_info.client;

	state->has_departed = false;
	res->ops->client_arrives(res, client);
}

/* NOTE: drops state->lock */
static void pres_update_timer_and_unlock(struct pres_cpu_state *state)
{
	int local;
	lt_t update, now;

	update = state->sup_env.next_scheduler_update;
	now = state->sup_env.env.current_time;

	/* Be sure we're actually running on the right core,
	 * as pres_update_timer() is also called from pres_task_resume(),
	 * which might be called on any CPU when a thread resumes.
	 */
	local = local_cpu_state() == state;

	/* Must drop state lock before calling into hrtimer_start(), which
	 * may raise a softirq, which in turn may wake ksoftirqd. */
	raw_spin_unlock(&state->lock);

	if (update <= now) {
		litmus_reschedule(state->cpu);
	} else if (likely(local && update != SUP_NO_SCHEDULER_UPDATE)) {
		/* Reprogram only if not already set correctly. */
		if (!hrtimer_active(&state->timer) ||
		    ktime_to_ns(hrtimer_get_expires(&state->timer)) != update) {
			TRACE("canceling timer...\n");
			hrtimer_cancel(&state->timer);
			TRACE("setting scheduler timer for %llu\n", update);
			/* We cannot use hrtimer_start() here because the
			 * wakeup flag must be set to zero. */
			__hrtimer_start_range_ns(&state->timer,
					ns_to_ktime(update),
					0 /* timer coalescing slack */,
					HRTIMER_MODE_ABS_PINNED,
					0 /* wakeup */);
		}
	} else if (unlikely(!local && update != SUP_NO_SCHEDULER_UPDATE)) {
		/* Poke remote core only if timer needs to be set earlier than
		 * it is currently set.
		 */
		TRACE("pres_update_timer for remote CPU %d (update=%llu, "
		      "active:%d, set:%llu)\n",
			state->cpu,
			update,
			hrtimer_active(&state->timer),
			ktime_to_ns(hrtimer_get_expires(&state->timer)));
		if (!hrtimer_active(&state->timer) ||
		    ktime_to_ns(hrtimer_get_expires(&state->timer)) > update) {
			TRACE("poking CPU %d so that it can update its "
			       "scheduling timer (active:%d, set:%llu)\n",
			       state->cpu,
			       hrtimer_active(&state->timer),
			       ktime_to_ns(hrtimer_get_expires(&state->timer)));
			litmus_reschedule(state->cpu);
		}
	}
}

static enum hrtimer_restart on_scheduling_timer(struct hrtimer *timer)
{
	unsigned long flags;
	enum hrtimer_restart restart = HRTIMER_NORESTART;
	struct pres_cpu_state *state;
	lt_t update, now;

	state = container_of(timer, struct pres_cpu_state, timer);

	/* The scheduling timer should only fire on the local CPU, because
	 * otherwise deadlocks via timer_cancel() are possible.
	 * Note: this does not interfere with dedicated interrupt handling, as
	 * even under dedicated interrupt handling scheduling timers for
	 * budget enforcement must occur locally on each CPU.
	 */
	BUG_ON(state->cpu != raw_smp_processor_id());

	raw_spin_lock_irqsave(&state->lock, flags);
	sup_update_time(&state->sup_env, litmus_clock());

	update = state->sup_env.next_scheduler_update;
	now = state->sup_env.env.current_time;

	TRACE_CUR("on_scheduling_timer at %llu, upd:%llu (for cpu=%d)\n",
		now, update, state->cpu);

	if (update <= now) {
		litmus_reschedule_local();
	} else if (update != SUP_NO_SCHEDULER_UPDATE) {
		hrtimer_set_expires(timer, ns_to_ktime(update));
		restart = HRTIMER_RESTART;
	}

	raw_spin_unlock_irqrestore(&state->lock, flags);

	return restart;
}

static struct task_struct* pres_schedule(struct task_struct * prev)
{
	/* next == NULL means "schedule background work". */
	struct pres_cpu_state *state = local_cpu_state();

	raw_spin_lock(&state->lock);

	BUG_ON(state->scheduled && state->scheduled != prev);
	BUG_ON(state->scheduled && !is_realtime(prev));

	/* update time */
	state->sup_env.will_schedule = true;
	sup_update_time(&state->sup_env, litmus_clock());

	/* remove task from reservation if it blocks */
	if (is_realtime(prev) && !is_running(prev))
		task_departs(prev, is_completed(prev));

	/* figure out what to schedule next */
	state->scheduled = sup_dispatch(&state->sup_env);

	/* Notify LITMUS^RT core that we've arrived at a scheduling decision. */
	sched_state_task_picked();

	/* program scheduler timer */
	state->sup_env.will_schedule = false;
	/* NOTE: drops state->lock */
	pres_update_timer_and_unlock(state);

	if (prev != state->scheduled && is_realtime(prev))
		TRACE_TASK(prev, "descheduled.\n");
	if (state->scheduled)
		TRACE_TASK(state->scheduled, "scheduled.\n");

	return state->scheduled;
}

static void resume_legacy_task_model_updates(struct task_struct *tsk)
{
	lt_t now;
	if (is_sporadic(tsk)) {
		/* If this sporadic task was gone for a "long" time and woke up past
		 * its deadline, then give it a new budget by triggering a job
		 * release. This is purely cosmetic and has no effect on the
		 * P-RES scheduler. */

		now = litmus_clock();
		if (is_tardy(tsk, now))
			release_at(tsk, now);
	}
}

/* Called when the state of tsk changes back to TASK_RUNNING.
 * We need to requeue the task.
 */
static void pres_task_resume(struct task_struct  *tsk)
{
	unsigned long flags;
	struct pres_task_state* tinfo = get_pres_state(tsk);
	struct pres_cpu_state *state = cpu_state_for(tinfo->cpu);

	TRACE_TASK(tsk, "thread wakes up at %llu\n", litmus_clock());

	raw_spin_lock_irqsave(&state->lock, flags);
	/* Requeue only if self-suspension was already processed. */
	if (tinfo->has_departed)
	{
		/* Assumption: litmus_clock() is synchronized across cores,
		 * since we might not actually be executing on tinfo->cpu
		 * at the moment. */
		sup_update_time(&state->sup_env, litmus_clock());
		task_arrives(tsk);
		/* NOTE: drops state->lock */
		pres_update_timer_and_unlock(state);
		local_irq_restore(flags);
	} else {
		TRACE_TASK(tsk, "resume event ignored, still scheduled\n");
		raw_spin_unlock_irqrestore(&state->lock, flags);
	}

	resume_legacy_task_model_updates(tsk);
}

/* syscall backend for job completions */
static long pres_complete_job(void)
{
	ktime_t next_release;
	long err;
	
	TRACE_CUR("pres_complete_job at %llu (deadline: %llu)\n", litmus_clock(),
		get_deadline(current));

	tsk_rt(current)->completed = 1;
	prepare_for_next_period(current);
	next_release = ns_to_ktime(get_release(current));
	preempt_disable();
	TRACE_CUR("next_release=%llu\n", get_release(current));
	if (get_release(current) > litmus_clock()) {
		set_current_state(TASK_INTERRUPTIBLE);
		preempt_enable_no_resched();
		err = schedule_hrtimeout(&next_release, HRTIMER_MODE_ABS);
	} else {
		err = 0;
		TRACE_CUR("TARDY: release=%llu now=%llu\n", get_release(current), litmus_clock());
		preempt_enable();
	}

	TRACE_CUR("pres_complete_job returns at %llu\n", litmus_clock());
	return err;
}

static long pres_admit_task(struct task_struct *tsk)
{
	long err = -ESRCH;
	unsigned long flags;
	struct reservation *res;
	struct pres_cpu_state *state;
	struct pres_task_state *tinfo = kzalloc(sizeof(*tinfo), GFP_ATOMIC);

	if (!tinfo)
		return -ENOMEM;

	preempt_disable();

	state = cpu_state_for(task_cpu(tsk));
	raw_spin_lock_irqsave(&state->lock, flags);

	res = sup_find_by_id(&state->sup_env, tsk_rt(tsk)->task_params.cpu);

	/* found the appropriate reservation (or vCPU) */
	if (res) {
		task_client_init(&tinfo->res_info, tsk, res);
		tinfo->cpu = task_cpu(tsk);
		tinfo->has_departed = true;
		tsk_rt(tsk)->plugin_state = tinfo;
		err = 0;

		/* disable LITMUS^RT's per-thread budget enforcement */
		tsk_rt(tsk)->task_params.budget_policy = NO_ENFORCEMENT;
	}

	raw_spin_unlock_irqrestore(&state->lock, flags);

	preempt_enable();

	if (err)
		kfree(tinfo);

	return err;
}

static void task_new_legacy_task_model_updates(struct task_struct *tsk)
{
	lt_t now = litmus_clock();

	/* the first job exists starting as of right now */
	release_at(tsk, now);
}

static void pres_task_new(struct task_struct *tsk, int on_runqueue,
			  int is_running)
{
	unsigned long flags;
	struct pres_task_state* tinfo = get_pres_state(tsk);
	struct pres_cpu_state *state = cpu_state_for(tinfo->cpu);

	TRACE_TASK(tsk, "new RT task start: %llu, litmus_clock: %llu (on_rq:%d, running:%d)\n",
		tsk_rt(tsk)->sporadic_release_time,
		litmus_clock(), on_runqueue, is_running );
	/* acquire the lock protecting the state and disable interrupts */
	raw_spin_lock_irqsave(&state->lock, flags);

	if (is_running) {
		state->scheduled = tsk;
		/* make sure this task should actually be running */
		litmus_reschedule_local();
	}

	if (on_runqueue || is_running) {
		/* Assumption: litmus_clock() is synchronized across cores
		 * [see comment in pres_task_resume()] */
		sup_update_time(&state->sup_env, litmus_clock());
		task_arrives(tsk);
		/* NOTE: drops state->lock */
		pres_update_timer_and_unlock(state);
		local_irq_restore(flags);
	} else
		raw_spin_unlock_irqrestore(&state->lock, flags);

	//task_new_legacy_task_model_updates(tsk);
}

static void pres_task_exit(struct task_struct *tsk)
{
	unsigned long flags;
	struct pres_task_state* tinfo = get_pres_state(tsk);
	struct pres_cpu_state *state = cpu_state_for(tinfo->cpu);

	raw_spin_lock_irqsave(&state->lock, flags);

	if (state->scheduled == tsk)
		state->scheduled = NULL;

	/* remove from queues */
	if (is_running(tsk)) {
		/* Assumption: litmus_clock() is synchronized across cores
		 * [see comment in pres_task_resume()] */
		sup_update_time(&state->sup_env, litmus_clock());
		task_departs(tsk, 0);
		/* NOTE: drops state->lock */
		pres_update_timer_and_unlock(state);
		local_irq_restore(flags);
	} else
		raw_spin_unlock_irqrestore(&state->lock, flags);

	kfree(tsk_rt(tsk)->plugin_state);
	tsk_rt(tsk)->plugin_state = NULL;
}

static long create_polling_reservation(
	int res_type,
	struct reservation_config *config)
{
	struct pres_cpu_state *state;
	struct reservation* res;
	struct polling_reservation *pres;
	unsigned long flags;
	int use_edf  = config->priority == LITMUS_NO_PRIORITY;
	int periodic =  res_type == PERIODIC_POLLING;
	long err = -EINVAL;

	if (config->polling_params.budget >
	    config->polling_params.period) {
		printk(KERN_ERR "invalid polling reservation (%u): "
		       "budget > period\n", config->id);
		return -EINVAL;
	}
	if (config->polling_params.budget >
	    config->polling_params.relative_deadline
	    && config->polling_params.relative_deadline) {
		printk(KERN_ERR "invalid polling reservation (%u): "
		       "budget > deadline\n", config->id);
		return -EINVAL;
	}
	if (config->polling_params.offset >
	    config->polling_params.period) {
		printk(KERN_ERR "invalid polling reservation (%u): "
		       "offset > period\n", config->id);
		return -EINVAL;
	}

	/* Allocate before we grab a spin lock.
	 * Todo: would be nice to use a core-local allocation.
	 */
	pres = kzalloc(sizeof(*pres), GFP_KERNEL);
	if (!pres)
		return -ENOMEM;

	state = cpu_state_for(config->cpu);
	raw_spin_lock_irqsave(&state->lock, flags);

	res = sup_find_by_id(&state->sup_env, config->id);
	if (!res) {
		polling_reservation_init(pres, use_edf, periodic,
			config->polling_params.budget,
			config->polling_params.period,
			config->polling_params.relative_deadline,
			config->polling_params.offset);
		pres->res.id = config->id;
		if (!use_edf)
			pres->res.priority = config->priority;
		sup_add_new_reservation(&state->sup_env, &pres->res);
		err = config->id;
	} else {
		err = -EEXIST;
	}

	raw_spin_unlock_irqrestore(&state->lock, flags);

	if (err < 0)
		kfree(pres);

	return err;
}

#define MAX_INTERVALS 1024


static long create_table_driven_reservation(
	struct reservation_config *config)
{
	struct pres_cpu_state *state;
	struct reservation* res;
	struct table_driven_reservation *td_res = NULL;
	struct lt_interval *slots = NULL;
	size_t slots_size;
	unsigned int i, num_slots;
	unsigned long flags;
	long err = -EINVAL;


	if (!config->table_driven_params.num_intervals) {
		printk(KERN_ERR "invalid table-driven reservation (%u): "
		       "no intervals\n", config->id);
		return -EINVAL;
	}

	if (config->table_driven_params.num_intervals > MAX_INTERVALS) {
		printk(KERN_ERR "invalid table-driven reservation (%u): "
		       "too many intervals (max: %d)\n", config->id, MAX_INTERVALS);
		return -EINVAL;
	}

	num_slots = config->table_driven_params.num_intervals;
	slots_size = sizeof(slots[0]) * num_slots;
	slots = kzalloc(slots_size, GFP_KERNEL);
	if (!slots)
		return -ENOMEM;

	td_res = kzalloc(sizeof(*td_res), GFP_KERNEL);
	if (!td_res)
		err = -ENOMEM;
	else
		err = copy_from_user(slots,
			config->table_driven_params.intervals, slots_size);

	if (!err) {
		/* sanity checks */
		for (i = 0; !err && i < num_slots; i++)
			if (slots[i].end <= slots[i].start) {
				printk(KERN_ERR
				       "invalid table-driven reservation (%u): "
				       "invalid interval %u => [%llu, %llu]\n",
				       config->id, i,
				       slots[i].start, slots[i].end);
				err = -EINVAL;
			}

		for (i = 0; !err && i + 1 < num_slots; i++)
			if (slots[i + 1].start <= slots[i].end) {
				printk(KERN_ERR
				       "invalid table-driven reservation (%u): "
				       "overlapping intervals %u, %u\n",
				       config->id, i, i + 1);
				err = -EINVAL;
			}

		if (slots[num_slots - 1].end >
			config->table_driven_params.major_cycle_length) {
			printk(KERN_ERR
				"invalid table-driven reservation (%u): last "
				"interval ends past major cycle %llu > %llu\n",
				config->id,
				slots[num_slots - 1].end,
				config->table_driven_params.major_cycle_length);
			err = -EINVAL;
		}
	}

	if (!err) {
		state = cpu_state_for(config->cpu);
		raw_spin_lock_irqsave(&state->lock, flags);

		res = sup_find_by_id(&state->sup_env, config->id);
		if (!res) {
			table_driven_reservation_init(td_res,
				config->table_driven_params.major_cycle_length,
				slots, num_slots);
			td_res->res.id = config->id;
			td_res->res.priority = config->priority;
			sup_add_new_reservation(&state->sup_env, &td_res->res);
			err = config->id;
		} else {
			err = -EEXIST;
		}

		raw_spin_unlock_irqrestore(&state->lock, flags);
	}

	if (err < 0) {
		kfree(slots);
		kfree(td_res);
	}

	return err;
}

struct ss_res_config {
	long priority;
	int cpu;
	lt_t slot_quantum;
	lt_t cycle_length;
} config;

void print_reservation(struct sup_reservation_environment *sup_env)
{
	struct list_head *pos;
	struct reservation *res;
	struct slot_shift_reservation *slotshift_res;

	list_for_each(pos, &sup_env->inactive_reservations) {
		res = list_entry(pos, struct reservation, list);

		slotshift_res = get_slot_shift_reservation(res);
		printk("in_slot_boundary: %d\n"
			"ss: %p\n"
			"ss->algo: %p\n"
			"ss->algo->slot_shift_core: %p\n"
			"td_res->major_cycle: %lld\n"
			"td_res->next_interval: %x\n"
			"td_res->num_intervals: %d\n"
			"td_res->intervals->start: %lld\n"
			"td_res->intervals->end: %lld\n"
			"id: %d, priority: %lld\n",
				slotshift_res->in_slot_boundary,
				&slotshift_res->ss,
				slotshift_res->ss.algo,
				slotshift_res->ss.algo->slot_shift_core,
				slotshift_res->td_res.major_cycle,
				slotshift_res->td_res.next_interval,
				slotshift_res->td_res.num_intervals,
				slotshift_res->td_res.intervals->start,
				slotshift_res->td_res.intervals->end,
				slotshift_res->td_res.res.id,
				slotshift_res->td_res.res.priority);
	}
}

static long create_slot_shifting_reservation(
	struct res_data *type,
	void *data)
{
	struct ss_res_config *config = data;
	struct pres_cpu_state *state;
	struct reservation* res;
	struct slot_shift_reservation *new = NULL;
	struct lt_interval *slots = NULL;
	size_t slots_size;
	unsigned int num_slots;
	unsigned long flags;
	long err = -EINVAL;
	lt_t slot_quantum;

	printk("Starting %s\n", __FUNCTION__);
	state = cpu_state_for(config->cpu);

	raw_spin_lock_irqsave(&state->lock, flags);
	res = sup_find_by_id(&state->sup_env, type->id);
	raw_spin_unlock_irqrestore(&state->lock, flags);

	if (!res) {
		printk("%s: will start slot_shift_reservation_init\n",
			__FUNCTION__);

		/* Create only one interval the size of the Hyper Period */
		num_slots = 1;
		slots_size = sizeof(slots[0]) * num_slots;
		slots = kzalloc(slots_size, GFP_KERNEL);
		if (!slots) {
			err = -ENOMEM;
			goto out;
		}

		new = kzalloc(sizeof(*new), GFP_KERNEL);
		if (!new) {
			err = -ENOMEM;
			goto out_slots;
		} else {
			slots->start = 0;
			slots->end = 7999999; //config->cycle_length-1;
			err = 0;
		}

		slot_quantum = config->slot_quantum;

		raw_spin_lock_irqsave(&state->lock, flags);

		slot_shift_reservation_init(new,
			8000000, //config->cycle_length,
			slots, num_slots, slot_quantum);

		new->td_res.res.id = type->id;
		new->td_res.res.priority = config->priority;
		sup_add_new_reservation(&state->sup_env,
				&(new->td_res.res));
		err = type->id;

		raw_spin_unlock_irqrestore(&state->lock, flags);
	} else {
		err = -EEXIST;
	}

	/* DEBUG */
	//print_reservation(&state->sup_env);
	//printk("slotshift_core: %p\n", slotshift_core);

	return err;

out_slots:	kfree(slots);
out:
	printk("Ending %s WITH ERROR\n", __FUNCTION__);
	return err;
}

static long inject_interval(struct res_data *type, void *data)
{
	//struct slotshift_res_config *config = data;
	struct pres_cpu_state *state;
	struct reservation* res;
	unsigned long flags;
	long err = -EINVAL;

	printk("Starting %s\n", __FUNCTION__);

	state = cpu_state_for(0);

	raw_spin_lock_irqsave(&state->lock, flags);
	res = sup_find_by_id(&state->sup_env, type->id);
	raw_spin_unlock_irqrestore(&state->lock, flags);

	if (res) {
		struct ss_intr_struct *i;
		struct slot_shift_reservation *ssres;

		printk("%s: Will create new interval\n", __FUNCTION__);

		/* Check the type of reservation */
		// (actually, no check is going to be made now)
		//res->ss.dh->add_interval(res->ss.dh, data);
		ssres = get_slot_shift_reservation(res);
		if(ssres) {
			ssres->ss.dh->add_interval(ssres->ss.dh, data);
			//slotshift_data.add_interval(&slotshift_data, data);
		}
		err = 0;

		list_for_each_entry(i, &ss_data.interval_list, list)
			print_interval(i);
	}

	return err;
}

static long pres_reservation_create(struct res_data __user *res_type,
					void __user *_config)
{
	long ret = -EINVAL;
	void *config;
	struct res_data type;

	TRACE("Attempt to create reservation (%d)\n", res_type);

	if(copy_from_user(&type, res_type, sizeof(type))) {
		printk(KERN_ERR " invalid res_data\n");
		return -EFAULT;
	}

	config = kmalloc(type.param_len, GFP_KERNEL);

	if (copy_from_user(config, _config, type.param_len))
		return -EFAULT;

	/*
	if (config.cpu < 0 || !cpu_online(config.cpu)) {
		printk(KERN_ERR "invalid polling reservation (%u): "
				"CPU %d offline\n", config.id, config.cpu);
		return -EINVAL;
	}
	*/

	switch (type.type) {
		case PERIODIC_POLLING:
		case SPORADIC_POLLING:
			ret = create_polling_reservation(type.type, config);
			break;
		case TABLE_DRIVEN:
			ret = create_table_driven_reservation(config);
			break;
		case SLOT_SHIFTING:
			ret = create_slot_shifting_reservation(&type, config);
			break;
		default:
			return -EINVAL;
	};

	return ret;
}

static long slotshift_slot_shift_add_interval(struct res_data __user *res_type,
					void __user *_config)
{
	long ret = -EINVAL;
	void *config;
	struct res_data type;

	TRACE("Attempt to add new interval\n");

	if(copy_from_user(&type, res_type, sizeof(type))) {
		printk(KERN_ERR " invalid res_data\n");
		return -EFAULT;
	}

	config = kmalloc(type.param_len, GFP_KERNEL);

	if (copy_from_user(config, _config, type.param_len))
		return -EFAULT;

	switch (type.type) {
		case SLOT_SHIFTING:
			ret = inject_interval(&type, config);
			break;
		default:
			return -EINVAL;
	};

	return ret;
}

static long slotshift_slot_shift_aper_count(int res_id, unsigned int count) 
{

	struct pres_cpu_state *state;
	struct reservation* res;
	struct slot_shift_reservation *ssres;
	unsigned long flags;
	int ret = 0;

	do {
		if(!count) {
			ret = -EINVAL;
			break;
		}

		// temp value;
		state = cpu_state_for(0);

		raw_spin_lock_irqsave(&state->lock, flags);
		res = sup_find_by_id(&state->sup_env, res_id);
		raw_spin_unlock_irqrestore(&state->lock, flags);

		if(!res) {
			return -EFAULT;
			break;
		}

		ssres = get_slot_shift_reservation(res);
		ssres->ss.init->set_aper_cnt(ssres->ss.init, count);

	} while(0);

	return ret;
}
static struct domain_proc_info pres_domain_proc_info;

static long pres_get_domain_proc_info(struct domain_proc_info **ret)
{
	*ret = &pres_domain_proc_info;
	return 0;
}

static void pres_setup_domain_proc(void)
{
	int i, cpu;
	int num_rt_cpus = num_online_cpus();

	struct cd_mapping *cpu_map, *domain_map;

	memset(&pres_domain_proc_info, sizeof(pres_domain_proc_info), 0);
	init_domain_proc_info(&pres_domain_proc_info, num_rt_cpus, num_rt_cpus);
	pres_domain_proc_info.num_cpus = num_rt_cpus;
	pres_domain_proc_info.num_domains = num_rt_cpus;

	i = 0;
	for_each_online_cpu(cpu) {
		cpu_map = &pres_domain_proc_info.cpu_to_domains[i];
		domain_map = &pres_domain_proc_info.domain_to_cpus[i];

		cpu_map->id = cpu;
		domain_map->id = i;
		cpumask_set_cpu(i, cpu_map->mask);
		cpumask_set_cpu(cpu, domain_map->mask);
		++i;
	}
}

static long pres_activate_plugin(void)
{
	int cpu;
	struct pres_cpu_state *state;

	for_each_online_cpu(cpu) {
		TRACE("Initializing CPU%d...\n", cpu);

		state = cpu_state_for(cpu);

		raw_spin_lock_init(&state->lock);
		state->cpu = cpu;
		state->scheduled = NULL;

		sup_init(&state->sup_env);

		hrtimer_init(&state->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_PINNED);
		state->timer.function = on_scheduling_timer;
	}

	pres_setup_domain_proc();

	return 0;
}

static long pres_deactivate_plugin(void)
{
	int cpu;
	struct pres_cpu_state *state;
	struct reservation *res;

	for_each_online_cpu(cpu) {
		state = cpu_state_for(cpu);
		raw_spin_lock(&state->lock);

		hrtimer_cancel(&state->timer);

		/* Delete all reservations --- assumes struct reservation
		 * is prefix of containing struct. */

		while (!list_empty(&state->sup_env.active_reservations)) {
			res = list_first_entry(
				&state->sup_env.active_reservations,
			        struct reservation, list);
			list_del(&res->list);
			kfree(res);
		}

		while (!list_empty(&state->sup_env.inactive_reservations)) {
			res = list_first_entry(
				&state->sup_env.inactive_reservations,
			        struct reservation, list);
			list_del(&res->list);
			kfree(res);
		}

		while (!list_empty(&state->sup_env.depleted_reservations)) {
			res = list_first_entry(
				&state->sup_env.depleted_reservations,
			        struct reservation, list);
			list_del(&res->list);
			kfree(res);
		}

		raw_spin_unlock(&state->lock);
	}

	destroy_domain_proc_info(&pres_domain_proc_info);
	return 0;
}

static struct sched_plugin pres_plugin = {
	.plugin_name		= "P-RES",
	.schedule		= pres_schedule,
	.task_wake_up		= pres_task_resume,
	.admit_task		= pres_admit_task,
	.task_new		= pres_task_new,
	.task_exit		= pres_task_exit,
	.complete_job           = pres_complete_job,
	.get_domain_proc_info   = pres_get_domain_proc_info,
	.activate_plugin	= pres_activate_plugin,
	.deactivate_plugin      = pres_deactivate_plugin,
	.reservation_create     = pres_reservation_create,
	.slot_shift_add_interval = slotshift_slot_shift_add_interval,
	.slot_shift_aper_count = slotshift_slot_shift_aper_count,	
};

static int __init init_pres(void)
{
	return register_sched_plugin(&pres_plugin);
}

module_init(init_pres);

