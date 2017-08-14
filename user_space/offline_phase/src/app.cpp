#include "app.h"



//#define STS_DEBUG1
//#define DEBUG_L1
//#define DEBUG_L2
#define STARTOFTIME 0
#define MAXSIZE_OF_FILE (1024 * 10)  //10kb

//---------------------------------------------------------------------------------------------
//! ctor
//---------------------------------------------------------------------------------------------
App::App() : m_numberOfCores(0),
			 m_lcm(0),m_noTask(0)
{
		//printf("starting App\n");
		//m_resultTxtFileName = "result_prep.txt\0";
}

//---------------------------------------------------------------------------------------------
//! free the memory allocated for task map and interval map
//---------------------------------------------------------------------------------------------
App::~App()
{

	dout << "~App\n";

	// free the memory allocated for task map and interval map
	tTaskVector* pTaskVec ;
	tTaskVector::iterator itr;
	int size = 0;
	for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
	{
		pTaskVec = m_taskVectMap[coreID];
		assert(pTaskVec!=0);
		itr = pTaskVec->begin();
		size = pTaskVec->size();
		for (int i = 0; i < size; i++, itr++)
			delete *itr;



		pTaskVec = m_periodicTaskVectMap[coreID];
		assert(pTaskVec!=0);
		itr = pTaskVec->begin();
		size = pTaskVec->size();
		for ( int i = 0; i < size; i++, itr++)
				delete *itr;


		tIntervalVector* pIntVector = m_intervalVectMap[coreID] ;

		tIntervalVector::iterator itrInterVect =(*pIntVector).begin() ;
		for(;itrInterVect!=(*pIntVector).end() ; itrInterVect++)
			delete (*itrInterVect) ;
		delete pIntVector;


	}


}
//---------------------------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------------------------

void App::freakOut(unsigned arg = 1000)
{
        switch(arg)
        {
                case 1:
                {
                        printf("Give correct commandline arguments: ./preparation <inputfile> <resulttxtfile> <slot size>\n<slotsize> is 5000-1000000\nAborting!\n");
                        exit(1);
                }
                case 2:
                {
                        printf("Commandline arguments error: <inputfile> matches <resulttxtfile>\nAborting!\n");
                        exit(2);
                }
                case 3:
                {
                        printf("Couldn't open input file\nAborting!\n");
                        exit(3);
                }
                case 4:
                {
                        printf("Problem in input file\nAborting!\n");
                        exit(4);
                }
                case 5:
                {
                        printf("Problem in input file: ID mismatch\nAborting!\n");
                        exit(5);
                }
                case 6:
                {
                        printf("task configuration error\nAborting!\n");
                        exit(6);
                }
                case 7:
                {
                        printf("Caramba! Cannot read/create successor map\nAborting!\n");
                        exit(7);
                }
                case 8:
                {
                        printf("problem setting deadlines for tasks\n");
                        exit(8);
                }
                case 9:
                {
                        printf("couldn't write to m_resultTxt file\n");
                        exit(9);
                }

                default:
                {
                        printf("Something unexpected happend!\n");
                        exit(1000);
                }
        }
}



//---------------------------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------------------------
unsigned App::extractArguments(int argc, char** argv)
{
	dout<<"-->>extractArguments" << '\n' ;
        if(argc == 4)
        {
        	if(atoi(argv[3]) < 5000 || atoi(argv[3]) > 1000000)
        		freakOut(1);
        	m_slotsize = atoi(argv[3]);
        	//cout<<"\t-> slot size is set to: " << m_slotsize << '\n';
        }
        else
        {
        	m_slotsize = 1;
        }

        m_inputFileName = argv[1];
        m_outputFileName = argv[2];

        dout<<"\tIp File  : " << m_inputFileName <<'\n' ;
        dout<<"\top File  :" << m_outputFileName <<'\n' ;
        if (m_inputFileName == m_outputFileName)
        	freakOut(2);

        dout<<"<<--extractArguments" << '\n' ;
        return 0;
}


//----------------------------------------------------------
//!
//----------------------------------------------------------

unsigned App::doCalculations()
{
	bool ret = false;

	//create task map from the taskVector which contains tasks belonging to all cores
	//In other words, store the task based on the cores it belongs to!
    createTaskVectorMap();
 	// sort task map according to dl and assign Ids based on dl
 	sortAndRenameTasks();
 	createPeriodicTaskVectorMap();




	tTaskVector* pTaskVector = 0;



	dout << "\tperiodic vectors for all cores created and sorted according to deadline\n\tlet's go\n";
	for (unsigned corecounter = CORE_COUNT_START; corecounter <= m_numberOfCores; corecounter++) // cores start with 1 !
	{
		//get filtered task vector for the core
		pTaskVector = getPeriodicTaskVector(corecounter);

		ret = markInterval(pTaskVector, corecounter);
		if (ret == false)
		{
			cerr << "App::doCalculations() -->  markInterval():  error\n";
			return 1;
		}

		//Validate the intervals
		correctStartOfIntervals(corecounter);


#ifdef DEBUG
		dout << "before spare capacity calculation" << '\n';
		dumpIntervalsFromCore(corecounter);
#endif
		dout << "calculating spare capacity" << '\n';
		calculateSpareCapacities(corecounter);
#ifdef DEBUG
		dout << "After calculating spare capacity" << '\n';
		dumpIntervalsFromCore(corecounter);
#endif
	}



	createAperiodicTaskVectorMap();
	createPeriodicIndexVectorMap();
	return 0;
}

