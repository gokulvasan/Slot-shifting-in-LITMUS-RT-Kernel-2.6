#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "fsm_parser.h"


/**********************************States********************************************/
struct fsm;

typedef int (*fsm_action_t) (struct fsm *fsm);


enum fsm_primary_state {
	STATE_HP_START = 0, // this state will read the next set of hyper periods with its job count.
	STATE_JOB_TRAVERSAL_START, // start job traversal
	STATE_JOB_TRAVERSAL_FINISH,
	STATE_INT_TRAVERSAL_START,
	STATE_INT_TRAVERSAL_FINISH,
	STATE_HP_END,
	STATE_END_STATE
};

enum fsm_secondary_states_job {
	STATE_JOB_START = 0, // malloc the data
	STATE_JOB_MINOR_ID,
	STATE_JOB_MAJOR_ID,
	STATE_JOB_WCET,
	STATE_JOB_DL,
	STATE_JOB_COREID,
	STATE_JOB_INTR_COUNT,
	STATE_JOB_INTR_BOND,
	STATE_JOB_BINARY,
	STATE_JOB_NXT, // decision on iteration
	STATE_JOB_END  // exit
};

enum fsm_secondary_states_interval {
	STATE_INT_START = 0,
	STATE_INT_ID,
	STATE_INT_CORE_ID,
	STATE_INT_START_TIME,
	STATE_INT_END_TIME,
	STATE_INT_SC,
	STATE_INT_TASK_COUNT,
	STATE_INT_JOBS_TRAVERSAL,
	STATE_INT_NXT,
	STATE_INT_END
};
/**************************************************************
 * TODO: apply Backus-Naur Form 
 *Class Name:
 *      struct fsm
 *
 *General Description:
 *		Typical State machine that parses the given input file.
 *       Main objective of this machine is to convert state x-> state y.
 *        a typical state machine with {Σ, S, s, Ɣ, F }
 *
 *Field definition:
 *      1. curr_state: holds the current state of the machine
 *      2. prev_state : holds the previous state of the machine.
 *      3. state: this is functors that defines every state s
 *      4. transition_function Ɣ: functor that takes the machine from S X S
 *      5. ret_value: holds the state of the whole machine on basis of per state
 *                      0: current state is successful.
 *                      -1: current state is unsuccessful.
 *                      1 : whole state machine is successful F.
 *      *********** MISCELANEOUS VARIABLES************
 *      6. fd: file holding the format x
 *      7. table: buffer that holds the local copy of the fd but per HP basis
 *      8. idx: index of the buffer,
TODO: this could be avoided rather direct pointer manipulation
would make things much better when file size increases.
 *      9. data: holds the converted struct in the form of LL, ready for inject.
 *****************************************************************/
struct fsm {
	enum fsm_primary_state curr_state;
	enum fsm_primary_state prev_state;
	fsm_action_t state[STATE_END_STATE];
	fsm_action_t transition_function;
	int ret_value;

	FILE* fd;
	char* table;
	int idx;
	struct ss_data* data;
};



/***************************************HELPER FUNCTIONS START****************************************/
/*****************************************************************************************************
 ********************************ALL HELPER FUNCTIONS SHOULD BE PLACED HERE****************************
 ******************************************************************************************************/


/**************************************************************
 *   FUNCTION NAME: fsm_strncpy
 *   GENERAL DESCRIPTION: does strncpy function;
 *        but rather than checking for '/0' checks for 0xFF.
 *        appends the dst with '/0'
 *   INPUT PARAMETER:
 *           src: pointer to source.
 *           n : number of bytes to be copied.
 *           dst: destination addr where source needs to be copied
 *   OUTPUT :
 *           string copied to dst with '/0' appended
 ***************************************************************/
int fsm_strncpy(char *dst,char *src, const size_t n) {

	unsigned char* s = src;
	unsigned char* d = dst;
	int i = 0;

	while((i < n) && (0xFF != ((int)*s)) ){

		*d = *s;
		s++;
		d++;
		i++;
	}

	*d = '\0';

#ifdef DEBUG
	printf("%s: bytes copied: %d, Bin: %s\n", __FUNCTION__, i, dst);
#endif
	return i;

}
/**************************************************************
 *   FUNCTION NAME: fsm_stoi
 *   GENERAL DESCRIPTION: copies MAX_NON_STRING_FIELD_LENGTH
 *                        from char to int dst ;
 *
 *   INPUT PARAMETER:
 *           src: pointer to source.
 *           dst: destination addr of type int
 *   OUTPUT :
 *           string copied to dst
 ***************************************************************/
