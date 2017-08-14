/*
 * litmus/sched_ss.c
 *.
 */

#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include <litmus/litmus.h>
#include <litmus/wait.h>
#include <litmus/jobs.h>
#include <litmus/preempt.h>
#include <litmus/fp_common.h>
#include <litmus/sched_plugin.h>
#include <litmus/sched_trace.h>
#include <litmus/trace.h>
#include <litmus/budget.h>
#include <litmus/sched_trace.h>

/* to set up domain/cpu mappings */
#include <litmus/litmus_proc.h>
#include <linux/uaccess.h>
#include <litmus/sched_prm.h>
#include <litmus/bheap.h>


struct prm_param_t prm_param; /// this will define per CPU for partitioned.

struct bheap prio_queue;

static int bheap_compare(struct bheap_node *a, struct bheap_node *b) {
	
	struct task_struct *ta = a->value;
	struct task_struct *tb = b->value;

	if(get_priority(ta) < get_priority(tb))
		return 1;
	else 
		return 0;
}


static long prm_activate_plugin(void)
{
	int ret = 0;

	bheap_init(&prio_queue);
	
	return ret;
}

static long prm_deactivate_plugin(void)
{
	/*FIXME: free all bheap nodes */
}

/*	Prepare a task for running in RT mode
 */

static void prm_task_new(struct task_struct * t, int on_rq, int is_scheduled)
{
		struct bheap_node *node;

		if(!is_realtime(t))
			return;

		bheap_add(bheap_compare, &prio_queue, t, GFP_KERNEL);
		if(1 == get_priority(t) ) {
			prm_param.t1 = t;	
		}
		return;

}

static void prm_task_exit(struct task_struct * t)
{
	return;	
}

static void prm_prepare_next_period(struct task_struct* t)
{
	struct task_prm *p = tsk_prm(t);

	prepare_for_next_period(t);
	tsk_rt(t)->completed = 0;
	p->release = get_release(t);

}

static struct task_struct* prm_schedule(struct task_struct * prev)
{
#if 0
	sched_state_task_picked();
	return NULL;
#else
	struct task_struct *i_tsk = NULL;
	struct bheap_node *node;
 	quanta_t curr_time;
	quanta_t i_tsk_exec_time;
	quanta_t hp_tsk_release;
	unsigned int blocks;
	unsigned int completion;

	blocks      = is_realtime(prev) && !is_running(prev);
	completion  = is_realtime(prev) && is_completed(prev);

	if(blocks) {
		TRACE("PRM_TRACE: ERROR: Task cannot block itself\n");
	}

	if(completion) {
		prm_prepare_next_period(prev);
	}
	else {
		if(is_realtime(prev))
			i_tsk = prev;
		else
			i_tsk = NULL;

		goto RET;
	}
	node = bheap_take(bheap_compare, &prio_queue);

	if(node) {
		i_tsk = node->value;
	}
	else {
		TRACE("PRM_TRACE: nxt task is NULL\n");
		goto RET;
	}

	curr_time  = litmus_clock();
	hp_tsk_release = tsk_prm(prm_param.t1);
	i_tsk_exec_time = curr_time + get_exec_time(i_tsk);

	/*precautious on next execution */
	if(
	 (i_tsk_exec_time <= hp_tsk_release) ||
	( (prev == prm_param.t1) && 
	(i_tsk_exec_time <= (hp_tsk_release +  
	get_rt_period(prm_param.t1) - get_rt_period(i_tsk))) 
	)
	) {
		/*
			1. delete the node from the list.
			2. then decide on nxt 
		*/
		node = bheap_take_del(bheap_compare, &prio_queue);
		i_tsk = node->value;
	}
	else {
		i_tsk = NULL;
	}

	RET:
		return i_tsk;
#endif
}

static void prm_task_wake_up(struct task_struct *task)
{
	
}

static void prm_task_block(struct task_struct *t)
{
	if(is_realtime(t)) {
		TRACE("PRM_TRACE: ERROR: task cannot block itself\n");
	}

	return;
}

static long prm_admit_task(struct task_struct* tsk)
{
	if(is_realtime(tsk))
		return 0;

	return -EINVAL;	
}

/*	Plugin object	*/
static struct sched_plugin prm_plugin = {
	.plugin_name		= "PRM",
	//.task_new		= prm_task_new,
	//.complete_job		= complete_job,
	//.task_exit		= prm_task_exit,
	.schedule		= prm_schedule,
	//.task_wake_up		= prm_task_wake_up,
	//.task_block		= prm_task_block,
	.admit_task		= prm_admit_task,
	.activate_plugin	= prm_activate_plugin,
	//.deactivate_plugin	= prm_deactivate_plugin,
};

static int __init init_prm(void)
{
	int i;

	return register_sched_plugin(&prm_plugin);
}

module_init(init_prm);