//----------------------------------------------------------
//!
//----------------------------------------------------------
void App::parseScheduleFile()
{
	dout<<"-->>parseScheduleFile" << '\n' ;


		//working copy of ptr and base address of allocated array - needed to delete
        char* pRawBuf = 0 ;
        char*_pRawBuf = 0;
        char* pPurgedArray = 0;
        char* _pPurgedArray = 0;

        FILE* fp = fopen(m_inputFileName.c_str(), "r");
        if(fp == 0)
        {
        		cout << "file error: " << m_inputFileName << "\n";
                freakOut(3);
        }
        int eol = 0;

        // obtain file size:
        int sizeOfArray = 0;
        fseek(fp, 0, SEEK_END);
        sizeOfArray = ftell(fp);
        rewind(fp);

	   dout<<"\tinput file size: " << sizeOfArray << " bytes\n";



	   _pRawBuf =  new char[sizeOfArray];
	   if(_pRawBuf==0) { cerr << "\tApp::parseScheduleFile(): ERROR creating _pRawBuf\n"; exit(1); }

       //working copy of pointer.
       pRawBuf = _pRawBuf;
       //read the entire file to memory
       int sizeRead =  fread(pRawBuf,
							 sizeof(char),
							 sizeOfArray,
							 fp);

       dout<<"\tread " << sizeRead << " bytes\n";

       if (ferror (fp) || sizeRead != sizeOfArray )
       {
    	   perror ("\tSize read != size of file or, the following error occurred");
       }

       fclose(fp);


       _pPurgedArray = new char[sizeOfArray];
       if(_pPurgedArray==0) { cerr << "\tApp::parseScheduleFile(): ERROR creating _pPurgedArray\n"; exit(1); }
       //working copy
       pPurgedArray = _pPurgedArray ;


       //removes the comment,white spaces and other junk
       sizeOfArray = purgeMe(pRawBuf, pPurgedArray, sizeOfArray);


#ifdef DEBUG_L2
        for (int k=0;k<sizeOfArray;++k)
        {
        	printf("\t%x : " , pPurgedArray+k);
        	printf("\t%x -  %c \n" , *(pPurgedArray+k), *(pPurgedArray+k));

        }
#endif

        if(sizeOfArray <0 )  freakOut(10);


        //extract header data
         eol =   readHeader(pPurgedArray, sizeOfArray);

#ifdef DEBUG_L2

         for ( int m=0;m<sizeOfArray;++m)
         {
         	printf("%x : " , pPurgedArray+m);
         	printf("%x -  %c \n" , *(pPurgedArray+m), *(pPurgedArray+m));

         }
#endif


         //progress pointer to beginning of task descriptors
        pPurgedArray += eol ;
        int itr = eol;
        int count  = 0;
        int check_id = 0;

        int id = -1,est=999,dl=999,wcet=999,core = 990,type = TASKTYPE_PERIODIC;

        Task* pTask = 0;
        while((id + 1 < m_noTask) && (itr < sizeOfArray))
        {
        	//default values : note the default value of type.
        	id = -1,est=999,dl=999,wcet=999,core = 999,type = TASKTYPE_PERIODIC;
		    dout << "m_noTask" << m_noTask;

        	eol = getEndOfLine(pPurgedArray); //returns the end of row
        	count = sscanf(pPurgedArray,"%d,%d,%d,%d,%d,%d;",&id,&core,&est,&wcet,&dl,&type);
            
            if(id == check_id)
            {
                ++check_id;
            }
            else
            {
                cout << "ERROR: task ID mismatch! Task " << check_id << " has wrong id " << id << "\nIDs should start with 0!\n";
                exit(1);
            }
            if(core > m_numberOfCores)
            {
                cout << "ERROR: job " << id << " has wrong core id " << core << "\nmax core ID is " << m_numberOfCores << "!\n";
                exit(1);
            }

        	//minimum 5 parameters are required to define a task;
        	//if type is not defined,default value is assumed
        	if(count != 0)  // if line is empty ignore, else there is really something wrong
        	{
        		if(count < 5)
        		{
        			cout << "\t------Not enough parameters for task descriptor (" << itr << ") --- ABORTING \n";
				cout<<"READ: \tid "<<id<<"\tcore "<<core<<"\test "<<est<<"\twcet "<<wcet<<"\tdl "<<dl<<"\ttype "<<type<<'\n';
        			exit(1);
        		}

        		dout<<"\tid "<<id<<"\tcore "<<core<<"\test "<<est<<"\twcet "<<wcet<<"\tdl "<<dl<<"\ttype "<<type<<'\n';

        		//create new task with the read information
        		pTask = new Task(id, est, dl, wcet, core, type);
        		pTask->setUid(id);
        		//push to the task vector
        		m_taskVector.push_back(pTask);
        	}
            //progress pointer to beginning of next task descriptor
        	pPurgedArray += eol ; //next line
        	itr += eol;
        }
        if(check_id != m_noTask)
        {
            cout << "ERROR: input file contains wrong number of tasks!\nRead " << check_id<< " tasks\n";
            exit(2);
        }


        delete[] _pRawBuf ;
        delete[] _pPurgedArray ;

    	dout<<"<<--parseScheduleFile" << '\n' ;

        return;
}


//---------------------------------------------------------------------------
// STS: I think what it really does is:
// 		it goes through the TaskVectorMap core by core,
//          sorts all tasks according to deadline (first entry: latest deadline) and assigns new IDs (starting with 0 from the end of the vector)
//---------------------------------------------------------------------------
void App::sortAndRenameTasks()
{
	dout<<"		-->>sortAndRenameTasks\n";
	tTaskVector* pTaskVector = 0;

	dout << m_numberOfCores << "\n";
		for (unsigned coreID = 1; coreID <= m_numberOfCores; ++coreID) // cores start with 1 !
		{
			dout<< "\t\tCOREID = " << coreID << "\n";
			//get task vector for the core
			pTaskVector = getTaskVector(coreID);

			if (pTaskVector==0) return;
			sortTaskVector(pTaskVector);
#ifdef STS_DEBUG1
			//for every core show result
			dumpTaskVector(pTaskVector);
#endif
		}
    dout<<"		<<--sortAndRenameTasks\n";
}


//---------------------------------------------------------------------------------------------
//! read header information
//! header should contain
//			lcm;
//			NoOfCore;
//			NoOfTask;
//! @return : the end of header position
//---------------------------------------------------------------------------------------------

int App::readHeader(char* pPurgedArray,int sizeOfArray)
{
	dout<<"-->>readHeader" << '\n' ;
	int ret = 0;

	int lcm= 0,NoCore = 0,NoTask = 0,eol = 0;

	//reads the header info
	//header should contain
	//lcm;
	//NoOfCore;
	//NoOfTask;


	assert(pPurgedArray != 0);

	sscanf(pPurgedArray,"%d",&lcm );
	eol = getEndOfLine(pPurgedArray);
	pPurgedArray = pPurgedArray + eol ; //next line
	ret += eol ;

	sscanf(pPurgedArray,"%d",&NoCore );
	eol = getEndOfLine(pPurgedArray);
	pPurgedArray = pPurgedArray + eol ; //next line
	ret += eol ;

	sscanf(pPurgedArray,"%d",&NoTask );
	eol = getEndOfLine(pPurgedArray);
	pPurgedArray = pPurgedArray + eol ; //next line
	ret += eol ;


	m_numberOfCores = NoCore;
	m_lcm = lcm;
	m_noTask = NoTask ;


	dout<<"<<--readHeader" << '\n' ;
	return ret ;
}