int fsm_stoi(unsigned int *dst,char * const src) {

	char* t = src;
	char j[MAX_NON_STRING_FIELD_LENGTH+2] = {"\0"};
	size_t i = 0;

	memcpy(dst, src, MAX_NON_STRING_FIELD_LENGTH);
	return MAX_NON_STRING_FIELD_LENGTH;
}


/**************************************************************
 *   FUNCTION NAME: fsm_check_header_consistency
 *   GENERAL DESCRIPTION: checks header consistency
 *
 *   INPUT PARAMETER:
 *           e_mn: expected magic number.
 *           header: actual magic number
 *   OUTPUT :
 *           0: correct header
 *           -1: wrong header
 ***************************************************************/
int fsm_check_header_consistency(const unsigned int e_mn, const char* header ) {

	char header_magic_number[DS_MAGIC_NO_LENGTH+2];
	int hp_magic_number =0;

	if(header[0] != DATA_SEPERATOR_DELIMITER) {
		fprintf(stderr, "%s: Parsing failed, wrong delimiter: %c (Exp: %c)\n"
				,__FUNCTION__, header[0], DATA_SEPERATOR_DELIMITER);
		return -1;
	}
	memcpy(&hp_magic_number, &header[1], DS_MAGIC_NO_LENGTH);
	if(hp_magic_number != e_mn) {
		fprintf(stderr, "%s: parsing failed, wrong magic number:%x (expected:%x)\n",
				__FUNCTION__, hp_magic_number, e_mn);
		return -1;
	}
	if('\n' != header[2+DS_MAGIC_NO_LENGTH] || '\r' != header[1+DS_MAGIC_NO_LENGTH] ){
		fprintf(stderr, "%s: parsing failed, wrong EOL\n", __FUNCTION__);
		return -1;
	} /* check for EOL */
	return DS_PER_LENGTH;
}

/**************************************************************
 *   FUNCTION NAME: fsm_per_ji_asco
 *   GENERAL DESCRIPTION: creates per job interval ascosiation.
 *			 It directly increments buffer.
 *   INPUT PARAMETER:
 *           buffer: buffer of HP.
 *   OUTPUT :
 *           NULL: ERROR
 *           On success returns pointer to created job_bond
 *           
 ***************************************************************/
struct job_bond* fsm_per_ji_asco(char *buffer) {

	struct job_bond *job_asco = NULL;

	if(!buffer) {
		fprintf(stderr, "Error: %s: Null param\n", __FUNCTION__ );
		return NULL;
	}
	if(!(job_asco = malloc(sizeof(struct job_bond)) ) ) {
		fprintf(stderr,"Error: %s: alloc failure job_bond\n",
				__FUNCTION__);
		return NULL;
	}

	do {

		memset(job_asco, 0x00, sizeof(struct job_bond));
		memcpy(&job_asco->intr_id,buffer, MAX_NON_STRING_FIELD_LENGTH);
		buffer += MAX_NON_STRING_FIELD_LENGTH;

		if(DATA_GRP_DELIMITER != *buffer) {

			fprintf(stderr, "Error: %s: delimiter failure: %c(expected: %c)\n",
					__FUNCTION__, *buffer, DATA_GRP_DELIMITER);
			break;
		}
		buffer++; 
		memcpy(&job_asco->est, buffer, MAX_NON_STRING_FIELD_LENGTH);
		buffer += MAX_NON_STRING_FIELD_LENGTH;
#ifdef DEBUG
		printf("job assoc parsed data: %d, %d\n", 
				job_asco->est, job_asco->intr_id);
#endif
		return job_asco;

	} while(0);

	free(job_asco);
	return NULL;
}

/**************************************************************
 *   FUNCTION NAME: fsm_job_intr_asco
 *   GENERAL DESCRIPTION: creates job interval ascos of a job.
 *   INPUT PARAMETER:
 *           buffer: buffer of HP.
 *   OUTPUT :
 *           0: ERROR
 *           On success returns length of the parsed data.
 *           
 ***************************************************************/

