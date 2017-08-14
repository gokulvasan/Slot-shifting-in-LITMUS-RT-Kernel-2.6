#include <linux/cpu.h>
#include <linux/export.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/notifier.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/seq_file.h>
#include <linux/err.h>
//#include <linux/debugobjects.h>
#include <linux/sched.h>
#include <linux/sched/sysctl.h>
//#include <linux/sched/rt.h>
#include <linux/timer.h>
#include <linux/freezer.h>

#include <asm/uaccess.h>

#include <trace/events/timer.h>

//#include "timekeeping.h"

/*
  * callback function on timer wakeup
  */
static enum hrtimer_restart simulator_hrtimer_wakeup(struct hrtimer *timer)
{
	 struct hrtimer_sleeper *t =
		 container_of(timer, struct hrtimer_sleeper, timer);
	 struct task_struct *task = t->task;

	 t->task = NULL;
	 /* TODO: 1. reinitialize the task params of litmus rt
		  2. inject into ss queue as just arrived.
	*/
	TRACE("Simulator Expired\n");

	 if (task)
		 wake_up_process(task);
	
	 return HRTIMER_NORESTART;
}

void simulator_init_sleeper(struct hrtimer_sleeper *sl, struct task_struct *task)
 {
         sl->timer.function = simulator_hrtimer_wakeup;
         sl->task = task;
 }

/**
  * simulator_hrtimeout_range_clock - sleep until timeout
  * @expires:    timeout value (ktime_t)
  * @delta:      slack in expires timeout (ktime_t)
  * @mode:       timer mode, HRTIMER_MODE_ABS or HRTIMER_MODE_REL
  * @clock:      timer clock, CLOCK_MONOTONIC or CLOCK_REALTIME
  * @task :	 task that needs to be suspended.
  */
int
simulator_hrtimeout_range_clock(ktime_t *expires, unsigned long delta,
                        const enum hrtimer_mode mode, int clock, 
			struct task_struct *task)
{
	struct hrtimer_sleeper t;

	hrtimer_init_on_stack(&t.timer, clock, mode);
	hrtimer_set_expires_range_ns(&t.timer, *expires, delta);

	simulator_init_sleeper(&t, task);

	hrtimer_start_expires(&t.timer, mode);
	if (!hrtimer_active(&t.timer))
		t.task = NULL;

	if (likely(t.task))
		goto RET;

	hrtimer_cancel(&t.timer);
	destroy_hrtimer_on_stack(&t.timer);

	RET:
		return !t.task ? 0 : -EINTR;
}

int apriodic_simulator( unsigned long long time, 
			struct task_struct* tsk ) 
{
	int ret = 0;
	ktime_t expires;
	unsigned long delta;

	if( time <= 0 || NULL == tsk) {
		ret = -EINTR;
		goto RET;
	}

	set_task_state(tsk, TASK_INTERRUPTIBLE);
	
	expires = ns_to_ktime(time);

	ret = simulator_hrtimeout_range_clock(&expires, 0, 
					HRTIMER_MODE_ABS,
					CLOCK_MONOTONIC,
					tsk);
	if(unlikely(ret < 0)) {
		ret = -EINTR;
		goto RET;
	}

	litmus_reschedule_local();

	RET:
		return ret;
}