//---------------------------------------------------------------------------------------------
//! @return :  the location of end of line [marked by ;]
//---------------------------------------------------------------------------------------------
int App::getEndOfLine(char* ptr)
{
	dout<<"-->>getEndOfLine" << '\n' ;


	int ret = 0;
	while(*ptr != ';')
	{

#ifdef DEBUG_L2
		printf("%x : " , ptr);
		printf("%x -  %c \n" , *ptr, *ptr);
#endif

		ret ++ ;
		++ptr;
	}

	dout<<"<<--getEndOfLine" << '\n' ;
		return ret+1 ; //returns the end of line
}


//---------------------------------------------------------------------------------------------
//! cleans up the ip buffer and put to op
//! extract the digits ,line separator and column  separator
//---------------------------------------------------------------------------------------------

int App::purgeMe(char* ip,char* op,int ipsize)
{
	dout<<"-->>PurgeMe" << '\n' ;
    unsigned opSize = 0;
    char* limit = ip + ipsize;
    //the minimum size of rawbuffer .
    const int MinSize = 5;
    if(ipsize<MinSize)
        return -1;

#ifdef DEBUG_L1
    printf("ip: %x   limit: %x : " , ip, limit);
#endif

    for(int i =0;i<ipsize && ip+1 < limit;)
    {
        //eat the comment
        if(ip+1 < limit &&
        		*ip == '/' &&
        		*(ip+1) == '*')//start of comment
        {
            while( !( (*(ip) == '*') && (ip+1 < limit) && (*(ip+1) == '/') ) )//end
            {

                //can happen if the the comment end[*/] is missing
                if(i>=ipsize)
                {
                    return -1;
                }
                ip++;
                i++;
            }
            ip++;

        }

        //dout<<*ip << '\n' ;
        //extract the digits ,line seperator and column  seperator
        //------<<0>>---------------------<<9>>------------<<:>>--<<;>>------
        if( ( ( 0x30<=(*ip) ) && ( (*ip)<=0x39 ) ) || (SEPERATOR ==*ip) || ( EOL==*ip) )
        {

            *op = *ip;
           // dout<<op << " : " << *op <<'\n' ;//<<
#ifdef DEBUG_L1

            printf("%x : " , op);
          	printf("%x -  %c" , *op, *op);
#endif
          	//if(*op == ';')
           // 	dout<<'\n';


            op++;
            opSize++;
        }
        ip++;
        i++;
#ifdef DEBUG_L1
        printf("  (i: %d ip: %x)\n", i,ip);
#endif
    }
	dout<<"<<--PurgeMe" << '\n' ;

    return opSize;


}


//----------------------------------------------------------
//! calculate the space capacity for given core
//----------------------------------------------------------
void App::calculateSpareCapacities(unsigned core)
{
	dout<<"-->>calculateSpareCapacities\n";
	unsigned Intwcet = 0;
	unsigned IntEnd = 0;
	unsigned IntStart = 0;
	int PreIntSc = 0;
	unsigned intID = 999 ;
	dout<<"int"<<'\t'<<"start"<<'\t'<<"end"<<'\t'<<"wcet"<<'\t'<<"PreIntSc"<<'\n';
	unsigned icounter = m_intervalVectMap[core]->size();
	int IntSc = -9999;


	tItrIntervalVector itr = m_intervalVectMap[core]->end();
	--itr;
	for (;icounter>0;--icounter,--itr)
	{
			 Intwcet = (*itr)->getSumWcet();
			 IntStart = (*itr)->getStart();
			 IntEnd = (*itr)->getEnd() ;
			 intID = (*itr)->getId();

			 dout<<intID<<'\t'<<IntStart<<'\t'<<IntEnd<<'\t'<<Intwcet;

			 if(icounter != m_intervalVectMap[core]->size()) // all intervals expect last one
			 {
					 PreIntSc = (*(itr+1))->getSc() ;
					 dout<<'\t'<<PreIntSc <<'\n' ;
					 IntSc = IntEnd - IntStart - Intwcet + min(0,PreIntSc) ; // TODO check the correctness of formula
																			 //for 3rd task, sc is coming as 0!! not correct
					 (*itr)->setSc(IntSc);
			 }
			 else //last interval
			 {
					 dout<<'\n'<<" last interval"<<'\n';
					 IntSc = IntEnd - IntStart - Intwcet ;
					 (*itr)->setSc(IntSc);
			 }
			 dout<<"IntervalSpareCap = " <<(*itr)->getSc() <<'\n' ;
    }
	dout<<"<<--calculateSpareCapacities\n";
}

//----------------------------------------------------------
//!
//----------------------------------------------------------
void App::dumpIntervals(tIntervalVector* intervalVecPtr)
{
	cout << "==============================interval dump=================================================\n";


				cout << "Number of intervals =" << intervalVecPtr->size();
				tItrIntervalVector itr = intervalVecPtr->begin();
				for (;itr!=intervalVecPtr->end();itr++)
				{
					printf("---------------------INTERVAL %d \n",(*itr)->getId() );
					printf(" start: %d end: %d sc: %d wcet: %d\n",(*itr)->getStart(),
						                        		 (*itr)->getEnd(),
						                        		 (*itr)->getSc(),
						                        		 (*itr)->getSumWcet());


					dumpTaskMap((*itr)->getTaskMap());
					//dumpTaskVector((*itr)->getTaskVector());

				}
	cout << "==========================================================================================\n";
}
//----------------------------------------------------------
//!
//----------------------------------------------------------
void App::dumpIntervalsFromCore(unsigned int coreId)
{
	printf("==============================core : %d =================================================\n" , coreId );


				printf("Number of intervals =  %d for core %d \n" , (int)m_intervalVectMap[coreId]->size() , coreId);
				tItrIntervalVector itr = m_intervalVectMap[coreId]->begin();
				for (;itr!=m_intervalVectMap[coreId]->end();itr++)
				{
					printf("---------------------INTERVAL %d \n",(*itr)->getId() );
					printf(" start: %d end: %d sc: %d wcet: %d\n",(*itr)->getStart(),
						                        		 (*itr)->getEnd(),
						                        		 (*itr)->getSc(),
						                        		 (*itr)->getSumWcet());


					dumpTaskMap((*itr)->getTaskMap());
					//dumpTaskVector((*itr)->getTaskVector());

				}
	cout << "==========================================================================================\n";
}
//----------------------------------------------------------
//!
//----------------------------------------------------------
void App::dumpAllIntervals()
{
	for(unsigned ccounter = 1; ccounter <= m_numberOfCores; ccounter++)
	        {
				dumpIntervalsFromCore(ccounter);
	        }


}
//----------------------------------------------------------
//!
//----------------------------------------------------------
void App::dumpTaskVector(tTaskVector* pTaskVector)
{

	tTaskVector::iterator itr = pTaskVector->begin();
	int size = pTaskVector->size();
	Task* pTask = 0 ;
	cout << "########################################### dump begin ######################################\n";
	cout << "No. of Tasks = " << size << "\n";

	for(int i=0;i<size;i++,itr++)
			{
				pTask = *itr;
				cout << "Task ID:" << pTask->getId() << "\tcore:" << pTask->getCore() << " \test:" << pTask->getEst() << "\twcet:" << pTask->getWcet() << " \tDL:" << pTask->getDl() << "\ttype:"<< pTask->getType() << "\n";
			}
	cout << "###########################################  end  ###########################################\n";
}



