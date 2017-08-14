#ifndef LITMUS_SS_RESERVATIONS_H
#define LITMUS_SS_RESERVATIONS_H
/** @file ss_reservations.h 
 *  @brief contains function prototypes, enums and struct declarations.
 *
 *  This basically holds the class declarations of slot shifting
 *  scheduler, The whole architectural view of the implementaion
 *  is defined in tiki wiki of rts.eit.tukl 
 *  http://rts-wiki/tiki-index.php?page=LITMUS-rt+SLOTSHIFTING+ARCHITECTURE
 *
 * 	Notice the slot shifting algorithm is implemented independent of 
 *	Platform / simulator, Just filling the plugin class ss_os_fn
 *	the whole algorithm works seemlessly.
 *	
 *	The data injection part is done by ss_parser, which is not 
 *	as abstract as kernel implementation, but with little alteration
 *	The same module can be used for any RTOS or Simulator.
 *
 *  @author Gokul Vasan(gokulvas@gmail.com)
 *  @author John Gomboa(vaulttech@gmail.com)
 *
*/
#include <litmus/reservation.h>
#include <litmus/polling_reservations.h>
#include <litmus/ss_inj_data.h>

extern struct ss_data_hndl_fn ss_data;
/**
 * @enum ss_task_State
 *  task transition states.
*/
enum ss_task_state {
	JUST_ARRIVED, /// Task just arrived and not decided on G or !G
	INDEFINITE,   /// Aperiodic Tasks are moved here before Accepting
	DORMANT,      /// Tasks that is still not activated
	READY,        /// In Ready state
	RUNNING,      /// Current task executing
	BLOCKED,      /// Blocked state
	FINISHED,     /// Task Completed all its job execution
	UNKNOWN       /// error or top scenarios
};

/**
 * @enum ss_task_type
 * Slot shifting tries to address 3 kinds of tasks
 * This tries to differentiate the tasks.
 */
enum ss_task_type {
	PERIODIC,	/// Periodic/ offline Guaranteed Tasks.
	HARD_APERIODIC, /// Hard_aperiodic/ can be Guaranteed, comes with deadline
	SOFT_APERIODIC  /// comes without deadline and runs on free slots
};

/**
 * @enum task_queue_type
 *
 *  Needed 3 different kind of queues to handle 
 *  different task types.
 */
enum ss_task_queue_type {
	GUARANTEED,	/// Task Guaranteed offline / online
	NOT_GUARANTEED,	/// Task cannot be Guaranteed
	NOT_DECIDED	/// Task arrives online and still not decided on Guarantee.
};

/**
 * @struct ss_container
 *
 *  Basically a container that holds a list of data.
 *  data is named perspective because from task point 
 *  of view it holds intervals to which it belongs.
 *  From Interval point of view it holds tasks that 
 *  belongs to this intervals. 
 */
struct ss_container {
	void *perspective;	/// Data/ payload
	struct list_head list;	/// list
};

/**
 * @struct ss_intr_struct
 * 
 *  Holds the data needed for interval
 */
struct ss_intr_struct{
	signed int repetitions;		/// HP repetition count, used by class ss_hp_extend
	unsigned int id;		/// unique ID for the interval
	int sc;				/// Spare capacity of the interval
	unsigned int start;		/// Start slot of the interval
	unsigned int end;		/// End slot of interval
	struct ss_intr_inj *intr_struct;/// injected data: redundant data to be reused by ss_hp_extend
	struct list_head tasks; 	/// ref to intr_list in task 
	struct list_head list; 		/// list of intr
};

/**
 * @struct ss_queue_job
 *
 *  when we notice slot shifting works with jobs 
 *  rather than tasks and needs defined unique ID for
 *  every job so that there could be a 1:1 association
 *  with the interval:job. This structure tries to 
 *  provide that relation.
 *  Basically  a temporary view, used by ready queue to
 *  provide job perspective for the task on selection
 *  function and interval update functions.
 */
