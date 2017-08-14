#include "ss_data.h"

/*
 * This File is named after nav'i word Tsaheylu means "BONDING"
 * purpose of this file is to create a bonding between disassociated intervals, jobs and Tasks.
 */

/*
 * 1. associate ss_job-s to ss_task
 * 2. create a API that would fork, Exec and send apporpriate data  *	to kernel space
 * 3. create a API that would bound ss_task to ss_interval
 * 4. create a API that would create pres and inject intervals 
 *	it.  
 */
#ifdef DEBUG
static void display_per_task(struct task *t) {
	
	int count = 0;
	int intr = 0;
	if(!t) {
		fprintf(stderr, "Error: %s: NULL Param\n", __FUNCTION__);
		return;
	}

	printf("\n\n|============================non inject part============================\n");
	printf("|pid: %d\n", t->pid);
	printf("|Minor_id_count: %d|\n",t->period);
	printf("|Binary:  %s\n", t->bin);
	printf("|******************************inject part*******************************\n");
	printf("|\tss_tsk addr: %p\n", t->ss_tsk);
	printf("|\tmajor_id: %d\n",t->ss_tsk->major_id);
	printf("|\tminor_id_count: %d\n", t->ss_tsk->minor_id_count);
	printf("|-------------------------------minor bond-------------------------------\n");
	while(count < t->ss_tsk->minor_id_count) {
		int temp1 = ((int)t->ss_tsk) + t->ss_tsk->minor_id_bonds[count];
		struct minor_id_bond *mid = (struct minor_id_bond*)temp1;
		printf("|\t\tminor id [%d]@ offset: %d(addr:%p)\n", 
				count, t->ss_tsk->minor_id_bonds[count], mid);
		printf("|\t\t\tminor_id: %d\n", mid->minor_id);
		printf("|\t\t\twcet: %d\n", mid->wcet);
		printf("|\t\t\tdl: %d\n", mid->dl);
		printf("|\t\t\tcoreid: %d\n", mid->coreid);
		printf("|\t\t\tintr_count: %d\n", mid->intr_count);
		intr = 0;
		printf("|--------------------------------Job bond--------------------------------\n");
		while(intr < mid->intr_count){
			printf("|\t\t\tintr_bond[%d]:\n", intr);
			printf("|\t\t\t\tintr_id: %d\n", 
					mid->intr_bond[intr].intr_id);	
			printf("|\t\t\t\test: %d\n", 
					mid->intr_bond[intr].est);
			printf("|\t\t\t\texec_count: %d\n", 
					mid->intr_bond[intr].exec_count);
			intr++;
		}
		printf("|---------------------------Job bond end---------------------------------\n");

		count++;
	}
	printf("|-------------------------minor bond end---------------------------------\n");
	printf("|\n|\t\t\t\t**inject part END***\n");
	printf("|\n|\t\t\t\t=====paket END======\n");

	return;
}

void task_display(struct ss_container *t) {

	struct ss_container *temp = t;
	struct task *tsk = NULL;

	while(temp) {
		tsk = (struct task*)temp->data;
		display_per_task(tsk);			
		temp = temp->nxt;
	}

}

#endif

static struct ss_container* pluck_out_jobs(unsigned int *major_id,struct ss_container **jobs) {

	struct ss_container *t, *head;
	unsigned int i = 0;
	struct ss_job *j = (*jobs)->data;
	t = head = NULL;

	if(!jobs) {
		fprintf(stderr, "Error: %s null param\n", __FUNCTION__);
		return NULL;
	}
	while(*major_id == j->major_id && j && *jobs) {

		t = *jobs;
		*jobs = (*jobs)->nxt;
		t->nxt = head;
		head = t;
		if(*jobs) {
			j = (*jobs)->data;
		}
		i++;
	}
	*major_id = i;

	return head;
}

static int job_intr_count (struct ss_container *jobs) {

	struct ss_container *tr = jobs;
	struct ss_container *t;
	struct ss_job *j;
	int i = 0;

	while (tr) {

		j =(struct ss_job*)tr->data;
		i += j->intr_count;
		tr = tr->nxt;			
	}
	return i;
}

#define SS_TASK_OFFSET(job_count) (sizeof(struct ss_task) \
		+ (sizeof(int) * (job_count -1)) )