/**
 * inserts interval into given tIntervalVector and returns tItrIntervalVector to its location
 * @param  intervalID, corenumber, start of interval, end of interval and intervalVector pointer with iterator to location where to add
 * @return tItrIntervalVector to location of newly inserted interval
 */
tItrIntervalVector App::insertInterval(unsigned IntID ,unsigned core,
						unsigned start ,unsigned end ,
						tItrIntervalVector itrIntervalVect ,
						tIntervalVector* pIntervalVector)
{
	tItrIntervalVector itrInsertedLocation = itrIntervalVect ;
	Interval* pInterval = new Interval(IntID, core,	start, end);

	itrInsertedLocation = pIntervalVector->insert(itrIntervalVect, pInterval);
	dout<<"added interval with ID = " << pInterval->getId()
			<< "start = "<< pInterval->getStart()
			<< "END = " << pInterval->getEnd()
			<<"WCET = " <<pInterval->getSumWcet()
			<< "Core  " <<pInterval->getCore()<< "\n";

	return itrInsertedLocation;
}

/**
 * checks whether there is free space between given task's end and previous interval start
 */
bool App::isFreeSpace(tItrTaskVector itr, tItrIntervalVector itrPreviousInterval) const
{
	dout<<" -->>isFreeSpace\n";

	//assert( itr != 0 && itrPreviousInterval != 0 ) ;

	bool bRetVal = false;

	unsigned taskend = (*itr)->getDl() ;
	unsigned preIntBeg = (*itrPreviousInterval)->getStart();

	dout<<"taskend = " << taskend << "preIntBeg = " << preIntBeg << '\n';

	//bRetVal = !(taskend == preIntBeg) ;
	bRetVal = (taskend < preIntBeg) ;
	dout<<" <<--isFreeSpace: ret = "<<bRetVal<< "\n";
	return bRetVal;
}

//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------
struct predicateObj
{
	inline bool operator()(Task* const& a, Task* const& b)
	{

		return  ( a->getDl() >  b->getDl() );
	}
};

//---------------------------------------------------------------------------
//! STS sorts all tasks in  according to deadline (first entry: latest deadline) and assigns new IDs (starting with 0 from the end of the vector)
//---------------------------------------------------------------------------
void App::sortTaskVector(tTaskVector* pTaskVector)
{
	dout<<" -->>sortTaskVector\n";
	unsigned int i = 0;

	std::sort(pTaskVector->begin(), pTaskVector->end(), predicateObj());

	tTaskVector::reverse_iterator tItr = (*pTaskVector).rbegin();

	for(; tItr!=(*pTaskVector).rend(); ++tItr)
	{
		(*tItr)->setId(i++) ;



	}

	dout<<" <<--sortTaskVector\n";
	return;
}

//---------------------------------------------------------------------------
//! form a task vector with only tasks of type PERIODIC
//---------------------------------------------------------------------------
void App::createPeriodicTaskVectorMap()
{
	dout<<"		 -->>createPeriodicTaskVectorMap\n";

	tTaskVector* pTaskVector = 0;
	Task* pTask = 0;
	for (unsigned coreID = CORE_COUNT_START; coreID <= m_numberOfCores; ++coreID) // cores start with 1 !
	{
		dout<<"\t		COREID = " << coreID << "\n" ;
		tTaskVector* pNewTaskVect = new tTaskVector();
		//get task vector for the core
		pTaskVector = getTaskVector(coreID);
		assert(pTaskVector!=0) ;

		tItrTaskVector itr = pTaskVector->begin();

		for(; itr!=pTaskVector->end(); ++itr)
		{
			if( (*itr)->getType() == TASKTYPE_PERIODIC )
			{
				//create new task and add to filtered task map
				pTask = new Task(*(itr));

#ifdef DEBUG
				printf("\t	Task ID:%d \tcore:%d \test:%d \twcet:%d \tDL:%d \n",
						pTask->getId(),
						pTask->getCore(),
						pTask->getEst(),
						pTask->getWcet(),
						pTask->getDl() );
#endif
				pNewTaskVect->push_back(pTask);

			}
		}
		m_periodicTaskVectMap[coreID] = pNewTaskVect;
		dout<<"\t		number of tasks on core " << coreID << " is "<< pNewTaskVect->size() <<"\n" ;


	}

	dout<<"		 <<--createPeriodicTaskVectorMap\n";

}

//---------------------------------------------------------------------------
//! m_aperiodicIndexVectMap is filled with index of aperiodics
//---------------------------------------------------------------------------
void App::createAperiodicTaskVectorMap()
{
	dout<<"		 -->>createAperiodicTaskVectorMap\n";



	std::vector<int>* pAperiodicsIndexVector = 0;
	tTaskVector* pTaskVector = 0;

	for (unsigned coreID = 1; coreID <= m_numberOfCores; ++coreID) // cores start with 1 !
	{
		dout<<"		COREID = " << coreID << "\n" ;
		pAperiodicsIndexVector = new std::vector<int>();

	//::vector<int> tem ;



		//get task vector for the core
		pTaskVector = getTaskVector(coreID);
		assert(pTaskVector!=0) ;


	//iterate the  array in which, tasks are kept in the increasing DL
	//Cannot use the existing taskVector since it is reversed ordered ie Decreasing DL
	//it is originally made for the interval calculation which starts from the last deadline task

	//** trick use a reverse iterator - which iterate from the reverse order
	//reverse of reverse  -- forward !!
	// iterate the task vector[ordered in the decreasing DL] from end to start
	// --> will give the same result as iterating it fwd as if it is ordered in the increasing DL


		tTaskVector::reverse_iterator itr = pTaskVector->rbegin();
		//the position on an array sorted according to increasing DL]
		int index = 0;

		tTaskIndexPairVec taskIndexPairVec;

		for(; itr!=pTaskVector->rend(); ++itr)
		{

			if( (*itr)->getType() == TASKTYPE_APERIODIC )
			{

				//create a pair of task* and its position on an array sorted according to increasing DL]
				taskIndexPairVec.push_back( tTaskIndexPair(*itr,index) );

				//sort based on est of task[second item in the pair]



#ifdef DEBUG
				printf("	Aperiodic ID:%d \tcore:%d \test:%d \twcet:%d \tDL:%d \n",
						(*itr)->getId(),
						(*itr)->getCore(),
						(*itr)->getEst(),
						(*itr)->getWcet(),
						(*itr)->getDl() );
#endif

			}
			++index;
		}

		//sort based on est of task[first item in the pair]
		sortTaskIndexPairVecOnEst(&taskIndexPairVec);


		//iterate through the vector and store the index[first of the pair] in to pAperiodicsIndexVector
		std::vector<tTaskIndexPair>::iterator itr_TaskIndexPair  = taskIndexPairVec.begin();
		for(;itr_TaskIndexPair!=taskIndexPairVec.end();++itr_TaskIndexPair)
		{
			//stores the index to the vector
			pAperiodicsIndexVector->push_back(itr_TaskIndexPair->second);
		}



		//store it into map
		m_aperiodicIndexVectMap[coreID] = pAperiodicsIndexVector;

#ifdef DEBUG
		std::vector<int>::iterator it = pAperiodicsIndexVector->begin();
		cout<<"******************Index of aperiodics[in the taskArray] on COREID " <<coreID <<endl ;
		for(;it!=pAperiodicsIndexVector->end();++it)
		{
			cout<<*it <<'\t' ;
		}
		cout<<endl<<"****************************************"<<endl;
#endif


	}



	dout<<"		 <<--createaPeriodicTaskVectorMap\n";

}