struct ss_queue_job {
	unsigned int est;		/// early start time of job
	unsigned int dl;		/// deadline of the job
	unsigned int wcet;		/// Worst case execution
	unsigned int jindex; 		/// job index within the minor_id
	struct job_bond* job;		/// reference to job_bond 
	struct minor_id_bond* minor_id; /// reference to minor_id bond 
	struct list_head list; 		/// list of ready.
};

/**
 * @struct ss_task_struct 
 *
 *  holds the slot shifting needed data for a task  
 */
struct ss_task_struct {
	enum ss_task_type type;		/// type of the data from enum ss_task_type
	enum ss_task_state  state;	/// state transition
	enum ss_task_queue_type q_type;	/// Current queue in which the task is
	struct ss_task 	  *s_task;  	/// refernce to the data that arrived from offline schedule
	struct ss_queue_job curr_job;	/// current job	if any that needs to be executed
	void  *os_task; 	    	/// linux/rtos specific task struct.
	struct list_head intr_list; 	/// holds for interval list
	struct list_head list;      	/// list of tasks
};

/**
 * @struct task_count
 *  Holds the global count of the number of tasks
 *  in the schedule.
 */
struct task_count {
	lt_t guaranteed_tasks;		/// count of guaranteed tasks
	lt_t h_aper_tasks;		/// count of hard aperiodic tasks
	lt_t not_guaranteed_tasks;	/// count of not guaranteed tasks
};

struct ss_data_hndl_fn;

/* -- function pointers used by data class-- */
/**
 * @fn krnl_side_job_complete_t
 * @brief: 
 * @param ss      : 
 * @return: void
 */
/**
 * @fn initialize
 * @brief: initilizes all the list in data_hndl_fn
 * @param this pointer 
 * @return: void
 */
typedef void (*initialize)(struct ss_data_hndl_fn*);
/**
 * @fn add_data
 * @brief: Generic functor that is used for adding the data
 *	to the desired list, the functor is morphed for the
 *	desired need in its particular classes
 * @param this pointer
 * @param void pointer to data
 * @return: void
 */
typedef void (*add_data)(struct ss_data_hndl_fn*,  void *);
/**
 * @fn get_data
 * @brief: Generic functor that is used for getting the data
 *	from the desired list, the functor is morphed for the
 *	desired need in its particular classes
 * @param this pointer
 * @param void pointer to data
 * @return: void
 */
typedef void* (*get_data)(struct ss_data_hndl_fn*, void *);
/**
 * @fn split_intr
 * @brief: splits the given interval at desired point
 * @param this pointer
 * @param *intr: interval that needs split
 * @param *split_point: point where split should happen
 * @curr_slot : current slot executed 
 * @return:returns new interval created
 */
typedef void* (*split_intr)(struct ss_data_hndl_fn*,
			void *intr,
			int split_point,
			lt_t curr_slot);
/**
 * @fn get_curr_data
 * @brief: used to provide data from curr list
 *	morphed for requiered needs by class
 * @param this pointer
 * @return: returns the desired data
 */
typedef void* (*get_curr_data)(struct ss_data_hndl_fn*);
/**
 * @fn update_task_state
 * @brief: manages task state
 *	  - provides a abstraction over data handling fns
 *	  - checks and updates
 * @param this pointer
 * @param void*: task that needs update
 * @param ss_task_state: state to move 
 * @return: 0 on suceess else negative val
 */
typedef int (*update_task_state)(struct ss_data_hndl_fn*,
				void*,
				enum ss_task_state);
/**
 * @fn updates_task_q_state
 * @brief:  updates task q state based on state transition
 * @param This pointer
 * @param task: pointer to task
 * @param queue_type: which queue to move
 * @return: 0 on sucess else -ve val
 */
typedef int(*update_task_q_state)(struct ss_data_hndl_fn*,
				void * task,
				enum ss_task_queue_type);
