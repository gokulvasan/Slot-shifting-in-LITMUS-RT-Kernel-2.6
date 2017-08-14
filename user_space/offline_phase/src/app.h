#ifndef APP_H
#define APP_H

#include <cstdio>  // for fopen, etc.
#include <string>
#include <cstdlib>
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "task.h"
#include "interval.h"

//#include "pg.h"
//#include "edge.h"
//#include "msg.h"


//#define DEBUG



#ifdef DEBUG
#define dout cout
#else
#define dout 0 && cout
#endif

#define TASKTYPE_PERIODIC 1
#define TASKTYPE_APERIODIC 2
#define CORE_COUNT_START 1



#define SEPERATOR ','
#define EOL  ';'

using namespace std;



typedef std::map<unsigned, Task*> tTaskMap;
typedef tTaskMap::iterator tItrTaskMap;

typedef std::vector<Interval*> tIntervalVector;
typedef tIntervalVector::iterator tItrIntervalVector;

//index gives the core ; item give a vector if intervals
typedef std::map<unsigned, tIntervalVector*> tIntervalMap;
typedef tIntervalMap::iterator tItrIntervalMap;

typedef std::vector<Task*> tTaskVector;
typedef tTaskVector::iterator tItrTaskVector;
typedef std::map<int,tTaskVector*> tTaskVectMap;

//store the index of task in the task array for every core
typedef std::map<int,std::vector<int>* > tTaskndexVectMap;


typedef std::pair<Task*,int> tTaskIndexPair;
typedef std::vector<tTaskIndexPair> tTaskIndexPairVec ;
typedef std::vector<int> tTaskIndexVec ;

class App
{
        public:

		App();
                ~App();


                /** Main functions **/
                //! entry function
                unsigned extractArguments(int argc, char** argv);
                void parseScheduleFile();
                unsigned doCalculations();




                bool markInterval(tTaskVector* pTaskVector, unsigned core); // what does it do???


                map<unsigned, tIntervalVector*>* getIntervalMap();
                unsigned getCoreCount() const;
                unsigned getLcm() const;
                unsigned getSlotsize() const;
                unsigned getTaskNumOnCore(unsigned core);
                unsigned getIntervalNumOnCore(unsigned core);
                unsigned getMaxIntervalCount();
                unsigned getMaxTaskCount();
                unsigned getMaxAperiodicTaskCount();// returns maximum no of paeriodics on any core (not their sum)
                unsigned getAperiodicTaskCount(); //returns number of all aperiodics (i.e. sum of all on all cores)

                unsigned getMaxPeriodicTaskCount();

                //! returns the interval id to which the task belongs to
                int getIntervalID(unsigned tId,unsigned coreId);
                const string& getIpFileName() const { return m_inputFileName ;};
                const string& getOpFileName() const { return m_outputFileName ; };
                void sortTaskVector(tTaskVector* ptempTaskVector);



                tIntervalMap* getInterVectMap();
                vector<Task*>* getTaskVector(int coreID);
                //returns the vector of task type PERIODIC
                vector<Task*>* getPeriodicTaskVector(int coreID);
                tTaskIndexVec* getAperiodicsVector(int CoreId);
                tTaskIndexVec* getPeriodicsVector(int CoreId);



        private:


                //char* m_resultTxtFileName;
                string m_inputFileName;
                string m_outputFileName;
                //FILE* m_resultTxtFile;   //human readable output
                FILE* m_inputFile;
                FILE* m_outputFile;      // .c file output for scheduler

                unsigned m_slotsize;
                unsigned m_numberOfCores;
                unsigned m_lcm;
                unsigned m_noTask;

                tTaskVector m_taskVector;


                tTaskVectMap m_taskVectMap;			 // contains vectors of tasks
                tTaskVectMap m_periodicTaskVectMap;  // contains only periodic tasks
                // contains index of aperiodic  in the task array for each core
                tTaskndexVectMap m_aperiodicIndexVectMap;
                // contains index of periodic  in the task array for each core
                tTaskndexVectMap m_periodicIndexVectMap;


                tIntervalMap m_intervalVectMap;  //map with index =core and element as vector of Interval pointers




                //! create a taskMap with tasks ordered according to coreIDs
                void createTaskVectorMap();
                //! form a task vector with only task of type PERIODIC
                void createPeriodicTaskVectorMap();
                //! error reporting function
                void freakOut(unsigned arg);
                //! calculate the spare capacity in the intervals
                void calculateSpareCapacities(unsigned core);

                //! reorder interval ID with id 0 given to first interval in timeline
                void reorderIntervalId(tIntervalVector* pIVect);


                void dumpTaskList(tItrTaskMap itr,unsigned int size) const;
                void dumpTaskMap(map<unsigned, Task*>* pMap);
                void dumpTaskVector(tTaskVector* pTaskVector);
                //! dump intervals to console
                void dumpIntervalsFromCore(unsigned int coreId);
                void dumpAllIntervals();
                void dumpIntervals(tIntervalVector* intervalVecPtr);


                void correctStartOfIntervals(unsigned  coreId);
                bool isFreeSpace(tItrTaskVector itr,tItrIntervalVector itrPreviousInterval) const;


                tItrIntervalVector insertInterval(unsigned IntID, unsigned core,
												  unsigned start, unsigned end,
												  tItrIntervalVector itrIntervalVect,
												  tIntervalVector* pIntervalVector);


                bool predicateFn(Task* a , Task* b );




                //! returns the end of line [marked by ;]
                int getEndOfLine(char* ptr) ;
                //! cleans up the ip character array and put to op
                int purgeMe(char* ip,char* op,int ipsize) ;    // remove comments from array created from reading input file

                int readHeader(char* pPurgedArray,int sizeOfArray);

                //stores the index of aperiodics in m_aperiodicIndexVectMap for all cores
                void createAperiodicTaskVectorMap();
                //stores the index of periodics in m_periodicIndexVectMap for all cores
                void createPeriodicIndexVectorMap();

                void sortAndRenameTasks();

            	//sort based on est of task[second item in the pair]
                void sortTaskIndexPairVecOnEst(tTaskIndexPairVec* );
};
#endif
