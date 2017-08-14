#ifndef __SS_INJ_DATA_H
#define __SS_INJ_DATA_H

/**
 * @struct job_bond 
 *  3.used for associating the task with interval
 */
struct job_bond {
	unsigned int intr_id; 		/// interval to which this job belongs
	unsigned int est; 		/// est of the task
	unsigned int exec_count; 	/// keeping a count of how much is completed.
};
/**
 * @struct minor_id_bond
 *  2.Creates minor id bond of a task/ job specific bond
 */
struct minor_id_bond {
	unsigned int minor_id; 		///job id of the task.
	unsigned int est; 		/// early start time TODO: remove This
	unsigned int wcet; 		/// Worst case execution time
	unsigned int dl; 		/// deadline
	unsigned int coreid; 		/// core
	int intr_count; 		///no of intervals to which this job belongs
	struct job_bond intr_bond[1]; 	///list of interval offsets
};
/**
 * @struct ss_task
 *  1.slot shift specific task data injected
 * 
 */
struct ss_task{
	unsigned int major_id; 		///task id defined by offline scheduler
	int minor_id_count; 		///Total count of the jobs that belongs to this tasks
	long minor_id_bonds[1]; 	///lists offsets of minor ids.
};

/*-----------------------INTERVAL DATA--------------------------- */

/**
 * @struct ss_intr
 * 
 * holds generic data of the interval
 */
struct ss_intr {
	unsigned int intr_id; 	/// interval_id
	unsigned int core; 	/// core to which interval belongs
	unsigned int start; 	/// start of this interval
	unsigned int end;	/// end of this interval
	signed int sc; 		/// spare capacity
	unsigned int no_of_tsk; /// number of tasks
};
/**
 * @struct ss_intr_task
 *
 * holds an instance of the list of jobs that belongs to interval 
 */
struct ss_intr_task {
	int major_id;		/// Major id of the task
	int minor_id;		/// minor id of the task
	int pid;		/// OS specific ID
};
/**
 * @struct ss_intr_inj
 *
 *  couples generic and task specific data into one structure
 *  for injection.
 */
struct ss_intr_inj {
	struct ss_intr intr;		/// interval struct holding generic data
	struct ss_intr_task tsks[1];	/// struct holding task specific interval data
};

#endif