/**
 * @fn update_task_quantum_t
 * @brief: function that manages certain functionality 
 *	when time or slot progresses. An approach to abstract 
 *	the data handling from other classes
 * @param this pointer
 * @param task: task that progressed
 * @param job_completed: a flag that returns whether job complettion
 * @return: task itself, not necessary
 */
typedef void*(*update_task_quantum_t)(struct ss_data_hndl_fn*,
				void * task,
				int * job_completed);

/* ---Classes and declarations--- */

/**
 * @struct ss_data_hndl_fn
 *
 * Description: 
 *   -Tries to couple data and the functions 
 *   	that handle the data into one single structure.
 *   -This Whole structure imitates a class like implemetnaion.
 *   - Handles only  handling functionality of slot shifting
 *   - FUNCTORS: update_tsk_state and update_tsk_quantum tries to 
 *   		 abstract the core algorithm from many data handling 
 *		 functions.
 *   -Takes a self/this pointer as first parameter.
 *   FIXME: NEEDS a better abstract, seperate, OO classes. 
 */

struct ss_data_hndl_fn {

		struct list_head ready_list;		/// Ready list
		struct list_head blkd_list;		/// Blocked list
		struct list_head task_list;		/// Guaranteed tasks list
		struct list_head unConcluded_list;	/// un Concluded task list
		struct list_head sft_aper_list;		/// soft Aperiodic List TODO: Needs seperate FW
		struct list_head interval_list;		/// Interval List
		struct list_head* curr_intr;		/// reference of current interval
		struct list_head* curr_task; 		/// reference of current task
		initialize init_list;			/// Function that initializes all the list
		add_data add_interval;			/// function that adds interval to interval_list
		add_data add_guaranteed_task;		/// function that adds task to task_list or Guaranteed list
		add_data add_unConcluded_task;		/// function that adds task to unConcluded list
		add_data add_notGuaranteed_task;	/// function that adds task to not guaranteed / sft_aper_list
		add_data add_block_list;		/// function that adds task to blocked list
		add_data add_ready_list;		/// function that adds job to ready list
		get_data get_nxt_intr;			/// function that provides the next interval of the interval provided as i/p
		get_data get_prev_intr;			/// function that provides the prev interval of the interval provided as i/p
		get_data release_task_with_curr_est;	/// function that Works on tasks, checks the list of task_list to see 
							/// 	who all needs to be added to ready queue. 
							/// 	-Takes current slot count as i/p 
							/// 	-works on task_list and adds to ready_list	
		get_data get_unConcluded_task;		/// function that gets a task from unconcluded list of tasks
		get_data get_notGuaranteed_task;	/// function that gets a task from not guaranteed list TODO: needs seperate FW
		get_data get_nxt_rdy_task;		/// function that gets next ready task t be released: EDF 
		get_data get_intr_with_id;		/// Get interval which matches ID, IP: ID of interval, o/p: interval/NULL
		split_intr split_interval;		/// splits interval provided, i/p: interval, split_point. curr_slot, O/p: new interval
		add_data remove_interval;		/// Removes the Given i/p interval from interval_list
		get_data get_task;			/// Takes major_id as i/p and spits out ss_task_struct/NULL
		get_curr_data get_curr_task;		/// Function that spits current task
		get_curr_data get_curr_intr;		/// Function That spits out current interval
		add_data update_curr_tsk;		/// Function That updates curr_task
		get_data update_curr_intr;		/// Function that update curr_intr
		update_task_state update_tsk_state;	/// Function that manages state transition of tasks
		update_task_q_state update_tsk_q_state; /// Function that manages queue transition of tasks
		update_task_quantum_t update_tsk_quantum;/// function that updates and decides on task progress
		struct task_count tsk_cnt;		/// task count 
		lt_t intr_cnt;				/// interval count
};

struct slot_shift;

typedef struct ss_intr_struct interval;
typedef struct ss_task_struct task;

