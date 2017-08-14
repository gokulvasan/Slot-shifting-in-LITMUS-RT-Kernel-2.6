#include "ss_data.h"

#define __NR_reservation_create 363
#define __NR_reservation_destroy 364
#define __NR_wait_for_ts_release 360
#define __NR_set_task_table_data 365

struct ss_tsk_plce_hlder {
	pid_t pid;
	size_t len;
};

struct res_type {
	int type;
	int id;
	int len;
};

static int reservation_id = 0;

int reservation_create(struct res_type rtype, void *config) {

#ifdef SYS_HOST
	return 0;
#endif

	return syscall(__NR_reservation_create, rtype, config);
}

int reservation_destroy(unsigned int reservation_id, int cpu) {

#ifdef SYS_HOST
	return 0;
#endif
	return syscall(__NR_reservation_destroy, reservation_id, cpu);
}

int wait_for_ts_release(void) {

#ifdef SYS_HOST
	sleep(10);
	return 0;
#endif
	return syscall(__NR_wait_for_ts_release);
}

int ss_inject_task_table(struct ss_tsk_plce_hlder place_holder, struct ss_task *inj) {

#ifdef SYS_HOST
	return 0;
#endif
	return syscall(__NR_set_task_table_data, place_holder, inj);

}

static struct minor_id_bond* get_minor_id(struct ss_task *major_id) {
	
	return (OFFSET(major_id, major_id->minor_id_bonds[0]));
}

void ss_inj_update_res_id(int res_id) {

	reservation_id = res_id;
}

int ss_inj_get_res_id() {

	return reservation_id;
}

#ifdef SYS_HOST
	#define BIN "/bin/ls"
	#define BIN_LEN 8
#else
	#define BIN "/bin/rtspin"
	#define BIN_LEN 12
#endif
#define PARAM_LEN 64

int ss_inj_per_task(struct task *task) {
	
	pid_t pid;
	int ret = -1;
	char *param[9];
	int i;
	struct ss_tsk_plce_hlder tmp;

	do {
		if(!task->bin[0]) {
			struct minor_id_bond *job;
			int period;

			memcpy(task->bin, BIN , BIN_LEN);
			job = get_minor_id(task->ss_tsk);
			/* FIXME: needs check of second job*/
			period = job->intr_bond[0].est - job->intr_bond[1].est;
			param[0] = BIN;
#ifdef DEBUG
			printf("%s:No binary so: %s is copied",
					 __FUNCTION__, param[0]);
			printf("period: %d ", period);
			printf("dl: %d ", job->dl);
			printf("wcet: %d\n", job->wcet);
#endif

#ifdef SYS_HOST
			param[1] = NULL;
#else
			for(i=1; i< ((sizeof(param)/sizeof(param[0])) - 1); i++) {
				param[i] = malloc(PARAM_LEN);
			}
			sprintf(param[1], "%s", "-r");
			sprintf(param[2], "%d", ss_inj_get_res_id());
			sprintf(param[3], "%s", "-p");
			sprintf(param[4], "%d", 0);
			sprintf(param[5], "%d", job->wcet);
			sprintf(param[6], "%d", period);
			sprintf(param[7], "%d", job->dl);
			param[8] = NULL;

#endif
		}
		pid = fork();

		if(0 == pid) {

			wait_for_ts_release();
#ifdef DEBUG
			printf("We are Execing a child: %s\n", param[0]);
#endif	
			execvp(param[0], param);
			fprintf(stderr, "Error: %s:Execing child Failed\n", 
				__FUNCTION__);
			exit(1);
		}
		else {

			if(-1 == pid) {
				fprintf(stderr,
					"Error: %s:fork(Major_id:%d) failed\n", 
					task->ss_tsk->major_id,__FUNCTION__);
				break;
			}
			tmp.pid = pid;
			tmp.len = task->ss_tsk_len;
			
			task->pid = pid;
			if(ss_inject_task_table(tmp, task->ss_tsk) ) {
				fprintf(stderr, 
					"Error: %s: task(%d) table inject fail\n",
					 __FUNCTION__,pid);
				break;
			}
#ifdef DEBUG
			printf("task %d is injected successful\n", pid);
#endif
			ret = 0;
		}
	
	} while(0);

	return ret;
}

int ss_inj_task(struct ss_container *tsks){

	struct ss_container *tmp = tsks;
	struct task *tsk = NULL;
	int ret = 0;
	
	while (tmp) {

		if(!(tsk = tmp->data)) {
			fprintf(stderr, "Error: %s task is NULL\n", __FUNCTION__);
			ret = -1;
			break;			
		}
		if(ss_inj_per_task(tsk)) {
			ret = -1;
			break;
		}
		tmp = tmp->nxt;	
	}
	return ret;
}

int ss_inj_res_create(long res_len,  
		long budget,
		long offset,
		long period,
		long relative_deadline ) {
	
	int ret;
	struct res_type type;
	struct ss_res_config {
		long priority;
		int cpu;
		long period;
		long budget;
		long relative_deadline;
		long offset;
	} config;

	type.id = ss_inj_get_res_id();
	type.type = 3; //slot shifting;
	type.len = sizeof(config);
	
	config.priority = 0; //LITMUS_HIGHEST_PRIORITY;
	config.period = period;
	config.budget = budget;
	config.relative_deadline = relative_deadline;
	config.offset = offset;
	
	ret = reservation_create(type, &config);
	if(ret < 0)
	{	
		perror("Error\n");
		return -1;	
	}
	
	return 0;
}

int ss_inj_interval(int len, const struct ss_intr_inj *inj) {

	struct res_type type;
	int ret;

	if(!inj) {
		perror("ERROR\n");
		return -1;
	}
#ifdef DEBUG
	return 0;
#endif 
	type.type = 3; //SLOT SHIFTING 
	type.id = ss_inj_get_res_id();
	type.len = len;

	ret = reservation_create(type, inj);
	if(ret < 0)
	{
		fprintf(stderr, "Error: %s():failed to inject\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int ss_inj_release_tasks() {

	return 0;
}

