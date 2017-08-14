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

/* to set up domain/cpu mappings */
#include <litmus/litmus_proc.h>
#include <linux/uaccess.h>

#include <litmus/ss_reservations.h>

	
static struct domain_proc_info pfp_domain_proc_info;
static long ss_get_domain_proc_info(struct domain_proc_info **ret)
{
	*ret = &pfp_domain_proc_info;
	return 0;
}

#if 0
static void ss_setup_domain_proc(void)
{
	int i, cpu;
	int release_master =
#ifdef CONFIG_RELEASE_MASTER
		atomic_read(&release_master_cpu);
#else
		NO_CPU;
#endif
	int num_rt_cpus = num_online_cpus() - (release_master != NO_CPU);
	struct cd_mapping *cpu_map, *domain_map;

	memset(&pfp_domain_proc_info, sizeof(pfp_domain_proc_info), 0);
	init_domain_proc_info(&pfp_domain_proc_info, num_rt_cpus, num_rt_cpus);
	pfp_domain_proc_info.num_cpus = num_rt_cpus;
	pfp_domain_proc_info.num_domains = num_rt_cpus;
}
#endif
struct slot_shift ss;
struct hrtimer timer;
int in_slot_boundry = 0;

#define SLOT_SIZE 100000000
static enum hrtimer_restart ss_timer_callback(struct hrtimer *timer)
{

	hrtimer_forward_now(timer, ns_to_ktime(SLOT_SIZE));

	TRACE("====== TIMER CALLBACK AT %llu\n", litmus_clock());

	in_slot_boundry = 1;
		
	/* Will this call schedule as soon as this interruption is finished? */
	litmus_reschedule_local();

	return HRTIMER_RESTART;
}

static long ss_activate_plugin(void)
{
	int ret;

	//ss_setup_domain_proc();
	ss.algo = &ss_algo_fn;
	ss.dh = &ss_data;
	
	ret = hrtimer_start_range_ns(&timer,
				ns_to_ktime(litmus_clock()),
				0 /* timer coalescing slack */,
				HRTIMER_MODE_ABS);
	
	timer.function = ss_timer_callback;
	if(ret) {
		TRACE("hrtimer failure\n");
		return -EFAULT;
	}
	TRACE("Finished SS timer creation\n");
	return 0;
}

static long ss_deactivate_plugin(void)
{

#if 0
	int ret;

	destroy_domain_proc_info(
			&pfp_domain_proc_info);	
	hrtimer_cancel(&timer);
	
	if(ret)
		TRACE("Timer was active on deactivation");
	
	return 0;
#endif
}

/*	Prepare a task for running in RT mode
 */
static void ss_task_new(struct task_struct * t, int on_rq, int is_scheduled)
{
	
}

static void ss_task_exit(struct task_struct * t)
{
	
}


static struct task_struct* ss_schedule(struct task_struct * prev)
{
	struct ss_task_struct* t;
	struct task_struct *os_task;
	
#if 0
	if(!in_slot_boundry) {
		/*check if curr task is blocked*/
		/*if yes change our internal state */
		/*return NULL */
		t = ss.dh->get_curr_task(ss.dh);
		os_task = t->os_task;
		if(is_blocked(os_task)){
			ss.dh->update_tsk_state(ss.dh, t, BLOCKED);
		}
		os_task = NULL;
		goto DONE;
	}

	in_slot_boundry = 0;
	t = (struct ss_task_struct*)ss.algo->slot_shift_core(&ss);
	os_task = t->os_task;
	BUG_ON(is_blocked(os_task));


	DONE:
		return os_task;
#endif
	return NULL;
}

static void ss_task_wake_up(struct task_struct *task)
{
	
}

static void ss_task_block(struct task_struct *t)
{
	
}

static long ss_admit_task(struct task_struct* tsk)
{
	
}


/*	Plugin object	*/
static struct sched_plugin ss_plugin __cacheline_aligned_in_smp = {
	.plugin_name		= "SLOT-SHIFT",
	.task_new		= ss_task_new,
	.complete_job		= complete_job,
	.task_exit		= ss_task_exit,
	.schedule		= ss_schedule,
	.task_wake_up		= ss_task_wake_up,
	.task_block		= ss_task_block,
	.admit_task		= ss_admit_task,
	.activate_plugin	= ss_activate_plugin,
	.deactivate_plugin	= ss_deactivate_plugin,
	.get_domain_proc_info	= ss_get_domain_proc_info,
};


static int __init init_ss(void)
{
	int i;

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	return register_sched_plugin(&ss_plugin);
}

module_init(init_ss);