struct tSortPairPred {

	bool operator ()(const tTaskIndexPair& l, const tTaskIndexPair& r)
	{
		return (  (l.first)->getEst() < (r.first)->getEst()  );
	}
};

void App::sortTaskIndexPairVecOnEst(tTaskIndexPairVec* pVec)
{

	std::sort(pVec->begin(),pVec->end(),tSortPairPred());


}

//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//! m_periodicIndexVectMap is filled with index of periodics
//---------------------------------------------------------------------------
void App::createPeriodicIndexVectorMap()
{
	dout<<"		 -->>createPeriodicIndexVectorMap\n";



	std::vector<int>* pPeriodicsIndexVector = 0;
	tTaskVector* pTaskVector = 0;

	for (unsigned coreID = 1; coreID <= m_numberOfCores; ++coreID) // cores start with 1 !
	{
		dout<<"		COREID = " << coreID << "\n" ;
		pPeriodicsIndexVector = new std::vector<int>();

	//::vector<int> tem ;



		//get task vector for the core
		pTaskVector = getTaskVector(coreID);
		//pTaskVector = getPeriodicsVector(coreID);
		assert(pTaskVector!=0) ;


	//iterate the  array in which, tasks are kept in the increasing DL
	//Cannot use the existing taskVector since it is reversed ordered ie Decreasing DL
	//it is originally made for the interval calculation which starts from the last deadline task

	//** trick use a reverse iterator - which iterate from the reverse order
	//reverse of reverse  -- forward !!
	// iterate the task vector[ordered in the decreasing DL] from end to start
	// --> will give the same result as iterating it fwd as if it is ordered in the increasing DL


		tTaskVector::reverse_iterator itr = pTaskVector->rbegin();
		//the position on an array sorted according to increasing DL]
		int index = 0;

		tTaskIndexPairVec taskIndexPairVec;

		for(; itr!=pTaskVector->rend(); ++itr)
		{

			if( (*itr)->getType() == TASKTYPE_PERIODIC )
			{

				//create a pair of task* and its position on an array sorted according to increasing DL]
				taskIndexPairVec.push_back( tTaskIndexPair(*itr,index) );

				//sort based on est of task[second item in the pair]



#ifdef DEBUG
				printf("	Periodic UID:%d ID:%d \tcore:%d \test:%d \twcet:%d \tDL:%d \n",
						(*itr)->getUid(),
						(*itr)->getId(),
						(*itr)->getCore(),
						(*itr)->getEst(),
						(*itr)->getWcet(),
						(*itr)->getDl() );
#endif

			}
			++index;
		}

		//sort based on est of task[first item in the pair]
		sortTaskIndexPairVecOnEst(&taskIndexPairVec);


		//iterate through the vector and store the index[first of the pair] in to pAperiodicsIndexVector
		std::vector<tTaskIndexPair>::iterator itr_TaskIndexPair  = taskIndexPairVec.begin();
		for(;itr_TaskIndexPair!=taskIndexPairVec.end();++itr_TaskIndexPair)
		{
			//stores the index to the vector
			pPeriodicsIndexVector->push_back(itr_TaskIndexPair->second);
		}



		//store it into map
		m_periodicIndexVectMap[coreID] = pPeriodicsIndexVector;

#ifdef DEBUG
		std::vector<int>::iterator it = pPeriodicsIndexVector->begin();
		cout<<"******************Index of Periodics[in the taskArray] on COREID " <<coreID <<endl ;
		for(;it!=pPeriodicsIndexVector->end();++it)
		{
			cout<<*it <<'\t' ;
		}
		cout<<endl<<"****************************************"<<endl;
#endif


	}



	dout<<"		 <<--createPeriodicIndexVectorMap\n";

}


//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------
tTaskIndexVec* App::getAperiodicsVector(int CoreId)
{
	return m_aperiodicIndexVectMap[CoreId] ;

}


//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------
tTaskIndexVec* App::getPeriodicsVector(int CoreId)
{
	return m_periodicIndexVectMap[CoreId] ;

}
//---------------------------------------------------------------------------
//!function object
//---------------------------------------------------------------------------

bool App::predicateFn(Task* a , Task* b )
{


	//return  ( (*a)->getDl() <  (*b)->getDl() );
	//return  ( a->getDl() <  b->getDl() );
	return true;
}

//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------

/*void App::printTasks(tTaskVector* ptempTaskVector) const
{
	dout<<" -->>printTasks " <<'\n';
	tItrTaskVector itr = ptempTaskVector->begin();
	int size = ptempTaskVector->size();

	dout<<" task vector size =  " <<size<<'\n';

	for(int i=0;i<size;i++,itr++)
			{


				printf("Task ID:%d \tcore:%d \test:%d \twcet:%d \tDL:%d \n",
						(*itr)->getId(),
						(*itr)->getCore(),
						(*itr)->getEst(),
						(*itr)->getWcet() ,
						(*itr)->getDl() );

			}

	dout<<" <<--printTasks " <<'\n';
}*/

