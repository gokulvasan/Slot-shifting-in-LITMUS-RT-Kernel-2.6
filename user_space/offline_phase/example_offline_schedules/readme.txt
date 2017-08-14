###########################################################
# The structure of the offline schedule config files is:  #
###########################################################

LCM:<PeriodOfSchedule>;
Cores:<>numberOfCoresAvailable>;
Tasks:<numberOfTasksInThisFile>;
Task:<taskId>,<coreNumber>,<earliestStartTime>,<worstCaseExecutionTime>,<deadline>,[<type>];

No spaces are used, "," is used as separator for now.
Make sure to define constants for "LCM", "Cores", "Tasks", "Task" as well as the
separator token to allow for easy changes in the code in the future.
<type> is an optional parameter, should be 1 if not set.


Schedules 000-017: only one core available
Schedules 100-117: use a single core, but two are available!
Schedules 200-217: use 2 out of 2 cores, do the same on each core
Schedule      300: uses 2 out of 3 cores, does the same on core 1 and 2
Schedule      305: uses 2 out of 5 cores, does the same on core 1 and 2
Schedules 301-304 + 306-317: use 2 out of 4 cores, do the same on core 1 and 2

The syntax of all schedules should be correct, but the following schedules
are not feasible thus resulting in an error when applying the offline phase
of slot shifting:
009, 107, 209, 309, 010, 110, 210, 310, 017, 117, 217, 317