/* -- function pointers used by algorithmic class-- */
/**
 * @fn slot_shift_core_t
 * @brief: core selection and interval functionality
 *	1.INTERVAL UPDATE
 *	2.APERIODIC CHECK 
 *	3.EDF
 * @param slot_shift: Object holding conglomeration of
 *			needed classes
 * @return: selected task.
 */
typedef task* (*slot_shift_core_t) (
		struct slot_shift *ss
);

/**
 * @fn acceptance_test_t
 * @brief: Acceptance test on aperiodic arrival
 *	   On Valid SC it also runs guarantee algorithm.
 * @param slot_shift: Object holding conglomeration of
 *			needed classes
 * @return: The count of acepted firm Aperiodics.
 */
typedef unsigned int (*acceptance_test_t) (
		struct slot_shift *ss
);

/**
 * @fn guarantee_algo_t
 * @brief: Guarantee algorithm for firm aperiodics
 * @param ss      : Object holding conglomeration of
 *			needed classes
 * @param intr    : interval in which deadline falls
 * @param aperidic: the task that needs to be guaranteed
 * @return: 0: success.
 */
typedef unsigned int (*guarantee_algo_t) (
		struct slot_shift *ss,
		interval *intr,	
		task *aperiodic
);

/**
 * @fn calculate_sc_t
 * @brief: Guarantee algorithm's recalculation of SC
 * @param ss      : Object holding conglomeration of
 *			needed classes
 * @param intr    : interval in which deadline falls
 * @param aperidic: the task that needs to be guaranteed
 * @return: void
 */
typedef void (*calculate_sc_t) (
		struct slot_shift *ss,
		interval *intr,
		task *aperiodic
);

/**
 * @fn update_sc_t
 * @brief: runs for every slot, Basically manages SC
 * @param ss      : Object holding conglomeration of
 *			needed classes
 * @param next    : task that got selected in previous slot
 * @return: void
 */
typedef void (*update_sc_t) (
		struct slot_shift *ss,
		task* next
);

/**
 * @fn ready_queue_update_t
 * @brief: updates ready queue, by iterating over data_hndl_fns
 *	Provides an abstraction over data_hndl_fn.
 * @param ss      : Object holding conglomeration of
 *			needed classes
 * @return: void
 */
typedef void (*ready_queue_update_t) (
		struct slot_shift *ss
);

/**
 * @struct ss_algo_fn
 *
 * DESCRIPTION:
 *	-Implements slot shifting Algorithm.
 *	-Made platform independednt approach.
 *
 */
struct ss_algo_fn {
	slot_shift_core_t slot_shift_core;	/// selection and interval functionality
	acceptance_test_t acceptance_test;	/// function for Acceptance test on Aperiodics
	guarantee_algo_t guarantee_algo;	/// function for Guarantee algorithm on aperiodics
	calculate_sc_t calculate_sc;		/// helper function that calculates SC for aperiodics
	update_sc_t update_sc;			/// slot level SC update
	ready_queue_update_t ready_queue_update; /// updates and tries to abstract data hndling function
};

/**
 * @fn reinsert_past_interval_t
 * @brief: hyper period wrap around functionality.
 * 	Given a pointer to an interval that is part 
 * 	of the data handler list, reinserts it in the end of the list, 
 * 	with reset values. 
 * @param ss      : Object holding conglomeration of
 *			needed classes
 * @param intv	  : Interval That needs wrap around
 * @return: void
 */
typedef void (*reinsert_past_interval_t) (struct slot_shift *ss,
						void *intv);
/**
 * @struct ss_hp_extend
 *
 * DESCRIPTION:
 *	-Implements interval wrap around algorithm.
 *	-Made into a seperate class for scalability.
 *
 */
struct ss_hp_extend {
	reinsert_past_interval_t reinsert_past_interval; /// Function that wraps the interval
};

