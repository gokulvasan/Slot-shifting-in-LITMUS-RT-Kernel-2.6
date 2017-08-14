#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <signal.h>
#include <execinfo.h>


jmp_buf latest_exception_buf;
#define TRY jmp_buf _exception_buf;if((setjmp(_exception_buf) == 0) && memcpy(latest_exception_buf, _exception_buf,sizeof(jmp_buf)))
#define CATCH else
#define THROW(s) {puts(s);longjmp(_exception_buf, 1);}
#define JUMPBACK(s) {puts(s);longjmp(latest_exception_buf, 1);}
//#define DEBUG 
#ifdef  DEBUG 
#define dprint dmesg
#else 
#define dprint 
#endif

#define GEN_DATA_LEN 8

#define MALLOC(variable,type,size) if((variable = (type*)malloc(sizeof(type)*size))==NULL)\
                                       {dprint("Error:No Heap Memory");THROW("NOMEM")}\
                                   else{memset(variable,'\0',size);}

//static jmp_buf exception_buf;

const char* delim=";";
int genData[GEN_DATA_LEN];
unsigned *g_number_of_tasks_on_core; 
unsigned *g_number_of_intervals;
unsigned max_intervals_on_any_core = 0;
FILE *fp = NULL;
struct tTask{
        int uid;                                // unique ID for each task (as in original input file)
        unsigned arrival_time, wcet, remaining, deadline;         
        short int type;                           // determines whether offline guaranteed task (1) or aperiodic (2) or aperiodic already accepted (3) or aperiodic rejected (4) finished execution (99)
        short unsigned core_id;
        struct tInterval* interval_ptr;           
 };
struct tInterval
{
    unsigned uid;
    unsigned start, end;
   short int spare_capacity;                                      
   struct tInterval* prev;                                        
   struct tInterval* next;                                        
    unsigned number_of_tasks;                                     
   struct tTask* *tasks; // entries in this array contain task ids. length = maxTaskPerInterval
}; 

struct tInterval **g_interval_array;
struct tTask **g_task_array;
struct tTask* **g_aperiodics_index;
struct tTask* **g_periodics_index;

unsigned int* g_number_of_aperiodics;
unsigned int* g_number_of_periodics;
enum{ maxCores = 0,
      maxTime,
      maxTasksPerCore,
      maxIntervalsPerCore,
      maxNumofTaskPerInterval,
      maxTasksPerInterval,
      maxAperiodicsOnAnyCore,
      maxPeriodicsOnAnyCore 
    };



static void handler(int sig, siginfo_t *si, void *unused)
{
    //Print the trace
    void           *array[32];    /* Array to store backtrace symbols */
    size_t          size;     /* To store the exact no of values stored */
    char          **strings;    /* To store functions from the backtrace list in ARRAY */
    size_t          nCnt;

    printf("Got SIGNAL [%d] at address [%p]\n", sig,si->si_addr);
    printf("Reason:");
    switch (si->si_code) {
        case SEGV_MAPERR:
            printf("Address not mapped.\n");
            break;
        case SEGV_ACCERR:
            printf("Access to this address is not allowed.\n");
            break;
        default:
            printf("Unknown reason.\n");
            break;
    }

    printf("<==================TRACE START====================>\n");
    size = backtrace(array, 32);
    strings = backtrace_symbols(array, size);
    /* prints each string of function names of trace*/
    for (nCnt = 0; nCnt < size; nCnt++)
        fprintf(stderr, "%s\n", strings[nCnt]);
    printf("<==================TRACE END======================>\n");
    printf("EXITING PROGRAM\n");
    JUMPBACK("");
}

