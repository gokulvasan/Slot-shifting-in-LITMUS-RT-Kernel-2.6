#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "config.h"
#include "fsm_parser.h"

struct ss_data *parse_and_check_tables(struct configuration *conf)
{
	assert(conf);
	assert(conf->tpars);
	assert(conf->tpars->parse_table);

	return conf->tpars->parse_table(conf);
}


int main (int argc, char *argv[])
{
	/* Reads command line, as well as .conf file */
	struct ss_data *inject_data = NULL;
	int ret = -1;

	do {
		/* Parse file */
		if(!(inject_data = parsing_main("./temp")) ) {
			fprintf(stderr, 
				"ERROR:%s: Parsing failed\n", 
				__FUNCTION__);
			break;
		}
		
		ss_inj_update_res_id(1234);

		if(ss_job_to_task(&inject_data->task) ) {
			fprintf(stderr, 
				"ERROR %s: task bonding failed\n", 
				__FUNCTION__);
			break;
		}	
		
		
		//TODO: create reservation here.
	
		if(ss_intr_task_bond(inject_data->task, inject_data->interval)) {
			fprintf(stderr, 
				"ERROR: %s: interval bonding failed\n",
				 __FUNCTION__);
			break;
		} 
		ret = 0;
		printf("INJECTION_SUCCESS\n");
		/*TODO: release all tasks */
	} while(0);
	
	return ret;
}