static int create_ss_task(struct task *t_task, 
		struct ss_container *tlist, 
		int job_count) {

	struct ss_container *tmp = tlist;
	struct ss_job *job =(struct ss_job*)(tlist->data);

	memcpy(t_task->bin, job->binary, PATH_MAX);
	t_task->pid = 0;
	t_task->period = 0; //FIXME
	t_task->ss_tsk->major_id = job->major_id;
	if(!(t_task->ss_tsk->minor_id_count = job_count))
	{
		fprintf(stderr,"Error: %s: minor id count is zero\n",__FUNCTION__);
		return 0;
	}

#ifdef DEBUG
	printf("%s: task offset: %ld\n",
		 __FUNCTION__, SS_TASK_OFFSET(job_count));
#endif
	return SS_TASK_OFFSET(job_count);
}


static int create_minor_id_bond( struct ss_task *t_task, 
		struct ss_container *tlist,
		int offset ) {

	struct minor_id_bond *t;
	struct ss_job *j = (struct ss_job*)tlist->data;
	int i = 0;
	struct ss_container *intr_t;
	struct ss_intr_bond *intr;
	int temp = (((int)t_task) + offset);

	t = (struct minor_id_bond *)(temp);
#ifdef DEBUG
	printf("task addr: %p minor_id[%d]->offset:%d(%p)\n", 
			t_task, j->jid, offset, t );
#endif
	t->minor_id = j->jid;
	t->est = j->est;
	t->wcet = j->wcet;
	t->dl = j->dl;
	t->coreid = j->coreid;
	t->intr_count = j->intr_count;
#ifdef DEBUG
	printf("intr_count:%d\n",t->intr_count);
#endif
	while(j->intr) {

		struct ss_container *intr_t = j->intr;
		struct ss_intr_bond *intr = intr_t->data;
		t->intr_bond[i].intr_id = intr->intr_id;
		t->intr_bond[i].est = intr->est;
		t->intr_bond[i].exec_count = 0;
#ifdef DEBUG
		printf("intr_id%d:est%d\n", 
			t->intr_bond[i].intr_id, t->intr_bond[i].est);
#endif
		j->intr = j->intr->nxt;
		free(intr_t->data);
		free(intr_t);

		i++;
	}

	if(i != t->intr_count) {
		fprintf(stderr,"ERROR: %s: interval count error:%d[exp: %d]\n",
				__FUNCTION__, i, t->intr_count);
		return 0;
	}
#ifdef DEBUG
	printf("%s: minor_id[%d]->ends:%ld\n", 
			__FUNCTION__,j->jid, (offset + (sizeof(struct minor_id_bond) + 
					(sizeof(struct job_bond) * (i - 1)))) );
#endif
	free(j);
	free(tlist);

	return (offset + (sizeof(struct minor_id_bond) + 
				(sizeof(struct job_bond) * (i - 1)) ) );

}

#define TASK_LENGTH(job_count, intr_count) ( 				\
		sizeof(struct ss_task) + 				\
		((sizeof(struct minor_id_bond)) +			\
		( sizeof(struct minor_id_bond) * (major_id -1) ) ) +	\
		(sizeof(struct job_bond) * (intr_count -1 ) ) 		\
		)

static int create_task( struct task *t_task, struct ss_container *tlist, 
		int job_count, int intr_count ) {

	int count = 0;
	int offset = 0;
	int ret = -1;
	struct ss_task *t = t_task->ss_tsk;
	

	do {
		if(!t_task || !tlist || 0 == job_count || 0 == intr_count || !t) {
			fprintf(stderr,
			"Error: %s : wrong prams: t_task(%p), tlist(%p), job(%d), intr_count(%d), ss_tsk(%p)\n",
		 				__FUNCTION__, 
						t_task, tlist, job_count, 
						intr_count, t);
			break;
		}
		
		/* first populate non-inject part */
		if(!(offset = create_ss_task(t_task, tlist, job_count)) ) {

			fprintf(stderr,"Error: %s: Failed to create ss_task\n",
								 __FUNCTION__);
			break;
		}
#ifdef DEBUG
		printf("ss_task addr: %p\n",t);
#endif
		while (tlist) {
			struct ss_container *tmp = tlist;
			tlist = tlist->nxt;
			t->minor_id_bonds[count] = offset;
#ifdef DEBUG
			printf("%s: job absolute task: %p, count:%d, offset: %d\n",
						 __FUNCTION__, t, count, offset);
#endif
			if(!(offset = create_minor_id_bond(t, tmp, offset)) ) {
				/*TODO: Cleanup here. */
				fprintf(stderr,"Error: %s: Failure @ minor count:%d bonding\n",
						__FUNCTION__, count);
				break;
			}
			count++;
		}
		if(count != t->minor_id_count) {
			fprintf(stderr,"Error: %s: minor count conflict: %d(Expected: %d)\n", 
					__FUNCTION__, count, t->minor_id_count);
			break;
		}
		ret = 0;

	} while(0);

	return ret;
}