int fsm_job_intr_asco(char *buffer, struct ss_job *job) {

#define JOB_INTR_LNGTH(x) ( JOB_INTR_BOND_LEN(x) \
		+ PER_JOB_INTR_DELIMITER_LENGTH)

	int i = 0;
	struct ss_container *data = NULL;
	struct job_bond *job_asco = NULL;
	int halt = 0;

	if(!buffer || !job) {
		fprintf(stderr, "Error: %s: NULL params\n", __FUNCTION__);
		return 0;
	}
	if(DATA_SEP_SEC_DELIMITER_START != *buffer) {
		fprintf(stderr, "Error: %s: Wrng delimiter %c (Exp: %c)\n", 
				__FUNCTION__, *buffer, DATA_SEP_SEC_DELIMITER_START);
		return 0;
	}
	halt++;

	while((DATA_SEP_SEC_DELIMITER_END != buffer[halt]) 
			&& (halt < JOB_INTR_LNGTH(job->intr_count)) ) {

		if(!(data = malloc(sizeof(struct ss_container)) ) ) {
			fprintf(stderr,"Error: %s: alloc fail ss_container, count: %d\n",
					__FUNCTION__, i);
			return 0;
		}

		if(!(data->data = fsm_per_ji_asco(&buffer[halt]))) {
			free(data);
			fprintf(stderr, "Error: %s: job assoc failure @ :%d\n",
					__FUNCTION__, i);
			return 0;
		}
		halt += JOB_FIELD_PER_INTR_BOND_LENGTH; 
		data->nxt = job->intr;
		job->intr = data;

		if(PRIMARY_DELIMITER == buffer[halt]) {
			halt++;
		}
		i++;

	} // while end

	if(i != job->intr_count ) {
		fprintf(stderr, "Error: %s: intr count mismatch:%d(Exp: %d)", 
				__FUNCTION__, i, job->intr_count);
		return 0;
	}
#ifdef DEBUG
	printf("%s: count: %d\n", __FUNCTION__, i);
#endif

	return(JOB_INTR_LNGTH(i));
}

#if 1 //def DEBUG
/**************************************************************
 *   FUNCTION NAME: job_print
 *   GENERAL DESCRIPTION: prints job param
 *
 *   INPUT PARAMETER:
 *           j: struct ss_container containing job.
 *
 *   OUTPUT : NONE
 ***************************************************************/
void job_print(struct ss_container *j) {

	struct ss_job *job = j->data;
	struct ss_container *ji = job->intr;
	struct job_bond *jintr;

	if(!j || !job){
		fprintf(stderr, "ERROR: %s : null param\n", __FUNCTION__);
		return;
	}

	printf("minor_id: %d, major_id: %d, intr:%d, dl: %d, wcet: %d, core: %d, intr_cnt: %d Bin:%s [ ",
			job->jid, 
			job->major_id, 
			job->interval_id, 
			job->dl, job->wcet, 
			job->coreid, 
			job->intr_count, 
			job->binary);

	while(ji) {
		jintr = ji->data;     
		printf("%d:%d, ", jintr->intr_id, jintr->est);
		ji = ji->nxt;
	}
	printf("]\n");

}

#endif

/******************************************HELPER FUNCTIONS END***************************************/

/*******************************************STATE MACHINE DEFINITONS**********************************/

//TODO: ALL FUNCTIONS NEEDS BRIEF DESCRIPTION.