//---------------------------------------------------------------------------
//!
//---------------------------------------------------------------------------
void App::dumpTaskMap(map<unsigned, Task*>* pMap)
{

	map<unsigned, Task*> ::iterator itr = pMap->begin();
	int size = pMap->size();

	printf("No. of Tasks = %d \n",size);

	for(int i=0;i<size;i++,itr++)
			{


				printf("Task ID:%d \tcore:%d \test:%d \twcet:%d \tDL:%d \n",
						(itr)->second->getId(),
						(itr)->second->getCore(),
						(itr)->second->getEst(),
						(itr)->second->getWcet() ,
						(itr)->second->getDl() );

			}

// TODO remember !!
}



//----------------------------------------------------------
//! STS modifies the start of intervals in case they are not adjacent
//! check if the interval start and end makes sense;
//! modify the interval start and end in cases like the following task set
//! 	Id   Type   Est   dl   wcet   IntervalID
//! 	2,   0,   10,   80,   40,   2
//! 	1,   0,   10,   50,   30,   1
//! before performing sanity check
//! 	intervals : 0 - 10 ;10-50; 10-80; 80-100
//! After sanity check
//! 	intervals : 0 - 10 ;10-50; 50-80; 80-100
//----------------------------------------------------------
void App::correctStartOfIntervals(unsigned  coreId)
{
		dout<<"-->> correctStartOfIntervals\n";

// TODO STS: there might be a bug in the code: for(; itrCur!= (m_intervalVectMap[coreId]->rend()-1); itrCur++)

		// no or only aperiodic tasks on this core, so create single interval from 0 to m_lcm
		if(m_intervalVectMap[coreId]->size() == 0)
		{
			dout<<"\t core " << coreId << " is empty\n";

			tIntervalVector::iterator itrCur;
			Interval* onlyInterval = new Interval(0, coreId, 0, m_lcm, m_lcm);
			onlyInterval->setSumWcet(0);

			//itrCur.push_back(onlyInterval);
			m_intervalVectMap[coreId]->insert(m_intervalVectMap[coreId]->begin(), onlyInterval);
			itrCur = m_intervalVectMap[coreId]->begin();

#ifdef DEBUG
			cout << "\t no of intervals on core " << coreId << " is: " << m_intervalVectMap[coreId]->size() << "\n";
			printf("\t---------------------itrCur %d \n",(*itrCur)->getId() );
			printf(" \tstart: %d end: %d sc: %d wcet: %d\n",(*itrCur)->getStart(),
						                        		 (*itrCur)->getEnd(),
						                        		 (*itrCur)->getSc(),
						                        		 (*itrCur)->getSumWcet());
#endif

			dout<<"<<-- correctStartOfIntervals\n";
			return;
		}

		tIntervalVector::reverse_iterator itrCur;
		tIntervalVector::reverse_iterator  itrPrev;

		for (itrCur = m_intervalVectMap[coreId]->rbegin(); itrCur!= (m_intervalVectMap[coreId]->rend()-1); ++itrCur)
		{
			itrPrev = itrCur + 1;

#ifdef DEBUG
			printf("---------------------itrCur %d \n",(*itrCur)->getId() );
			printf(" start: %d end: %d sc: %d wcet: %d\n",(*itrCur)->getStart(),
						                        		 (*itrCur)->getEnd(),
						                        		 (*itrCur)->getSc(),
						                        		 (*itrCur)->getSumWcet());

			printf("---------------------itrPrev %d \n",(*itrPrev)->getId() );
			printf(" start: %d end: %d sc: %d wcet: %d\n",(*itrPrev)->getStart(),
									                      (*itrPrev)->getEnd(),
									                      (*itrPrev)->getSc(),
									                      (*itrPrev)->getSumWcet());
#endif

			if((*itrCur)->getStart() < (*itrPrev)->getEnd())
			{
#ifdef DEBUG
				cout<<" ----  Cur.start  < prev.end"<<'\n';
				cout<<" ---- Cur.start  = prev.end "<<'\n';
#endif
				(*itrCur)->setStart( (*itrPrev)->getEnd() ); //STS correct
			}
		}  // end for
		dout<<"<<-- correctStartOfIntervals\n";
}
//------------------------------------------------------------------
//!
//------------------------------------------------------------------
std::vector<Task*>* App::getTaskVector(int coreID)
{
	dout<<"-->> getTaskVector\n";

	//idea
	//push this to a map with index as core id
#ifdef DEBUG
	dout<<"--coreID" <<coreID <<'\n';

	dumpTaskVector(m_taskVectMap[coreID]);
#endif
	dout<<"<<-- getTaskVector\n";

	return m_taskVectMap[coreID];
}

//------------------------------------------------------------------
//!
//------------------------------------------------------------------
std::vector<Task*>* App::getPeriodicTaskVector(int coreID)
{
	dout<<"-->> getPeriodicTaskVector   --   coreID" << coreID << "\n";

	//dumpTaskVector(m_periodicTaskVectMap[coreID]);

	dout<<"<<-- getPeriodicTaskVector\n";
	return m_periodicTaskVectMap[coreID];
}

//------------------------------------------------------------------
//! form mTaskVectMap from mTaskVector which contain all the read tasks;
//! in mTaskVectMap, the tasks are ordered according to the core
//------------------------------------------------------------------
void App::createTaskVectorMap()
{
	dout<<"		<<-- createTaskVectorMap\n";
	for (unsigned coreID = CORE_COUNT_START; coreID <= m_numberOfCores; ++coreID) // cores start with CORE_COUNT_START !
	{
		dout << "\t\tcreating map for coreID : " << coreID << "\n";

		tTaskVector* pTaskVect = new tTaskVector();
		tItrTaskVector itr = m_taskVector.begin();
		Task* pTask = 0;

		//search in the mTaskVector for tasks belonging to given core
		for (; itr != m_taskVector.end(); ++itr) {

			pTask = (*itr);
			dout << "\t\t --- iteration over all tasks, this one is on core: " << pTask->getCore() << "\n";
			if (coreID == pTask->getCore())
				pTaskVect->push_back(pTask);
			m_taskVectMap[coreID] = pTaskVect;
		}
	}
	dout<<"		-->> createTaskVectorMap\n";
}

/**
 * This function adds an interval for each task's dl
 * It will also add (empty) intervals at the end and at the
 * beginning and in between
 */
