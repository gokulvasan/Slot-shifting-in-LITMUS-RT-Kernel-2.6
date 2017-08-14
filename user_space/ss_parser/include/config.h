#ifndef SS_PARSER_CONF_H
#define SS_PARSER_CONF_H

#include "ss_data.h"

struct configuration;

/* Functions in struct conf_parse */
typedef void (*conf_parse_t) (struct configuration *conf);

struct conf_parse {
	conf_parse_t parse_conf;
};

/* Functions in struct table_parse */
typedef struct ss_data *(*table_parse_t) (struct configuration *conf);

struct table_parse {
	table_parse_t parse_table;
};


/* path of the binary that is used to inject the job and interval */
/* we use system() function call to call these binaries*/
struct krnl_inject {
	char job_inject[PATH_MAX];
	char interval_inject[PATH_MAX];
	int (*inject_fn)(char *);
};

struct configuration {
	char *table_file_path;
	char *conf_file_path;
	int use_rtspin;

	struct conf_parse *cpars;
	struct table_parse *tpars;
	struct krnl_inject *ki;
};


void usage(char *error);

/* Configures the behavior of the parser. */
struct configuration *configure_behavior(int argc, char *argv[]);

#endif