int fsm_parse_hp_header(char *buffer, unsigned int *data)
{
	int i = 0;

	if(!buffer || !data) {
		fprintf(stderr, "Error: %s: null params\n", __FUNCTION__);
		return -1;
	}
	while(i < HP_FIELD_COUNT && ('\r' != *buffer) && ('\n' != *buffer) ){
		memcpy(&data[i], buffer, MAX_NON_STRING_FIELD_LENGTH);
		buffer = buffer + HP_PER_FIELD;
		i++;
	}
	if(i < (HP_FIELD_COUNT - 1) ) {
		fprintf(stderr, "Error: %s: Field param Error\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int fsm_hp_start(struct fsm* fsm_iterator) {

#define HP_HEADER_LENGTH (DS_PER_LENGTH + HP_FIELD_LENGTH)

	char header[HP_HEADER_LENGTH + 2];
	char data[MAX_NON_STRING_FIELD_LENGTH + 2]; /// scribble pad
	size_t read_return;
	struct hp_header *hp_field;
	int ret;
	int len = 0;

	do {
#ifdef DEBUG
		printf("HP_HEADER_LENGTH: %d\n", HP_HEADER_LENGTH);
#endif
		read_return = fread(header, HP_HEADER_LENGTH, 1, fsm_iterator->fd);
		if( read_return < HP_HEADER_LENGTH ){

			if(feof(fsm_iterator->fd)){

				if(read_return > 0){
					fprintf(stderr,"error: %s: parsing failed, file struct is wrong\n", 
							__FUNCTION__);
					fsm_iterator->ret_value = -1;
					break;
				}
				printf("1.reached EOF\n");
				fsm_iterator->ret_value = 1;
				break;
			}
		}
		/*
		 * 2. check consistency and copy the data from file to buffer.
		 */
		if(-1 == fsm_check_header_consistency(HP_START, header)) {
			fsm_iterator->ret_value = -1;
			break;
		}

		hp_field = malloc(sizeof(struct hp_header));
		if(!hp_field) {
			fprintf(stderr, "Error: %s, malloc failed on hp_field\n",
					__FUNCTION__);
			fsm_iterator->ret_value = -1;
			break;
		}
		ret = fsm_parse_hp_header((char*)&header[DS_PER_LENGTH],
					  (unsigned int*)hp_field);
		if(-1 == ret){
			fsm_iterator->ret_value = -1;
			break;	
		}
		fsm_iterator->data->res_data = malloc(sizeof(struct ss_container));
		if(!fsm_iterator->data->res_data) {
			fprintf(stderr, "Error\n");
			fsm_iterator->ret_value = -1;
			break;
		}
		fsm_iterator->data->res_data->data = hp_field;
		fsm_iterator->data->res_data->nxt = NULL;

		len = BUFFER_LENGTH(hp_field->int_count, hp_field->job_count, hp_field->intr_job_asc);
#ifdef DEBUG
		fprintf(stdout, "buffer(%d,%d,%d):%d, slotcnt:%d, slotlen: %d\n", 
				hp_field->int_count, 
				hp_field->job_count, 
				hp_field->intr_job_asc,
				len,
				hp_field->slot_count, hp_field->slot_quantum
		       );
#endif		
		/* Adding 5 bytes extra for safety :) */
		if(!(fsm_iterator->table = (char *)malloc(len + 5))) {
			fprintf(stderr, "Error: %s: failed to alloc buffer:%d\n",
					__FUNCTION__, len);
			fsm_iterator->ret_value = -1;
			break;
		}
		/*
		 * copy the HP into the buffer
		 */
		if(!(read_return = fread(fsm_iterator->table, len, 1, fsm_iterator->fd))) {

			fprintf(stderr, "error: %s: failed to get table: read_return: %zd(expected:%d)\n",
					__FUNCTION__, read_return, len);

			if(feof(fsm_iterator->fd)) {
				fprintf(stderr, "Error: %s: 2.REACHED EOF in middle\n",
						__FUNCTION__);
			}
			fsm_iterator->ret_value = -1;
			break;
		}

	} while(0);

	return 0;
}


unsigned int job_decide_nxt(struct fsm* fsm_iterator,
		unsigned int curr){

	unsigned int state_t = curr;

#ifdef DEBUG
	printf("%s:\n", __FUNCTION__);
#endif
	do {

		if(-1 == fsm_iterator->ret_value) {
			fprintf(stderr, "Error: %s: job state(%d) Failed\n", 
					__FUNCTION__, state_t);
			break;
		}

		switch(state_t) {

			/* JOBS that have no delimiters*/
			case STATE_JOB_START:
			case STATE_JOB_BINARY:
				curr++;
				return curr;

			case STATE_JOB_NXT:
				{
					int i = fsm_iterator->idx;
					if( (('\r' != fsm_iterator->table[i]) || 
								('\n' != fsm_iterator->table[i+1]))) {
						fprintf(stderr, 
								"Err: %s: STATE_JOB_NXT,parsing failed, NO EOL\n",
								__FUNCTION__);
						break;
					}
					else {
						i += EOL_LENGTH;
						if(DATA_SEPERATOR_DELIMITER == fsm_iterator->table[i]){
							curr = STATE_JOB_END;
							fsm_iterator->ret_value = 0;
						}
						else {
							curr = STATE_JOB_START;
						}
					}
					fsm_iterator->idx += 2; // '\r\n'
					return curr;
				}

			case STATE_JOB_END:
				return curr;

			default:

				if(PRIMARY_DELIMITER != fsm_iterator->table[fsm_iterator->idx]) {
					fprintf(stderr, "%s: ERROR: wrong delimiter\n", __FUNCTION__);
					break;
				}
				curr++;
				fsm_iterator->idx++;
				return curr;

		}

	} while(0);

	curr = STATE_JOB_END;
	fsm_iterator->ret_value = -1;
	return curr;
}

int fsm_job_traversal_start(struct fsm* fsm_iterator) {

	int exit = 0;
	unsigned int state;
	char scrib[MAX_NON_STRING_FIELD_LENGTH+2];
	int i;
	struct ss_job *job;
#ifdef DEBUG
	printf("%s idx:%d \n", __FUNCTION__, fsm_iterator->idx);
#endif
	/* 1. check whether we are in valid field
	 * 2. if not just return, let transition function take decision
	 */
	if( -1 == (i = fsm_check_header_consistency(JOB_START, &fsm_iterator->table[fsm_iterator->idx]))) {
		fprintf(stderr, "%s: Error, header consistency failed\n", __FUNCTION__);
		fsm_iterator->ret_value = -1;
		return 0;
	}

	fsm_iterator->idx += i;
	state = STATE_JOB_START;

	while(!exit) {
		switch(state){

			case STATE_JOB_START:
				if( !(job = malloc(sizeof(struct ss_job)))) {
					fprintf(stderr,"%s:Failed to malloc job\n", __FUNCTION__);
					fsm_iterator->ret_value = -1;
					exit = 1;
				}
				break;
			case STATE_JOB_BINARY:

				i = fsm_strncpy(job->binary, &fsm_iterator->table[fsm_iterator->idx], PATH_MAX);
				fsm_iterator->idx += PATH_MAX;

				break;
			case STATE_JOB_COREID:
				if(i = fsm_stoi(&job->coreid, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_MAJOR_ID:
				if(i = fsm_stoi(&job->major_id, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_DL:
				if(i = fsm_stoi(&job->dl, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_MINOR_ID:
				if(i = fsm_stoi(&job->jid, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_INTR_COUNT:
				/*TODO: we should use push down automata to check tally of 
				  total association count and summation of individual counts*/
				if(i = fsm_stoi(&job->intr_count, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_JOB_INTR_BOND:

				if(i = fsm_job_intr_asco(&fsm_iterator->table[fsm_iterator->idx],
							job) ) {
#ifdef DEBUG
					printf("job bond total width:%d\n", i);
#endif
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_WCET:
				if(i = fsm_stoi(&job->wcet, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_JOB_NXT:
			{
				struct ss_container *t;

				if(!(t = malloc(sizeof(struct ss_container)) )) {
					fprintf(stderr, "%s:Failed to malloc container\n", __FUNCTION__);
					fsm_iterator->ret_value = -1;
					break;
				}
				t->data = job;
#ifdef DEBUG
				job_print(t);
#endif
				t->nxt = fsm_iterator->data->task;
				fsm_iterator->data->task = t;
				job = NULL;
			}
			break;

			case STATE_JOB_END:

				if(-1 == fsm_iterator->ret_value) {
					free(job);
				}
				exit = 1;

				break;
			default:
				exit = 1;
				break;
		}
		state = job_decide_nxt(fsm_iterator, state);
	}
	return 0;
}

int fsm_job_traversal_finish(struct fsm* fsm_iterator){

	int i;

	if( -1 == (i = fsm_check_header_consistency(JOB_END, &fsm_iterator->table[fsm_iterator->idx]) ))
	{
		fprintf(stderr, "%s: FAILED JOB_END\n", __FUNCTION__);
		fsm_iterator->ret_value = -1;
		return 0;
	}
	fsm_iterator->idx += i;

	return 0;
}

unsigned int int_decide_nxt(struct fsm* fsm_iterator, unsigned int curr) {

	unsigned int state_t = curr;

#ifdef DEBUG
	printf("%s, curr state: %d, %d\n", 
			__FUNCTION__, curr, STATE_INT_NXT);
#endif

	do {

		if(-1 == fsm_iterator->ret_value) {
			fprintf(stderr, "%s: Failed,job state(%d)\n", 
					__FUNCTION__, state_t);
			break;
		}

		switch(state_t) {

			case STATE_INT_START:
			case STATE_INT_TASK_COUNT:
			case STATE_INT_JOBS_TRAVERSAL:
				curr++;
				return curr;

			case STATE_INT_NXT:
				{
					int i = fsm_iterator->idx;

					if( '\n' != fsm_iterator->table[i+1] || '\r' != fsm_iterator->table[i]) {
						fprintf(stderr, "%s: parsing failed Wrong EOL\n",__FUNCTION__);
						break;
					}
					else {
						i += EOL_LENGTH;
						if(DATA_SEPERATOR_DELIMITER == fsm_iterator->table[i]){
							curr = STATE_INT_END;
							fsm_iterator->ret_value = 0;
						}
						else {
							curr = STATE_INT_START;
						}
					}
					fsm_iterator->idx += EOL_LENGTH;
					return curr;
				}

			case STATE_INT_END:
				return curr;

			default:
				{
					if(PRIMARY_DELIMITER != fsm_iterator->table[fsm_iterator->idx]) {
						fprintf(stderr, "%s: parsing Failed, wrong delimiter: curr state: %d\n", 
								__FUNCTION__, curr);
						break;
					}
					curr++;
					fsm_iterator->idx++;
					return curr;
				}
		}

	} while(0);

	curr = STATE_INT_END;
	fsm_iterator->ret_value = -1;
	return curr;
}

int fsm_int_traversal_start(struct fsm* fsm_iterator) {

	struct ss_intr *interval;
	unsigned int state;
	int exit = 0;
	int i;

	if( -1 == (i = fsm_check_header_consistency(INTERVAL_START, &fsm_iterator->table[fsm_iterator->idx]))) {
		fsm_iterator->ret_value = -1;
		return 0;
	}

	fsm_iterator->idx += i;
	state = STATE_INT_START;

	while(!exit) {

		switch(state){

			case STATE_INT_START:
				// temporary solution needs Stack implementation
				if( !(interval = malloc(sizeof(struct ss_intr) + sizeof(struct ss_task)))) {
					fprintf(stderr,"%s:Failed to malloc interval\n", __FUNCTION__);
					fsm_iterator->ret_value = -1;
					exit = 1;
				}
				break;

			case STATE_INT_CORE_ID:
				if(i = fsm_stoi(&interval->core, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;
			case STATE_INT_ID:
				if(i = fsm_stoi(&interval->intr_id, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_INT_END_TIME:
				if(i = fsm_stoi(&interval->end, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_INT_SC:
				if(i = fsm_stoi(&interval->sc, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_INT_START_TIME:
				if(i = fsm_stoi(&interval->start, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_INT_TASK_COUNT:
				if(i = fsm_stoi(&interval->no_of_tsk, &fsm_iterator->table[fsm_iterator->idx])) {
					fsm_iterator->idx += i;
					break;
				}
				fsm_iterator->ret_value = -1;
				break;

			case STATE_INT_NXT:
				{
					struct ss_container *t;

					if(!(t = malloc(sizeof(struct ss_container)) )) {
						fprintf(stderr, "%s:Failed to malloc container\n", __FUNCTION__);
						fsm_iterator->ret_value = -1;
						break;
					}
#if 1 //def DEBUG
					printf("int: core:%d, id: %d, no_jobs: %d, start: %d, end: %d, sc: %d\n",
							interval->core, interval->intr_id, 
							interval->no_of_tsk, 
							interval->start, 
							interval->end, 
							interval->sc );
#endif
					t->data = interval;
					t->nxt = fsm_iterator->data->interval;
					fsm_iterator->data->interval = t;
					interval = NULL;
				}
				break;

			case STATE_INT_END:

				if(-1 == fsm_iterator->ret_value) {
					free(interval);
				}
				exit = 1;

				break;

			case STATE_INT_JOBS_TRAVERSAL:
				break;

			default:
				exit = 1;
				break;
		}

		state = int_decide_nxt(fsm_iterator, state);
	}
	return 0;
}

int fsm_int_traversal_finish(struct fsm* fsm_iterator){

	int i;

	i = fsm_check_header_consistency(INTERVAL_END, 
			&fsm_iterator->table[fsm_iterator->idx]);
	if(-1 ==  i)
	{
		fprintf(stderr, "%s: FAILED INTERVAL END: offset: %d\n", 
				__FUNCTION__, (fsm_iterator->idx + 7));
		fsm_iterator->ret_value = -1;
		return 0;
	}
	fsm_iterator->idx += i;

	return 0;
}


int fsm_hp_end(struct fsm* fsm_iterator){

	int i;

	i = fsm_check_header_consistency(HP_END, 
			&fsm_iterator->table[fsm_iterator->idx]);
	if(-1 == i)
	{
		fprintf(stderr, "%s: FAILED HP_END\n", __FUNCTION__);
		fsm_iterator->ret_value = -1;
		return 0;
	}
	fsm_iterator->idx += i;

	return 0;
}

int fsm_end(struct fsm* fsm_iterator) {

	fclose(fsm_iterator->fd);

	/* failure in some state, do cleaning and return -1*/
	/*
	 * cleaning procedure:
	 * 1. clear all malloced data
	 * 2. return -1
	 */
	if(fsm_iterator->ret_value == -1) {

		enum temp_state {
			s_exit = 0,
			s_task,
			s_interval,
		};
		enum temp_state iterate = s_interval;
		struct ss_data *j = fsm_iterator->data;
		struct ss_container *k = j->interval;
		struct ss_container *i = NULL;

		while(iterate){
			/* Sequence maintainer for freeing*/
			if(!k) {
				iterate--;
				if(iterate) {
					k = j->task;
				}
				continue;
			}

			i = k;
			k = k->nxt;
			free(i->data);
			free(i);
		}
		free(j);
		j = NULL;
		fsm_iterator->ret_value = -1;
		return 0;
	}

	/*TODO: use pushdown automata for semantic check the jobs and intervals */
	fsm_iterator->ret_value = 1;

	return 0;
}

int fsm_transition_fn(struct fsm* fsm_iterator){

	int i;

	switch (fsm_iterator->curr_state) {

		case STATE_HP_END:
			fsm_iterator->prev_state = fsm_iterator->curr_state;
			fsm_iterator->curr_state = STATE_HP_START;
		break;

		case STATE_END_STATE:
			return fsm_iterator->ret_value;

		default:
			fsm_iterator->prev_state = fsm_iterator->curr_state;
			fsm_iterator->curr_state = ++fsm_iterator->curr_state;
		break;
	}

	i = fsm_iterator->ret_value;
	if(i) {
		if( i == -1){
			fsm_iterator->curr_state = STATE_END_STATE;
			return 0;
		}
		else {
			return 1;
		}
	}

	return 0;
}

int fsm_init(const char* path, struct fsm *fsm_iterator){

	fsm_iterator->curr_state = STATE_HP_START;
	fsm_iterator->prev_state = STATE_HP_START;

	fsm_iterator->state[STATE_HP_START] = fsm_hp_start;
	fsm_iterator->state[STATE_JOB_TRAVERSAL_START] = fsm_job_traversal_start;
	fsm_iterator->state[STATE_JOB_TRAVERSAL_FINISH] = fsm_job_traversal_finish;
	fsm_iterator->state[STATE_INT_TRAVERSAL_START] = fsm_int_traversal_start;
	fsm_iterator->state[STATE_INT_TRAVERSAL_FINISH] = fsm_int_traversal_finish;
	fsm_iterator->state[STATE_HP_END] = fsm_hp_end;
	fsm_iterator->state[STATE_END_STATE] = fsm_end;

	fsm_iterator->transition_function = fsm_transition_fn;

	fsm_iterator->ret_value = 0;

	if (NULL == (fsm_iterator->fd = fopen(path, "r"))){
		fprintf(stderr, "%s: Failed to open the file: %s\n", __FUNCTION__, path);
		return -1;
	}
	fsm_iterator->table = NULL;
	fsm_iterator->idx = 0;
	if( !(fsm_iterator->data = malloc(sizeof(struct ss_data) ) ) ) {
		fprintf(stderr, "%s: Failed to alloc ss_data\n", __FUNCTION__);
		return -1;
	}
	fsm_iterator->data->interval = NULL;
	fsm_iterator->data->task = NULL;

	return 0;
}


struct ss_data* parsing_main(char *offline_schedule_path){

	static struct fsm fsm_iterator;
	int exit = 0;

	if(NULL == offline_schedule_path){
		fprintf(stderr, "%s:null parameter", __FUNCTION__);
		return NULL;
	}

	if(fsm_init(offline_schedule_path, &fsm_iterator) < 0)
		return NULL;

	while(!exit){
		fsm_iterator.state[fsm_iterator.curr_state](&fsm_iterator);
		exit = fsm_iterator.transition_function(&fsm_iterator);
	}

	return (fsm_iterator.data);
}