int ss_job_to_task(struct ss_container **jobs) {

	struct ss_container *tlist = NULL; // to hold the list of jobs that belongs to one task.
	struct ss_container *tasks = NULL; // list of tasks of type ss_task.
	struct ss_container *t = NULL; // temporary place holders.
	struct task *t_task;
	struct ss_job *j;
	unsigned int major_id = 0xFFFF;
	unsigned int intr_count = 0;
	int ret = 0;

	while(*jobs) {	
		
		j = (*jobs)->data;
		major_id = j->major_id;
#ifdef DEBUG		
		printf("major_id: %d has", major_id);
#endif
		if(!(tlist = pluck_out_jobs(&major_id, jobs)) ) {
			fprintf(stderr,"Error: %s(): empty jobs in major_id:%d\n",
					__FUNCTION__, major_id);
			ret = -1;
			break;	
		}
		
		intr_count = job_intr_count(tlist);
#ifdef DEBUG
		printf(" jobs %d, intr_count:%d so", major_id, intr_count);
#endif
		t_task = (struct task*)malloc(sizeof(struct task));
		if(!t_task) {
			fprintf(stderr,"Error: %s: in malloc of Task\n",
							__FUNCTION__);
			ret = -1;
			break;	
		}
		memset(t_task, 0x00, sizeof(struct task));
#ifdef DEBUG
		printf(" task_length: %ld\n", TASK_LENGTH(major_id, intr_count));
#endif
		t_task->ss_tsk = (struct ss_task*)malloc(TASK_LENGTH(major_id, intr_count) ); 
		if(!t_task->ss_tsk){

			fprintf(stderr,"Error: %s: ss_tsk malloc fail len:%ld\n", 
			__FUNCTION__, TASK_LENGTH(major_id, intr_count));

			ret = -1;
			break;	
		}
		memset(t_task->ss_tsk, 0x00, TASK_LENGTH(major_id, intr_count));
		t_task->ss_tsk_len = TASK_LENGTH(major_id, intr_count);

		if( create_task(t_task, tlist, major_id, intr_count) )
		{
			fprintf(stderr, "Error: %s: creation of task failed\n",
				__FUNCTION__);
			ret = -1;
			break;
		}
		if( ss_inj_per_task(t_task) ) {
			ret = -1;
			break;
		}
#ifdef DEBUG && !SYS_HOST
		display_per_task(t_task);		
#endif
		t = malloc(sizeof(struct ss_container));
		if(!t) {
			// TODO: cleanup here.
			fprintf(stderr,"Error: %s: malloc Fail ss_container\n", 
					__FUNCTION__);
			ret = -1;
			break;	
		}
		t->data = t_task;
		t->nxt = tasks;
		tasks = t;						
		t_task = NULL;
	}

	if(ret) {
		fprintf(stderr, "NOTE: NO FALLBACK MECHANISM IMPLEMENTED\n");	
	}
	else {
		*jobs = tasks;
#ifdef DEBUG && SYS_HOST
		task_display(*jobs);	
#endif		
	}
	return ret;
}

/****************************************INTERVAL BOND***************************/
#ifdef DEBUG
void intr_display(const struct ss_intr_inj *intr) {
	
	int i = 0;

	if(!intr){
		perror("Error: NULL Param\n");
		return;
	}
	printf("|****************INTERVAL START*******************\n");
	printf("|\tInterval ID: %d\n",intr->intr.intr_id);
	printf("|\tCore: %d\n",intr->intr.core);
	printf("|\tStart: %d\n", intr->intr.start);
	printf("|\tend: %d\n", intr->intr.end);
	printf("|\tsc: %d\n", intr->intr.sc);
	printf("|\tno_of_task: %d\n", intr->intr.no_of_tsk);
	while(i < intr->intr.no_of_tsk) {
		printf("|\t\ttasks[%d] is:\n", i);
		printf("|\t\t\tMajor_id:%d, Minor_id:%d, PID:%d\n",
		intr->tsks[i].major_id, intr->tsks[i].minor_id, intr->tsks[i].pid);
		i++;
	}
	return;
}
#endif

static int intr_srch_job(const unsigned int intr_id,
		const struct minor_id_bond const *job) {
	
	int intr_cnt = 0;
	
	while(intr_cnt < job->intr_count) {
		
		if(intr_id == job->intr_bond[intr_cnt].intr_id)
			return 1;
		intr_cnt++;
	}

	return 0;
}


