#ifndef SS_DATA_H
#define SS_DATA_H

#include "ss_parser.conf"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define OFFSET(absl_addr, rel_addr) ((int)((int)absl_addr) + ((int)rel_addr))	

/*Generic Container to handle disjoint data */
struct ss_container {
	void* data; /// generic data
	struct ss_container* nxt; /// pointer to nxt
};


/*
 * 4. this is used for associating the task with interval
 */
struct job_bond {
	unsigned int intr_id; /// interval to which this job belongs
	unsigned int est; /// est of the task
	unsigned int exec_count; /// keeping a count of how much is completed.
};


/*
 * 3.
 */

struct minor_id_bond {
	unsigned int minor_id; ///job id of the task.
	unsigned int est; /// early start time TODO: remove This
	unsigned int wcet; /// Worst case execution time
	unsigned int dl; /// deadline
	unsigned int coreid; /// core
	int intr_count; ///no of intervals to which this job belongs
	struct job_bond intr_bond[1]; ///list of interval offsets
};

/* 
 * 2.
 */
struct ss_task{
	unsigned int major_id; ///task id defined by offline scheduler
	int minor_id_count; ///Total count of the jobs that belongs to this tasks
	int minor_id_bonds[1]; ///lists offsets of minor ids.
};

/*
 * 1. this is first level 
 */
struct task {
	unsigned int pid; ///pid from fork.
	unsigned int period; /// TODO: yet to be done, lets not use this for now
	unsigned char bin[PATH_MAX]; /// binary itself
	int ss_tsk_len; /// holds length of the ss_tsk used for injection process.
	struct ss_task *ss_tsk;	
};



/*Interval Specific Data */


/*************************************************************/
/*Table parsed Data */

struct ss_intr {
	unsigned int intr_id; /// interval_id
	unsigned int core; /// core to which interval belongs
	unsigned int start; /// start of this interval
	unsigned int end; /// end of this interval
	signed int sc; /// spare capacity
	unsigned int no_of_tsk; /// number of tasks
};

struct ss_intr_task {
	int major_id;
	int minor_id;
	int pid;
};

struct ss_intr_inj {
	struct ss_intr intr;
	struct ss_intr_task tsks[1];		
};

/* very specific to slot shifting  aligning jobs and tasks*/

/*
 * TODO: There is a need for seperate table that would hold task details...
	task details: 
		1. binary,
		2. count of jobs 
		3. period
 */
struct ss_intr_bond {
	unsigned int intr_id;
	unsigned int est;
};
struct ss_job {
	unsigned int jid; /// minor id from offline schedule
	unsigned int major_id; /// task to which this job is associated
	unsigned int est; /// early start time, TODO: remove this
	unsigned int wcet; /// Worst case execution time
	unsigned int dl; /// deadline
	unsigned int coreid; /// core
	unsigned int interval_id; /// interval, TODO:this should be removed
	unsigned int intr_count; /// number of intervals to which this job belongs.
	char binary[PATH_MAX]; /// binary, every job would hold a binary that is same as task.
				/// TODO: change this redundancy.
	struct ss_container  *intr; /// list of interval ids
};

struct hp_header {
	unsigned int job_count;
	unsigned int int_count;
	unsigned int intr_job_asc;
	unsigned int slot_count;
	unsigned int slot_quantum;
};


/*This could be made into a array of ss_container pointer */
struct ss_data {
	struct ss_container *interval;
	struct ss_container *task;
	struct ss_container *res_data;
};

#endif /* SS_DATA_H */

