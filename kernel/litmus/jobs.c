/* litmus/jobs.c - common job control code
 */

#include <linux/sched.h>

#include <litmus/litmus.h>
#include <litmus/sched_plugin.h>
#include <litmus/jobs.h>

static inline void setup_release(struct task_struct *t, lt_t release)
{
	TRACE("SS_TRACE: DOING A RELEASE! (%lld)\n", litmus_clock());

	/* prepare next release */
	t->rt_param.job_params.release = release;
	t->rt_param.job_params.deadline = release + get_rt_relative_deadline(t);
	t->rt_param.job_params.exec_time = 0;

	/* update job sequence number */
	t->rt_param.job_params.job_no++;
}

void prepare_for_next_period(struct task_struct *t)
{
	BUG_ON(!t);

	TRACE("SS_TRACE: prepare_for_next_period (%lld)\n", litmus_clock());

	/* Record lateness before we set up the next job's
	 * release and deadline. Lateness may be negative.
	 */
	t->rt_param.job_params.lateness =
		(long long)litmus_clock() -
		(long long)t->rt_param.job_params.deadline;

	if (tsk_rt(t)->sporadic_release) {
		TRACE_TASK(t, "SS_TRACE: sporadic release at %llu\n",
			   tsk_rt(t)->sporadic_release_time);
		/* sporadic release */
		setup_release(t, tsk_rt(t)->sporadic_release_time);
		tsk_rt(t)->sporadic_release = 0;
	} else {
		/* periodic release => add period */
		setup_release(t, get_release(t) + get_rt_period(t));
	}
}

void release_at(struct task_struct *t, lt_t start)
{
	BUG_ON(!t);

	TRACE("SS_TRACE: release_at (%lld)\n", litmus_clock());

	setup_release(t, start);
	tsk_rt(t)->completed = 0;
}

long default_wait_for_release_at(lt_t release_time)
{
	struct task_struct *t = current;
	unsigned long flags;

	if(!is_realtime(t))
		return 0;

	local_irq_save(flags);
	tsk_rt(t)->sporadic_release_time = release_time;
	smp_wmb();
	tsk_rt(t)->sporadic_release = 1;
	local_irq_restore(flags);
	TRACE("SS_TRACE: %s: calling complete_job\n", __FUNCTION__);
	return litmus->complete_job();
}


/*
 *	Deactivate current task until the beginning of the next period.
 */
long complete_job(void)
{
	/* Mark that we do not excute anymore */
	tsk_rt(current)->completed = 1;
	/* call schedule, this will return when a new job arrives
	 * it also takes care of preparing for the next release
	 */
	schedule();
	return 0;
}
