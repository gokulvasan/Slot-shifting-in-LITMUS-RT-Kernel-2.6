/**
==========================================================================================
Creation Date: 20.07.2011
Author: Stefan Schorr
==========================================================================================


Purpose:
========
This file will use the following inputs to calculate the outputs below which are needed
to run the Slot Shifting algorithm.
Values will be parsed from txt-file and results will be printed to file results_prep.txt
AND (finally) parsed to .c code for execution.

Inputs:
--------
- number of tasks
- Offline scheduling table: fixed start and end times for each job
- LCM (implicitly given since equal to length of offline scheduling table)
- (earliest start time (=offset), period, WCET, deadline of tasks)

Outputs:
--------
- start and end points of scheduling intervals
- spare capacities of each interval

**/




// compile with -lrt option (in section libraries)

#include "app.h"
#include "outFileGenerator.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

using namespace std;





unsigned long timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
  return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) -
           ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}





int main (int argc, char** argv)
{
        struct timespec start, end;
        unsigned long timeElapsed;

        clock_gettime(CLOCK_MONOTONIC, &start);

        App* myApp = new App();



        myApp->extractArguments(argc, argv);

        myApp->parseScheduleFile();

        myApp->doCalculations();

        if(myApp->getSlotsize() != 1)
        {
        	OutFileGenerator* pOutFileGen = new OutFileGenerator(myApp);

        	pOutFileGen->setFileName(myApp->getOpFileName());

        	pOutFileGen->generateOutFile();

        	delete pOutFileGen;
        }
        else
        {
        	OutTxtFileGenerator* pOutTxtFileGen = new OutTxtFileGenerator(myApp);

        	pOutTxtFileGen->setFileName(myApp->getOpFileName());

        	dout << "GetMaxAperiodicTaskCount() " << myApp->getMaxAperiodicTaskCount() << endl;

        	pOutTxtFileGen->dummyWrite();
        }

        delete myApp;

        clock_gettime(CLOCK_MONOTONIC, &end);
        timeElapsed = timespecDiff(&end, &start);
        if(timeElapsed > 5000000000)
        	printf("time elapsed: %4.4f secs (%ld ns)\n", (float)timeElapsed/1000000000, timeElapsed);

        return 0;
}