void dmesg(const char *fmt,...) {
    va_list args;
    fprintf(stderr,"\n");
    va_start(args, fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
    fprintf(stderr,"\n");
}

void error(const char *fmt,...) {
    va_list args;
    fprintf(stderr,"\nERROR:Mesg:");
    va_start(args, fmt);
    vfprintf(stderr,fmt,args);
    va_end(args);
    fprintf(stderr,"\n");
}

void printgeneralData(void){
    int i = 0;
    dprint("MAXCORE=%d,MAXTIME=%d,MAXTASKPERCORE=%d,MAXINTERVALSPERCORE=%d,MAXNUMOFTASKPERINTEVAL=%d,MAXTASKPERINTERVAL=%d,MAXAPERIODICONANYCORE=%d,MAXPERIODICONANYCORE=%d",genData[maxCores],genData[maxTime],genData[maxTasksPerCore],genData[maxIntervalsPerCore],genData[maxNumofTaskPerInterval],genData[maxTasksPerInterval],genData[maxAperiodicsOnAnyCore],genData[maxPeriodicsOnAnyCore]);
	
}

int fillGeneralData(char* line){  
    char *a = strtok(line,delim);
    int i;
    TRY{
        for(i = 0; a!=NULL; i++){
            genData[i] = atoi(a);
            a = strtok(NULL,delim);
        }
        //Initalize the global #defines which have now
        // been converted to global variables
        /*MAX_CORES = genData[maxCores];
        MAX_TIME  = genData[maxTime];
        MAX_TASKS_PER_CORE  = genData[maxTasksPerCore];
        MAX_INTERVALS_PER_CORE  = genData[maxIntervalsPerCore];
        MAXNUMOFTASK_PERINTERVAL = genData[maxNumofTaskPerInterval];
        MAX_TASKS_PER_INTERVAL   = genData[maxTasksPerInterval];
        MAX_APERIODICS_ON_ANY_CORE  = genData[maxAperiodicsOnAnyCore];
        MAX_PERIODICS_ON_ANY_CORE  = genData[maxPeriodicsOnAnyCore];
        printgeneralData();
	*/
    }
    CATCH{
        error("Error:Parse General Data");
        return -1;
    }
}

   

int fillAperiodics(char* line){
    char *a ;
    int i,count;
    TRY{
        MALLOC(g_number_of_aperiodics,unsigned,genData[maxCores]);
        a = strtok(line,delim);
        count = atoi(a);
        a = strtok(NULL,delim);
        dprint("c=%d",count);
        assert(count == genData[maxCores]);
        for(i = 0; i< count; i++){
            g_number_of_aperiodics[i] = atoi(a);
            a = strtok(NULL,delim);
        }

        dprint("g_number_of_aperiodics[");
        for(i = 0;i < count; i++){
            dprint("%d,",g_number_of_aperiodics[i]);
        }
        dprint("]\n");
    }
    CATCH{
        error("Error:fillAperiodics");
        return -1;
    }
}

int fillPeriodics(char* line){
    char *a ;
    int i,count;
    TRY{
        MALLOC(g_number_of_periodics,unsigned,genData[maxCores]);
        a = strtok(line,delim);
        count = atoi(a);
        a = strtok(NULL,delim);
        dprint("c=%d",count);
        assert(count == genData[maxCores]);
        for(i = 0; i< count; i++){
            g_number_of_periodics[i] = atoi(a);
            a = strtok(NULL,delim);
        }

        dprint("g_number_of_periodics[");
        for(i = 0;i < count; i++){
            dprint("%d,",g_number_of_periodics[i]);
        }
        dprint("]\n");
    }
    CATCH{
        error("Error:fillPeriodics");
        return -1;
    }
}

int fillPeriodicsIndex(){
    //struct tTask* g_periodics_index[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE]
    char *a, sbrace, comma, crlbrace;
    int i=0, count, val, j=0, k=0;
    TRY{
        // Format = {[13,16,19,24,36,35,59,63,86,],[0,7,13,20,31,36,38,37,40,],[6,11,18,19,22,33,61,76,77,],[12,18,30,45,52,66,79,99,102,],}
        // Allocate the 2D array g_task_array[MAX_CORES][MAX_PERIODICS_ON_ANY_CORE]
        MALLOC(g_periodics_index, struct tTask**, genData[maxCores]);
        for (i = 0; i < genData[maxCores]; i++){
            MALLOC(*(g_periodics_index+i), struct tTask*, genData[maxPeriodicsOnAnyCore]);
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='{'); //BEGIN
	
        for(i = 0; i < genData[maxCores] ; i++){
            fscanf(fp,"%c",&sbrace);
            assert(sbrace=='[');
            for (j = 0; j < genData[maxPeriodicsOnAnyCore]; j++){
                fscanf(fp,"%d%c",&val,&comma);
                g_periodics_index[i][j] = (struct tTask*)val;
            }
	    fscanf(fp,"%c%c",&sbrace,&comma);
	    assert(sbrace==']');
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='}'); // END
    }
    CATCH{
        error("Error:fillPeriodicsIndex");
        return -1;
    }
}

int fillAperiodicsIndex(){
    //struct tTask* g_aperiodics_index[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE]
    char *a, sbrace, comma, crlbrace;
    int i=0, count, val, j=0, k=0;
    TRY{
        // Format = {[13,16,19,24,36,35,59,63,86,],[0,7,13,20,31,36,38,37,40,],[6,11,18,19,22,33,61,76,77,],[12,18,30,45,52,66,79,99,102,],}
        // Allocate the 2D array g_task_array[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE]
        MALLOC(g_aperiodics_index, struct tTask**, genData[maxCores]);
        for (i = 0; i < genData[maxCores]; i++){
            MALLOC(*(g_aperiodics_index+i), struct tTask*, genData[maxAperiodicsOnAnyCore]);
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='{'); //BEGIN
	
        for(i = 0; i < genData[maxCores] ; i++){
            fscanf(fp,"%c",&sbrace);
            assert(sbrace=='[');
            for (j = 0; j < genData[maxAperiodicsOnAnyCore]; j++){
                fscanf(fp,"%d%c",&val,&comma);
                g_aperiodics_index[i][j] = (struct tTask*)val;
            }
	    fscanf(fp,"%c%c",&sbrace,&comma);
	    assert(sbrace==']');
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='}'); // END
    }
    CATCH{
        error("Error:fillAperiodicsIndex");
        return -1;
    }
}

int fillTasksOnCore(char* line){
    char *a ;
    int i,count;
    TRY{
        MALLOC(g_number_of_tasks_on_core,unsigned,genData[maxCores]);
        a = strtok(line,delim);
	count = atoi(a);
        a = strtok(NULL,delim);
        dprint("c=%d",count);
        assert(count == genData[maxCores]);
        for(i = 0; i< count; i++){
            g_number_of_tasks_on_core[i] = atoi(a);
            a = strtok(NULL,delim);
        }
        
        printf("g_number_of_tasks_on_core->[");
        for(i = 0;i < count; i++){
            printf("%d,",g_number_of_tasks_on_core[i]);
        }
        printf("]\n");
    }
    CATCH{
        error("Error:fillTasksOnCore");
        return -1;
    }
}

int fillNumberOfIntervals(char* line){
    char *a ;
    int i,count;
    TRY{
        MALLOC(g_number_of_intervals,unsigned,genData[maxCores]);
        a = strtok(line,delim);
	count = atoi(a);
        a = strtok(NULL,delim);
        assert(count == genData[maxCores]);
        for(i = 0; i< count; i++){
	    printf("XXX=ATOI=[%d]",atoi(a));
	    if( atoi(a) > max_intervals_on_any_core ){
		    max_intervals_on_any_core = atoi(a);
	    }
            g_number_of_intervals[i] = atoi(a);
            a = strtok(NULL,delim);
        }
        
        dprint("g_number_of_intervals->[");
        for(i = 0;i < count; i++){
            dprint("%d,",g_number_of_tasks_on_core[i]);
        }
        dprint("]\n");
    }
    CATCH{
        error("Error:fillNumberOfIntervals");
        return -1;
    }
}

void printIntervalArray(void){
    int i=0, j =0;
    for (i = 0; i < 1; i++){
        printf("\n");
        for (j = 0; j < 2; j++){
            printf("uid=%d\t",g_interval_array[i][j].uid);
            printf("start=%d\t",g_interval_array[i][j].start);
            printf("end=%d\t",g_interval_array[i][j].end);
            printf("spare_capacity=%d\t",g_interval_array[i][j].spare_capacity);
            printf("prev=%d\t",g_interval_array[i][j].prev);
            printf("next=%d\t",g_interval_array[i][j].next);
            printf("number_of_tasks=%d\t",g_interval_array[i][j].number_of_tasks);
            printf("\n");
       }
    }
}

int fillTaskArray(void){
    char *a, sbrace, comma, crlbrace;
    int i=0, count, val, j=0, k=0;
    TRY{
        // Format = [0,0,13,13,23,1,1,0],[44,0,10,10,28,1,1,1],[1,23,13,13,46,1,1,2],
        // struct tTask g_task_array[MAX_CORES][MAX_TASKS_PER_CORE]
        // Allocate the 2D array g_task_array[MAX_CORES][MAX_TASKS_PER_CORE]
        MALLOC(g_task_array, struct tTask*, genData[maxCores]);
        for (i = 0; i < genData[maxCores]; i++){
            MALLOC(*(g_task_array+i), struct tTask, genData[maxTasksPerCore]);
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='{'); //BEGIN
	
        for(i = 0; i < genData[maxCores] ; i++){
            for (j = 0; j < genData[maxTasksPerCore]; j++){
                fscanf(fp,"%c",&sbrace);
                assert(sbrace=='[');
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].uid = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].arrival_time = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].wcet = val;
    
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].remaining = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].deadline = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].type = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_task_array[i][j].core_id = val;

                fscanf(fp,"%d%c",&val,&sbrace);
                g_task_array[i][j].interval_ptr = (struct tInterval*)val;
    
                fscanf(fp,"%c",&comma);
                assert(sbrace==']');
            }
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='}'); // END
    }
    CATCH{
        error("Error:fillTaskArray");
        return -1;
    }
    //printTaskArray();
}