bool App::markInterval(tTaskVector* pTaskVector, unsigned core)
{

	dout << "-->>MarkInterval\n";

	bool bRet = false;
	tIntervalVector* pIntervalVector = new tIntervalVector();
	tItrIntervalVector itrIntervalVect = pIntervalVector->begin();


	if (pTaskVector->size() == 0)
	{
		cout << "\t-------------App::markInterval() core " << core  << " empty\n";
		m_intervalVectMap[core] = pIntervalVector;

		dout << "<<--MarkInterval\n";
		return true;
	}

	//TODO case not checked: when there is no task in a core!!




	// Initialize to a dummy interval with id = 999 and start as end of time line (m_lcm)
	Interval* pDummyInt = new Interval(999);
	pDummyInt->setStart(m_lcm);
    // STS: it's not necessary to initialize the end of the interval
	tIntervalVector tempIntVector;
	tempIntVector.push_back(pDummyInt);



	tItrIntervalVector itrPreviousInterval = tempIntVector.begin();
	tItrIntervalVector itrWorkingInterval  = pIntervalVector->begin();

	unsigned IntID = 0;

#ifdef DEBUG
	dumpTaskVector(pTaskVector);
#endif

	tItrTaskVector itr    = pTaskVector->begin();
	tItrTaskVector itrNxt = pTaskVector->begin();
	tItrTaskVector itrCur = pTaskVector->begin();

	unsigned sizeofTaskVector = pTaskVector->size();
	if(sizeofTaskVector == 0) {return true;}

	if ((*itr)->getDl() > m_lcm)  // STS: first entry in vector: task with latest dl, so this is a check whether the latest dl is bigger than the lcm
	{

		printf("---------App::markInterval() ERROR : latest task's dl > lcm \n");
		printf("---------latest task's ID=%d ; dl = %d ;  m_lcm = %d \n",(*itr)->getId(),(*itr)->getDl(),m_lcm);

		return false;
	}

	// STS create an interval (to bridge gap between last task end and m_lcm)  and add this interval
	//space in the end
	if ((*itr)->getDl() < m_lcm) // STS: first entry in vector: task with latest dl, so this is a check whether there is some space left at the end before the lcm
	{		// add interval on this core to fill the gap
		dout << "FOUND space in the end" << '\n';
		itrPreviousInterval = insertInterval(IntID, core, (*itr)->getDl(), m_lcm, itrIntervalVect, pIntervalVector);
		IntID++;
	}

	// --- FOR LOOP ---
	//STS: this loop will create intervals, starting from the end of the time line(m_lcm)
	for (unsigned i = 0; i < sizeofTaskVector ; itr++, i++)
	{
		dout << "----------------------------------iteration  " << i << "\n";
		//STS: in case the itr task does not end at the previous intervals begin (0 in first iteration), add interval to fill the gap
		// not entered in first iteration: 2 cases exist, both don't enter
		// case 1: last task has dl = lcm
		//        -->  if ((*itr)->getDl() < m_lcm)   not triggered
		//        --> enters loop with dummy interval (start: lcm, end: 0) in itrPreviousInterval (== tempIntVector.begin())
		//		  --> will not trigger if (isFreeSpace(lcm, lcm))
		// case 2: last task has dl < lcm
		//        -->  if ((*itr)->getDl() < m_lcm)   triggered  ---> task added
		//        --> enters loop with interval (start: dl of task, end: lcm) in itrPreviousInterval (now iterator to pIntervalVector object)
		//		  --> will not trigger if (isFreeSpace(dl of task, dl of task))
		// I think it's correctly only triggered between if there's gap between intervals
		if (isFreeSpace(itr, itrPreviousInterval)) //space between current task's dl and previous inserted interval start, STS: bRetVal = (taskend < preIntBeg) ;
		{
			dout << "FOUND space in between tasks\n";
			itrIntervalVect = pIntervalVector->begin();

			itrPreviousInterval = insertInterval(IntID, core, (*itr)->getDl(),
					(*itrPreviousInterval)->getStart(), itrIntervalVect,
					pIntervalVector);
			IntID++;
		}


		unsigned IntWcet = (*itr)->getWcet();  // STS:  Don introduces a variable for the sum of all wcets in the interval
		unsigned IntEst = (*itr)->getEst();
		unsigned Intdl = (*itr)->getDl();

		itrIntervalVect = pIntervalVector->begin();
		itrWorkingInterval = insertInterval(IntID, core, IntEst, Intdl,
				itrIntervalVect, pIntervalVector);
		IntID++;

		//add task to interval
		(*itrWorkingInterval)->addTask((*itr)->getId(), *itr);

		//STS  weird comment from Don: "look for task with the same dl"
		// the following block will terminate the for loop if we reached the last task
		itrCur = itr; 
		itrNxt = ++itr; //returns the new location, itr will be restored later on 
				// itr+1 doesn't work ;++itr is the easiest way to find the next iterator
		

		if (itrNxt == pTaskVector->end())
		{
			dout << " --- itrNxt == pTaskVector->end() ; breaking the for loop\n";
			dout << " Setting the start of interval :" << IntEst << "\n";
			(*itrWorkingInterval)->setStart(IntEst);

			//setting the wcet of interval
			(*itrWorkingInterval)->setSumWcet(IntWcet);

			//for the next iteration
			itrPreviousInterval = itrWorkingInterval;

			//to restore the task iterator
			--itr;
			break;
		}

		// this loop adds all tasks with the same dl to the current interval (in case there are multiple)
		while ((*itrCur)->getDl() == (*itrNxt)->getDl())
		{

			dout << " Found task ID " << (*itr)->getId() << " with same dl\n";

			IntWcet += (*itr)->getWcet();

			// IntervalEst = min(est) of all task in that interval
			if (IntEst > (*itr)->getEst())
				IntEst = (*itr)->getEst();

			//add task to interval
			(*itrWorkingInterval)->addTask((*itr)->getId(), *itr);

			//for the next iteration
			itrCur = itr;
			itrNxt = ++itr; //returns the new location
			++i;//increment the count


			//after incrementing,check for validity of iterator
			if (itrNxt == pTaskVector->end())
			{
				dout << " --- itrNxt == pTaskVector->end() ; breaking the while loop\n";
				break;
			}

		} // end while
		//to set the task iterator to point to the latest added interval
		--itr;

		dout << " Setting the start of interval :" << IntEst << '\n';
		(*itrWorkingInterval)->setStart(IntEst);

		//setting the wcet of interval
		(*itrWorkingInterval)->setSumWcet(IntWcet);

		//for the next iteration
		itrPreviousInterval = itrWorkingInterval;
	}
	// --- END FOR LOOP ---



	// this adds an interval in the beginning of the time line if needed
	// checks for space in the beginning  and adds interval
	dout << " ---check for space in the beginning\n";
	unsigned intervalStart = (*itrPreviousInterval)->getStart();

	if (intervalStart != 0)
	{
		dout << " --------space found in the beginning between 0 and "
				<< intervalStart << "\n";

		itrIntervalVect = pIntervalVector->begin();
		itrWorkingInterval = insertInterval(IntID, core, STARTOFTIME, intervalStart,
				itrIntervalVect, pIntervalVector);
		IntID++;
	}
	reorderIntervalId(pIntervalVector) ;

	m_intervalVectMap[core] = pIntervalVector;
	bRet = true;
	dout << "<<--MarkInterval\n";
	return bRet ;
}