/* --------------------OS FUNCTIONALITY--------------------------*/
/* 
 * Forms a plugin functionality for making slot shifting 
 * Platform independent functionality.
 * 
 */

/**
 * @fn on_nxt_job_decision 
 * @brief: is called immediatly after the nxt job is decided
 *	by the core functionality.
 * @param ss      : Object holding ss class
 * @return: void
 */
typedef void (*on_nxt_job_decision) (struct slot_shift *ss);
/**
 * @fn krnl_side_job_complete_t
 * @brief: function that gets triggered when OS 
 *	needs to send job completion of the task
 *	
 * @param ss      : Object holding slot_shift
 * @param task	  : task whose job got completed. 
 * @return: void
 */
typedef void (*krnl_side_job_complete_t) (struct slot_shift *ss,
			struct ss_task_struct *task);
/**
 * @struct ss_os_fn
 *
 * DESCRITION:
 *	Basically holds the OS specific functionalities
 *	That needs to be carried out on particular event or
 *	decision taken.
 *	Suppose a sem/mutex is needed in implementaion of 
 *	slot shifting then it needs to be wrapped around here 
 *	and used in other classes, Direct usage of OS functionality
 *	in other slot shifting classes is considered CRIME.
 *
 */
struct ss_os_fn {
	on_nxt_job_decision on_job_decn;		/// function called on job decision taken by slot shifting 	
	krnl_side_job_complete_t krnl_side_job_complete;/// function for job completion event trigger in OS
};

/*
 *	This Class is mainly created for initializing aperiodic tasks.
 *	When we notice a aperiodic tasks are mostly in suspended state 
 * 	till a event gets triggered, For the task to move itself to
 *	suspended state there needs to be a framework
 * 	That can create task and run its initialization phase to move it
 *	self to suspended state.
 *	
 */
struct ss_init_phase;


/**
 * @fn	get_nxt_task 
 * @brief: gets the next aperiodic task that needs to be initialized
 *	
 * @param diese  : Trying to imitate this pointer for ss_init_phase 
 * @return: if queued return nxt aperiodic else NULL 
 */
typedef void* (*get_nxt_task) (struct ss_init_phase *diese);
/**
 * @fn add_aper_task
 * @brief: adds the aperiodic task to the queue for initialization
 *	
 * @param diese  : Trying to imitate this pointer for ss_init_phase
 * @param tsk : task that needs to be added into queue 
 * @return: void
 */
typedef void  (*add_aper_task)(struct ss_init_phase *diese, void* tsk);
/**
 * @fn initialize_periodic
 * @brief: Along with aperiodics periodics also arrive,
 *	This function facilitates the  periodics to get prepared 
 *	as periodics get release the platform may think it is 
 *	running as well 
 *	
 * @param diese  : Trying to imitate this pointer for ss_init_phase
 * @param dh : data_handling function object
 * @data : platform specific data, In Litmus RT we use this for nxt 
 *	release time
 * @return: void
 */
typedef void* (*initialize_periodic)(struct ss_init_phase *diese,
				struct ss_data_hndl_fn *dh, void *data);

/**
 * @fn set_aper_cnt
 * @brief: This function sets aperiodic count.
 *	either platform or implementation needs to figure 
 *	out how to set this data, if wrongly set then system may 
 *	either infinitely waiting for more aperiodics than expected
 *	or will ignore some aperiodic initialization
 *	
 * @param diese   : this pointer to ss_init_phase
 * @param cnt 	  : the count on aperiodic that needs to be set 
 * @return: void
 */
typedef void (*set_aper_cnt) (struct ss_init_phase*, unsigned int cnt);


/**
 * @struct ss_init_phase
 *
 * DESCRIPTION:
 *	-Class implementaion for initialization phase
 *
 */