int fillIntervalArray(void){
    char *a, sbrace, comma, crlbrace;
    int i=0, count, val, j=0, k=0;
    TRY{
        // Format = [0,0,23,5,0,0,1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1],
        // struct tInterval g_interval_array[MAX_CORES][MAX_INTERVALS_PER_CORE]
        // Allocate the 2D array g_interval_array[MAX_CORES][MAX_INTERVALS_PER_CORE]
        MALLOC(g_interval_array, struct tInterval*, genData[maxCores]);
        for (i = 0; i < genData[maxCores]; i++){
            MALLOC(*(g_interval_array+i), struct tInterval, genData[maxIntervalsPerCore]);
        }
	for (i = 0; i < genData[maxCores] ; i++){
            for (j = 0; j < genData[maxIntervalsPerCore]; j++){
                MALLOC(g_interval_array[i][j].tasks, struct tTask*, genData[maxTasksPerInterval]);
	    }
	}
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='{'); //BEGIN
	printf("D2=%d\n",genData[maxIntervalsPerCore]);
	printf("A=[%d] B[%d]\n",genData[maxCores],max_intervals_on_any_core);
        for(i = 0; i < genData[maxCores] ; i++){
            for (j = 0; j < max_intervals_on_any_core; j++){
                fscanf(fp,"%c",&sbrace);
                assert(sbrace=='[');
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].uid = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].start = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].end = val;
    
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].spare_capacity = val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].prev = (struct tInterval*)val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].next = (struct tInterval*)val;
    
                fscanf(fp,"%d%c",&val,&comma);
                g_interval_array[i][j].number_of_tasks = val;
    
                //next we scan an array of size maxTaskPerInterval of type "struct tTask*"
                //and then we fill this array
                for(k = 0; k < genData[maxTasksPerInterval]; k++){
                    // At the end of the loop we will scan a ']' as comma (eg. [1,2,3,4])
                    // but that does not matter.
                    fscanf(fp,"%d%c",&val,&comma);
                    dprint("&val=%d",val);
                    g_interval_array[i][j].tasks[k] = (struct tTask*)val;
                }
                fscanf(fp,"%c",&comma);
                assert(comma==',');
            }
        }
        fscanf(fp,"%c",&crlbrace);
	assert(crlbrace=='}'); // END
        //printIntervalArray();
    }
    CATCH{
        error("Error:fillIntervalArray");
        return -1;
    }
}

