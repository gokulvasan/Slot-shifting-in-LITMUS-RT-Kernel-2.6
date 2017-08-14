#ifndef __SCHED_PRM
#define __SCHED_PRM
	
#define tsk_prm(tsk) ((tsk)->rt_param.prm)

struct prm_param_t {
	unsigned int cpu_id;
	struct task_struct *t1; /* high priority task */
};

struct task_prm {
	unsigned int  cpu;
	quanta_t release; /* next release time of the task*/
};

#endif