struct ss_init_phase {
	struct list_head aperiodic_tsks; 	/// temp queue to manage aperiodic arrival
	unsigned int tick_cnt;		 	/// the periodic for wait for aperiodics 
	unsigned int aper_cnt;		 	/// total aperiodic that needs to arrive
	set_aper_cnt set_aper_cnt;	 	/// function that sets aper_cnt
	add_aper_task add_aper_tsk;	 	/// function that adds aper to aperiodic_tasks
	get_nxt_task get_nxt_aper;	 	/// function that iterates and returns nxt aperiodic
	initialize_periodic init_per_tsks; 	/// function that prepares periodics
};


/**
 * @struct slot_shit
 *
 * DESCRIPTION:
 *	- Main class for slot shifting algorithm
 *	- Holds the conglomeration of all that classes needed for slot shifting
 *	- holds the state of the slot shifting implementaion
 *
 */
struct slot_shift {
	struct ss_algo_fn *algo;	/// Core algorithm object
	struct ss_data_hndl_fn *dh;	/// Data handling object
	struct ss_os_fn *os;		/// os wrapper object
	struct ss_init_phase *init;	/// init phase object
	struct ss_hp_extend *hp_ext;	/// hyper period wrap around object
	lt_t  curr_slot;		/// variable to manage current slot count
};

/**
 * @struct slot_shift_reservation
 *
 * DESCRIPTION:
 *	- Extending slot shifting to adapt itself to reservation framework
 *	- we extended table driven reservation to make things work for slot shifting
 *	- Though extending td was a bad idea , but it was strongly suggested.
 *
 */
struct slot_shift_reservation {
	struct table_driven_reservation td_res; /// extend table driven reservation
	struct slot_shift ss;			/// slot shifting itself
	struct hrtimer timer;			/// HRtimer for decision functionality
	lt_t slot_quantum;			/// length of a slot
	unsigned int in_slot_boundary;		///flag indicates when exactly dispatch/schedule should work. 
};

/**
 * @fn ss_task_struct_init
 * @brief: function that gets called on task injection
 *	from user space, basically initializes kernel
 *	side DS with incloming data
 * @param type      : task type
 * @param _s_tsk    : data from user space
 * @param pid       : PID of the task 
 * @return: ss_task_struct
 */
struct ss_task_struct* ss_task_struct_init(
			enum ss_task_type type,
			struct ss_task *_s_tsk,
			pid_t pid);

/**
 * @fn slot_shift_reservation_init
 * @brief: initializes slot shifting based reservation
 *	
 * @param ss_res : struct containing slot_shiftin_reservation
 * @param major_cycle : td params
 * @param interval : td param
 * @param num_intervals: td_param
 * @param slot_quantum : length of the slot in milliseconds
 * @return: void
 */
void slot_shift_reservation_init (
		struct slot_shift_reservation *ss_res,
		lt_t major_cycle,
		struct lt_interval *intervals,
		unsigned int num_intervals,
		lt_t slot_quantum);

/**
 * @fn add_task
 * @brief: adds task injected into data_class
 *	
 * @param data_class: object to data_hndl_fn
 * @param task : initialized ss_task_Struct 
 * @return: 0 on success.
 */
int add_task(struct ss_data_hndl_fn *data_class,
	struct ss_task_struct *task);


/**
 * @fn get_slot_shifting_reservation
 * @brief: inline function that returns the slot_shifting reservation
 *	from struct reservation
 *	
 * @param res: pointer to reservation containing slot shifting
 * @return: struct slot_shift_reservation*
 */
static inline struct slot_shift_reservation *get_slot_shift_reservation(
				struct reservation *res)
{
	struct table_driven_reservation *tdres =
		container_of(res, struct table_driven_reservation, res);
	return container_of(tdres, struct slot_shift_reservation, td_res);
}

/**
 * @fn print_interval
 * @brief: prints the field of the given interval
 *	
 * @param i: interval that needs to be printed 
 * @return: void
 */
void print_interval(struct ss_intr_struct *i);

#endif /* LITMUS_SS_RESERVATIONS_H */