int parseFile(const char* filename){
    const char* delim = ";";
    char newline;
    TRY{
        fp = fopen(filename,"r");
        char line[2048];
        char *a;
        if (fp == NULL){
            THROW("Error:could not open file")
        }
        fgets(line, 2048, fp);
        dprint(line);
        //Parse General Data
	if(fillGeneralData(line) == -1)
            THROW("");

        fgets(line, 2048, fp);
        puts(line);
        if(fillTasksOnCore(line) == -1)
            THROW("");

        fgets(line, 2048, fp);
        if(fillNumberOfIntervals(line) == -1)
            THROW("");

        if(fillIntervalArray() == -1)
            THROW("");

	fscanf(fp,"%c",&newline);//scan the new line so that we can use fgets below 
	assert(newline == '\n');

        if(fillTaskArray() == -1)
            THROW("");

	fscanf(fp,"%c",&newline);//scan the new line so that we can use fgets below 
	assert(newline == '\n');
	
        fgets(line, 2048, fp);
	if(fillAperiodics(line) == -1)
            THROW("");


	if(fillAperiodicsIndex() == -1)
            THROW("");

	fscanf(fp,"%c",&newline);//scan the new line so that we can use fgets below 
	assert(newline == '\n');
	
        fgets(line, 2048, fp);
	if(fillPeriodics(line) == -1)
            THROW("");

	if(fillPeriodicsIndex() == -1)
            THROW("");
	printf("\nPARSING SUCCESSFUL\n");
    }
    CATCH{
        error("\nEXCEPTION:Parse File");
    }
}

void initialize(void){
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    TRY{
        if (sigaction(SIGSEGV, &sa, NULL) == -1)
            THROW("-1 returned");
    }
    CATCH{
        error("\nEXCEPTION");
    }
}

int main(int argc,char **argv){
    initialize();
    parseFile(argv[1]);
    return 0;
}