/*
  One task might have N different kind of jobs hence causing comlpication.
*/
static int intr_popl_per_task ( struct ss_intr_task *intr_tsk,
			const unsigned int intr_id,
			const struct task* const tsk) {

	int cnt = 0;
	int ptr;
	int j_cnt = 0;
	struct ss_task *inj_part = tsk->ss_tsk;
	struct minor_id_bond *job;

	while(j_cnt < inj_part->minor_id_count) {

		ptr = OFFSET(inj_part, inj_part->minor_id_bonds[j_cnt]);
		job = (struct minor_id_bond*)(ptr);

		if(intr_srch_job(intr_id, job) ) {

			intr_tsk[cnt].pid = tsk->pid;
			intr_tsk[cnt].major_id = tsk->ss_tsk->major_id;
			intr_tsk[cnt].minor_id = job->minor_id;
			cnt++;

		}
		j_cnt++;
	}
	
	return cnt;
}

static int intr_populate_task(	struct ss_intr_task *intr_tsk,
			const unsigned int intr_id, 
			const struct ss_container *tsks ) {

	int cnt = 0;
	struct ss_container *temp = tsks;
	struct task *tsk = NULL;
	int task_count = 0;
	
	while(temp) {

		int tmp_cnt = 0;
		if(!(tsk = (struct task *)temp->data) ) {

			fprintf(stderr,"Error:%s() no data in tasks\n", 
							__FUNCTION__);
			cnt = 0;
			break;
		}
		if(tmp_cnt = intr_popl_per_task(&intr_tsk[cnt], intr_id, tsk) ){
			cnt += tmp_cnt;	
		}		 		
		temp = temp->nxt;
		task_count++;
	}
	printf("Task_count: %d\n",task_count);
	return cnt;
}

#define INTR_INJ_SIZE(job_cnt) ((sizeof(struct ss_intr_inj)) + \
				(sizeof(struct ss_intr_task) * (job_cnt - 1)) \
				)
int ss_intr_task_bond(struct ss_container *tsk, struct ss_container *intr) {
	
	struct ss_intr *intr_e;
	struct ss_intr_inj *intr_inj;
	int ret = 0;
	struct ss_container *temp = intr;
	int tsk_cnt = 0;
	int int_seq_cnt = 0;
	
	if(!tsk | !intr) {
		fprintf(stderr, "Error:%s(): null params\n", __FUNCTION__);
		return -1;
	}

	while(temp) {

		intr_e = (struct ss_intr*)temp->data;

		if(!intr_e->no_of_tsk) {

			if(int_seq_cnt)
			{
				fprintf(stderr, 
				"Error:%s(): consecutive Empty interval(intr_id: %d)\n",
					 __FUNCTION__, intr_e->intr_id);
				ret = -1;
				break;
			}
			tsk_cnt = 1;
			int_seq_cnt++;
		}
		else {
			tsk_cnt = intr_e->no_of_tsk;
			int_seq_cnt = 0;
		}
		intr_inj = malloc(INTR_INJ_SIZE(tsk_cnt) );
		if(!intr_inj){
			fprintf(stderr, "Error:%s(): malloc fail(size:%ld) intr_id:%d\n", 
				__FUNCTION__, INTR_INJ_SIZE(tsk_cnt), intr_e->intr_id);
			ret = -1;
			break;
		}
		intr_inj->intr.intr_id = intr_e->intr_id;
		intr_inj->intr.core = intr_e->core;
		intr_inj->intr.start = intr_e->start;
		intr_inj->intr.end = intr_e->end;
		intr_inj->intr.sc = intr_e->sc;
		intr_inj->intr.no_of_tsk = intr_e->no_of_tsk;
		tsk_cnt = intr_populate_task(intr_inj->tsks, intr_inj->intr.intr_id, tsk);
		if( tsk_cnt != intr_inj->intr.no_of_tsk) {
			fprintf(stderr, "Error:%s(), Task Count:%d(exp: %d) Mismatch(intr: %d):\n",
				__FUNCTION__, tsk_cnt, 
				intr_inj->intr.no_of_tsk,
				intr_inj->intr.intr_id );
			ret = -1;
			break;
		}
#ifdef DEBUG
		intr_display(intr_inj);
#endif
		if(ss_inj_interval(INTR_INJ_SIZE(tsk_cnt), intr_inj) ) {
			fprintf(stderr, "Error:%s(): inj fail(intr: %d)\n", 
				__FUNCTION__, intr_inj->intr.intr_id);
			ret = -1;
			break;
		}
		free(intr_inj);
		temp = temp->nxt;		
	}

	if(ret) {
		perror("NOTE: NO FALLBACK MECHANISM STILL IMPLEMENTED\n");
	}
		
	return ret; 
}