//------------------------------------------------------------------
//! reorder the interval Ids ;
//! first interval gets ID = 0  and so on
//------------------------------------------------------------------
void App::reorderIntervalId(tIntervalVector* pIVect)
{

	int id = 0;
	int size = pIVect->size() - 1; //id starts from 0
	dout<< size << '\n' ;
	tItrIntervalVector itr = pIVect->begin();
	for (;itr!=pIVect->end();++itr)
	{
		id = (*itr)->getId() ;
		id =  size - id ;
		(*itr)->setId(id) ;
	}

}

//------------------------------------------------------------------
//! mIntervalVectMap getter
//------------------------------------------------------------------
std::map<unsigned, tIntervalVector*>* App::getIntervalMap()
{

	return &m_intervalVectMap ;

}
//------------------------------------------------------------------
//! m_numberOfCores getter
//------------------------------------------------------------------
unsigned App::getCoreCount() const
{
	return m_numberOfCores;
}
//------------------------------------------------------------------
//! m_lcm getter
//------------------------------------------------------------------
unsigned App::getLcm() const
{
	return m_lcm;
}
//------------------------------------------------------------------
//! m_slotsize getter
//------------------------------------------------------------------
unsigned App::getSlotsize() const
{
	return m_slotsize;
}

//------------------------------------------------------------------
//! m_NoTask getter
//------------------------------------------------------------------
unsigned App::getTaskNumOnCore(unsigned core)
{
	std::vector<Task*>* pTaskVec = getTaskVector(core);
	return (*pTaskVec).size();
}
//------------------------------------------------------------------
//!
//------------------------------------------------------------------
unsigned App::getIntervalNumOnCore(unsigned core)
{
	tIntervalVector* pIntervalVec = m_intervalVectMap[core] ;//
	return (*pIntervalVec).size();
}
//------------------------------------------------------------------
//! calculates max number of (really needed) intervals
//------------------------------------------------------------------
unsigned App::getMaxIntervalCount()
{
	unsigned max = 0;
	unsigned IntervalCount = 0;
	tIntervalVector* pIntervalVec ;
	for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
	{
		pIntervalVec = m_intervalVectMap[coreID];

		IntervalCount = (*pIntervalVec).size() ;
		if( max < IntervalCount )
			max = IntervalCount ;
	}

	return max;
}

//------------------------------------------------------------------
//!
//------------------------------------------------------------------
unsigned App::getMaxTaskCount()
{
	unsigned max = 0;
	unsigned taskCount = 0;
	tTaskVector* pTaskVec ;
	for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
	{
		pTaskVec = m_taskVectMap[coreID];

		assert(pTaskVec!=0);
		taskCount = (*pTaskVec).size() ;
		if( max < taskCount )
			max = taskCount ;
	}

	return max;
}

//------------------------------------------------------------------
//!
//------------------------------------------------------------------
unsigned App::getMaxAperiodicTaskCount()
{
        unsigned max = 0;
        unsigned taskCount = 0;
        tTaskVector* pTaskVec ;
        tItrTaskVector itr;
        
        for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
        {
                pTaskVec = m_taskVectMap[coreID];

                assert(pTaskVec!=0);
                // iterate over all tasks and count aperiodics
                
                itr = pTaskVec->begin();
                for(taskCount = 0; itr != pTaskVec->end(); ++itr)
                {
                    if((*itr)->getType() == TASKTYPE_APERIODIC)
                        ++taskCount;
                }                    
                if( max < taskCount )
                        max = taskCount;
        }

        return max;
}




unsigned App::getAperiodicTaskCount()
{
        unsigned taskCount = 0;
        tTaskVector* pTaskVec ;
        tItrTaskVector itr;

        for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
        {
                pTaskVec = m_taskVectMap[coreID];

                assert(pTaskVec!=0);
                // iterate over all tasks and count aperiodics

                itr = pTaskVec->begin();
                for(; itr != pTaskVec->end(); ++itr)
                {
                    if((*itr)->getType() == TASKTYPE_APERIODIC)
                        ++taskCount;
                }
        }

        return taskCount;

}

//------------------------------------------------------------------
//!
//------------------------------------------------------------------
unsigned App::getMaxPeriodicTaskCount()
{
        unsigned max = 0;
        unsigned taskCount = 0;
        tTaskVector* pTaskVec ;
        tItrTaskVector itr;

        for (unsigned coreID = 1; coreID <= m_numberOfCores; coreID++) // cores start with 1 !
        {
                pTaskVec = m_taskVectMap[coreID];

                assert(pTaskVec!=0);
                // iterate over all tasks and count aperiodics

                itr = pTaskVec->begin();
                for(taskCount = 0; itr != pTaskVec->end(); ++itr)
                {
                    if((*itr)->getType() == TASKTYPE_PERIODIC)
                        ++taskCount;
                }
                if( max < taskCount )
                        max = taskCount;
        }

        return max;
        //return 0;
}
//(m_pApp->GetMaxIntervalCount() + m_pApp->GetMaxAperiodicTaskCount())

//------------------------------------------------------------------
//!
//------------------------------------------------------------------
tIntervalMap* App::getInterVectMap()
{
	return 	&m_intervalVectMap ;
}
//------------------------------------------------------------------
//! returns the interval to which the task belongs to
//------------------------------------------------------------------
int App::getIntervalID(unsigned tId,unsigned coreId)
{
	int IntID = -1;

	tIntervalVector* pIntVector = m_intervalVectMap[coreId] ;
	map<unsigned, Task*>* pTaskMap = 0;
	map<unsigned, Task*>::iterator itrTaskMap ;
	assert(pIntVector!=0);
	tIntervalVector::iterator itrInterVect =(*pIntVector).begin() ;

	for(;itrInterVect!=(*pIntVector).end() ; itrInterVect++)
	{

		pTaskMap = (*itrInterVect)->getTaskMap();

#ifdef DEBUG_L2
		printf ("------------------- \n");
		dumpTaskMap(pTaskMap);
		printf ("------------------- \n");
#endif
		assert(pTaskMap!=0);

		itrTaskMap = (*pTaskMap).find(tId);
		if( itrTaskMap!=(*pTaskMap).end() )
		{
			//found the task id in the taskmap;
			//return the interval id
			IntID = (*itrInterVect)->getId();
			break;
		}


	}


	return IntID ;

}
