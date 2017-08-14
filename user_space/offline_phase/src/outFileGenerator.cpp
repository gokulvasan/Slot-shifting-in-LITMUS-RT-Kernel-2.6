/*
 * outputWriter.cpp
 *
 *  Created on: Oct 21, 2011
 *      Original-Author: Don Kuzhiyelil
 *	Modified by: Stefan Schorr
 */

#include "outFileGenerator.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <assert.h>


#define MAXNUMOFTASK_PERINTERVAL 10
#define MAX_TASKS_PER_INTERVAL 10


// this will enable debug info [struct type, task and interval table]to be printed in the generated file
#define DEBUGDATA_INFILE

//this will print the generated file content to console
//#define PRINT_FILECONTENT_IN_CONSOLE


OutFileGenerator::OutFileGenerator(App* pApp) :
	m_pApp(pApp)
{
	m_pApp = pApp;
}

OutFileGenerator::~OutFileGenerator()
{
}

OutTxtFileGenerator::OutTxtFileGenerator(App* pApp) :
        m_pApp(pApp)
{
        m_pApp = pApp;
	m_coreCount = m_pApp->getCoreCount();
}

OutTxtFileGenerator::~OutTxtFileGenerator()
{
}

void OutTxtFileGenerator::setFileName(string fName)
{

        m_fileName = fName;
}


//---------------------------------------------------------
//!
//---------------------------------------------------------
void OutFileGenerator::setFileName(string fName)
{
	
	m_fileName = fName;
	dout<<m_fileName;
}


void OutTxtFileGenerator::dummyWrite(void)
{
        //open file
        fstream txt_file;
	string CContent;
	string TxtFileName = m_fileName+".txt";
        txt_file.open(TxtFileName.data(), ios::out);
	//cout << "XXXX: OPENING FILE---" << TxtFileName.data();
        //write to file
	CContent += generalData();
	CContent += tasksOnCore();
	CContent += numOfIntervals();
	CContent += generateIntervalArray();
	CContent += generateTaskArray();
	CContent += generateAperiodicsArray();
	CContent += generatePeriodicsArray();
	txt_file<< CContent;
        //close file
        txt_file.close();
}



//---------------------------------------------------------
//!
//---------------------------------------------------------
int OutFileGenerator::generateOutFile()
{
	string HContent; // content of header file
	string CContent ; // content of c file

	string HFileName = m_fileName+".h";
	string CFileName = m_fileName+".c";


	HContent += generateHeaderFileContent();


	//open file
	fstream of_H;
	of_H.open(HFileName.data(), ios::out);
	//write to file
	of_H << HContent;
	//close file
	of_H.close();


	CContent += generateHeader();
	CContent += generateIntervalArray();
	CContent += generateTaskArray();
	CContent += generateAperiodicsArray();
	CContent += generatePeriodicsArray();

	CContent += generateEpilogue();


#ifdef PRINT_FILECONTENT_IN_CONSOLE

	cout << '\n'<< '\n'<<"----------------- "<<HFileName<<" content ----------------------------";
	cout << '\n' ;
	cout << HContent;

	cout << '\n'<< '\n'<<"----------------- "<<CFileName<<" content ----------------------------";
	cout << '\n' ;
	cout << CContent;
#endif

	//open file
	fstream of_C;
	of_C.open(CFileName.data(), ios::out);
	//write to file
	of_C << CContent;
	//close file
	of_C.close();


	//cout<<"---- schedule file created!!" <<endl ;
        return(0);
}

//---------------------------------------------------------
//! Generate header file
//---------------------------------------------------------
string OutFileGenerator::generateHeaderFileContent() {
	m_slotsize = m_pApp->getSlotsize();
	m_lcm = m_pApp->getLcm();
	m_coreCount = m_pApp->getCoreCount();

	dout << "mSlotSize" << m_slotsize << '\n';
	dout << "mLCM" << m_lcm << '\n';
	dout << "mCoreCount" << m_coreCount << '\n';
	dout << "mTaskNum" << m_pApp->getMaxTaskCount() << '\n';

	string header;
	stringstream hStream;

	hStream << "#ifndef SCHEDULING_H" << '\n';
	hStream << "#define SCHEDULING_H" << '\n'<< '\n';
	//hStream << "#include \"my_types.h\"" << '\n'<< '\n';

	hStream << "#define SLOT_SIZE " << m_slotsize << '\n';
	hStream << "#define MAX_CORES   " << m_coreCount << '\n';
	hStream << "#define MAX_TIME   " << m_pApp->getLcm()<< '\n' ;
	hStream << "#define MAX_TASKS_PER_CORE   " << m_pApp->getMaxTaskCount() << '\n';

	//WHY adding the number of Max aperiodictask to interval ??
	// because intervals may be split at run-time!

	dout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
	dout << "m_pApp->getMaxIntervalCount()" << m_pApp->getMaxIntervalCount() << "\n";
	dout << " m_pApp->getMaxAperiodicTaskCount()" <<  m_pApp->getAperiodicTaskCount() << "\n";

	dout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	hStream << "#define MAX_INTERVALS_PER_CORE   " << (m_pApp->getMaxIntervalCount() + m_pApp->getAperiodicTaskCount())<< '\n'  ;
	hStream << "#define MAXNUMOFTASK_PERINTERVAL   " << MAXNUMOFTASK_PERINTERVAL<< '\n'<< '\n';
	hStream << "#define MAX_TASKS_PER_INTERVAL   " << MAX_TASKS_PER_INTERVAL<< '\n'<< '\n';

	hStream << "#define MAX_APERIODICS_ON_ANY_CORE   " <<m_pApp->getMaxAperiodicTaskCount() << '\n'<< '\n';
	hStream << "#define MAX_PERIODICS_ON_ANY_CORE   " <<m_pApp->getMaxPeriodicTaskCount() << '\n'<< '\n';


	hStream << "unsigned g_number_of_tasks_on_core[MAX_CORES] ;" << '\n';
	hStream << "unsigned g_number_of_intervals[MAX_CORES] ;" << '\n';

    hStream << "struct tTask* g_aperiodics_index[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE] ;" << '\n';
	hStream << "int g_number_of_aperiodics[MAX_CORES] ;" << '\n';

    hStream << "struct tTask* g_periodics_index[MAX_CORES][MAX_PERIODICS_ON_ANY_CORE] ;" << '\n';
	hStream << "int g_number_of_periodics[MAX_CORES] ;" << '\n';




	hStream << " struct tTask"<<'\n'
			<< " {\n"
			<< " 	int uid;				// unique ID for each task (as in original input file)\n"
			<< " 	unsigned arrival_time, wcet, remaining, deadline;\n"
			<< " 	short int type;                           // determines whether offline guaranteed task (1) or aperiodic (2) or aperiodic already accepted (3) or aperiodic rejected (4) finished execution (99)\n"
			<< " 	short unsigned core_id;\n"
			<< " 	struct tInterval* interval_ptr;\n"
			<< " };\n";




	hStream << "struct tInterval"<<'\n'
			<< "{"<<'\n'
			<< "    unsigned uid;\n"
			<< "    unsigned start, end;"<<'\n'
			<< "   short int spare_capacity;"<<'\n'
			<< "   struct tInterval* prev;\n"
			<< "   struct tInterval* next;\n"
			<< "    unsigned number_of_tasks;"<<'\n'
			<< "   struct tTask* tasks[MAX_TASKS_PER_INTERVAL]; // entries in this array contain task ids"<<'\n'
			<< "};"<<'\n';


	hStream << "struct tInterval g_interval_array[MAX_CORES][MAX_INTERVALS_PER_CORE];"
			<< '\n';

	hStream << "struct tTask g_task_array[MAX_CORES][MAX_TASKS_PER_CORE];" << '\n';




	hStream << "#endif" << '\n';




	header += hStream.str();
	dout << header;
	return header;
}


string OutTxtFileGenerator::generalData(){
	stringstream bStream;
	dout << "\nE1:" << m_pApp->getCoreCount()<<endl;
	//bStream << 'g' << ";";
	bStream << m_pApp->getCoreCount() << ";";
	bStream << m_pApp->getLcm() << ";";
	bStream << m_pApp->getMaxTaskCount() << ";";
        //printf("XXXX [%d] [%d]\n",m_pApp->getMaxIntervalCount(),m_pApp->getAperiodicTaskCount());
	bStream << (m_pApp->getMaxIntervalCount() + m_pApp->getAperiodicTaskCount()) << ";";
	bStream << MAXNUMOFTASK_PERINTERVAL << ";";
	bStream << MAX_TASKS_PER_INTERVAL << ";";
	bStream << m_pApp->getMaxAperiodicTaskCount() << ";";
	bStream << m_pApp->getMaxPeriodicTaskCount() << "";
	bStream << "\n";
	return bStream.str();
}

string OutTxtFileGenerator::tasksOnCore(){
	stringstream bStream;
	//bStream << 't' << ";";
	bStream << m_coreCount << ";";
        unsigned i = 1;
        for (; i < m_coreCount;i++)
        {
                bStream << m_pApp->getTaskNumOnCore(i) << ";";
        }
        bStream << m_pApp->getTaskNumOnCore(i);
	bStream << "\n";
	return bStream.str();
}

string OutTxtFileGenerator::numOfIntervals(){

        //*******
        //      unsigned number_of_intervals[MAX_CORES] = {2, 1, 1};     // number of intervals per core
        //*******
	stringstream bStream;
	//bStream << 'i' << ";" ;
	bStream << m_coreCount << ";" ;
	unsigned i;
        for (i = 1; i < m_coreCount; ++i)
        {
                bStream << m_pApp->getIntervalNumOnCore(i) << ";";
        }

        //last entry
        bStream << m_pApp->getIntervalNumOnCore(i) << "\n" ;
        return bStream.str();
	
}

//---------------------------------------------------------
//! Generate header for c file
//*******
//unsigned number_of_tasks[MAX_CORES]     = {2, 1, 1};     // number of tasks per core
//*******
//---------------------------------------------------------
string OutFileGenerator::generateHeader()
{

	string header;
	stringstream hStream;


	hStream <<endl ;//"
	hStream << "#include \"scheduling_table.h\""<<endl ;//"
	hStream <<endl<<endl ;//"

	hStream << " unsigned g_number_of_tasks_on_core[MAX_CORES] = { ";//
	dout << " unsigned g_number_of_tasks[MAX_CORES] = { ";//

	unsigned i = 1;
	for (; i < m_coreCount; ++i)
	{
		hStream << m_pApp->getTaskNumOnCore(i) << ",";
		dout << m_pApp->getTaskNumOnCore(i) << ",";

	}
		//last entry
	hStream << m_pApp->getTaskNumOnCore(i) << "}; " << '\n';
	dout << m_pApp->getTaskNumOnCore(i) << "}; " << '\n';

	//*******
	//	unsigned number_of_intervals[MAX_CORES] = {2, 1, 1};     // number of intervals per core
	//*******
	hStream << " unsigned g_number_of_intervals[MAX_CORES] = { ";//
	dout << " unsigned g_number_of_intervals[MAX_CORES] = { ";//
	for (i = 1; i < m_coreCount; ++i)
	{
		hStream << m_pApp->getIntervalNumOnCore(i) << ",";
		dout<<m_pApp->getIntervalNumOnCore(i) << "," ;
	}

	//last entry
	hStream << m_pApp->getIntervalNumOnCore(i) << " }; " << '\n';
	dout<<m_pApp->getIntervalNumOnCore(i)<< " }; " << '\n';
	header += hStream.str();
	dout << header;
	return header;

}

//------------------------------------------------------------------------------------------------------
//! Generate Interval array
/*
struct tinterval
{
   unsigned start, end;
   int spare_capacity;
   unsigned number_of_tasks;
   unsigned tasks[MAX_TASKS_PER_INTERVAL];   // entries in this array contain task ids
};
 */
//------------------------------------------------------------------------------------------------------
//! Generate Interval array in format -
//! struct tInterval interval_array[MAX_CORES][MAXINTERVALCOUNT] =
//!	{
//! 	{	{0,10,10,-1,-1,-1,-1,-1},{10,50,20,1,-1,-1,-1,-1},{50,90,20,2,-1,-1,-1,-1}			},
//! 	{	{0,10,10,-1,-1,-1,-1,-1},{10,50,20,3,-1,-1,-1,-1},{50,90,20,4,-1,-1,-1,-1}			}
//   };
//!
//------------------------------------------------------------------------------------------------------


string OutTxtFileGenerator::generateIntervalArray(void){
	string body;
        stringstream bStream;
        // returns the maximum number of interval in any core
        int maxIntervalCount = m_pApp->getMaxIntervalCount();

        tIntervalMap* pIntVectMap = m_pApp->getInterVectMap();

        assert(pIntVectMap!=0);

        tIntervalVector pIntVec;
        map<unsigned, Task*>* pTaskMap = 0;
        map<unsigned, Task*>::iterator TaskItr;
        int size = 0;
        tIntervalVector::iterator Itr;

        int NumOfInterval = 0;
	//bStream <<'a'<<';';
	bStream<<'{';
        for (unsigned j = 1; j <= m_coreCount; j++) // rows
        {


                Itr = (*pIntVectMap)[j]->begin();

                NumOfInterval = (*pIntVectMap)[j]->size();


                for (int IntCount = 1; Itr != (*pIntVectMap)[j]->end(); Itr++, ++IntCount) 
                {

                        bStream<<"[" ;
                        bStream <<  (*Itr)->getId() << "," << (*Itr)->getStart() << "," << (*Itr)->getEnd() << ","
                                       << (*Itr)->getSc() << ",0,0";

                        pTaskMap = (*Itr)->getTaskMap();
                        assert(pTaskMap!=0);

                        TaskItr = pTaskMap->begin();
                        size = pTaskMap->size();

                        bStream << "," << size ;


                        if (size > MAXNUMOFTASK_PERINTERVAL) {
                                printf(
                                                "-------- ERROR : number of Tasks per Interval exceeded limit \n");
                                return "-------- ERROR : number of Tasks per Interval exceeded limit \n";
                        }
                        for (int count = 0; count < size; count++, TaskItr++)
                                bStream << "," << (TaskItr)->second->getId();

                        //if the tasks are less - fill the remaining ones with -1
                        if (size < MAXNUMOFTASK_PERINTERVAL) {
                                int rem = MAXNUMOFTASK_PERINTERVAL - size;
                                dout << "tasks are less by" << rem << '\n';
                                for (int count = 0; count < rem; count++)
                                        bStream << ",-1";
                        }

                        if (IntCount == NumOfInterval) //last interval
                                bStream << "]";
                        else
                                bStream << "],";//if not last one


                }
                if (NumOfInterval < maxIntervalCount)
                        insertDummyInterval(maxIntervalCount - NumOfInterval, bStream); //insert dummy interval
		bStream<<',';

        }
        bStream << '}' << '\n' ;
        body += bStream.str();
        dout << body;
	
        return body;
	
}


void OutTxtFileGenerator::insertDummyInterval(unsigned NoOfDummyInterval,
                stringstream& bStream)
{
        for (unsigned i = 1; i <=NoOfDummyInterval; ++i) {
                bStream << ",[-1,-1,-1,-1,0,0,0";
            for (int count = 0; count < MAXNUMOFTASK_PERINTERVAL; count++)
                bStream << ",0";
        bStream << "]";
        }
}


void OutTxtFileGenerator::insertDummyTask(unsigned NoOfDummyTask,
                stringstream& bStream)
{
        dout << "-->>InsertDummyTask : " << NoOfDummyTask << '\n';

        for (unsigned i = 1; i <= NoOfDummyTask; ++i) {
                bStream << "[-1,-1,-1,-1,-1,-1,-1,-1],";
        }

}



string OutFileGenerator::generateIntervalArray() {

	string body;
	stringstream bStream;
	// returns the maximum number of interval in any core
	int maxIntervalCount = m_pApp->getMaxIntervalCount();

	/*
	 bStream<<"struct tInterval interval_array["<< mCoreCount << "]"
	 << "["<< maxIntervalCount<< "]  = ";
	 */
	bStream
			<< "struct tInterval g_interval_array[MAX_CORES][MAX_INTERVALS_PER_CORE] = ";

	bStream << '\n' << "{" << '\n' << '\n';

	tIntervalMap* pIntVectMap = m_pApp->getInterVectMap();

	assert(pIntVectMap!=0);

	tIntervalVector pIntVec;
	map<unsigned, Task*>* pTaskMap = 0;
	map<unsigned, Task*>::iterator TaskItr;
	int size = 0;
	tIntervalVector::iterator Itr;

	int NumOfInterval = 0;
	for (unsigned j = 1; j <= m_coreCount; j++) // rows
	{

		bStream << "	{	";

		//Itr = (*pIntVectMap)[j]->rbegin();
		Itr = (*pIntVectMap)[j]->begin();

		NumOfInterval = (*pIntVectMap)[j]->size();

		dout << "---------------------------core ID : " << j << '\n';
		dout << "--NumOfInterval : " << NumOfInterval << '\n';
		//coloum
		for (int IntCount = 1; Itr != (*pIntVectMap)[j]->end(); Itr++, ++IntCount) //coloum
		{

			//Start of interval i
#ifdef DEBUG
			printf("---------------------INTERVAL %d \n", (*Itr)->getId());
			printf(" start: %d end: %d sc: %d\n", (*Itr)->getStart(),
					(*Itr)->getEnd(), (*Itr)->getSc());
#endif

			bStream<<"{" ;
			bStream << IntCount-1 << "," << (*Itr)->getStart() << "," << (*Itr)->getEnd() << ","
					<< (*Itr)->getSc() << ",0,0";

			pTaskMap = (*Itr)->getTaskMap();
			assert(pTaskMap!=0);

			TaskItr = pTaskMap->begin();
			size = pTaskMap->size();

			bStream << "," << size ;


			if (size > MAXNUMOFTASK_PERINTERVAL) {
				printf(
						"-------- ERROR : number of Tasks per Interval exceeded limit \n");
				return "-------- ERROR : number of Tasks per Interval exceeded limit \n";
			}
			for (int count = 0; count < size; count++, TaskItr++)
				bStream << ",(struct tTask*)" << (TaskItr)->second->getId();

			//if the tasks are less - fill the remaining ones with -1
			if (size < MAXNUMOFTASK_PERINTERVAL) {
				int rem = MAXNUMOFTASK_PERINTERVAL - size;
				dout << "tasks are less by" << rem << '\n';
				for (int count = 0; count < rem; count++)
					bStream << ",(struct tTask*)-1";
			}

			if (IntCount == NumOfInterval) //last interval
				bStream << "}";
			else
				bStream << "},";//if not last one


		}
		if (NumOfInterval < maxIntervalCount)
			insertDummyInterval(maxIntervalCount - NumOfInterval, bStream); //insert dummy interval


		if (j == m_coreCount) //for last core
			bStream << "			}" << '\n';
		else
			bStream << "			}," << '\n';

	}
	bStream << '\n' << '\n' << "};" << '\n';
	body += bStream.str();
	dout << body;
	return body;
}

//-------------------------------------------------
//! insert dummy interval
//-------------------------------------------------
void OutFileGenerator::insertDummyInterval(unsigned NoOfDummyInterval,
		stringstream& bStream)
{
	dout << "-->>InsertDummyInterval : " << NoOfDummyInterval << '\n';

	for (unsigned i = 1; i <=NoOfDummyInterval; ++i) {
		bStream << ",{ -1,-1,-1,-1";


	for (int count = 0; count < MAXNUMOFTASK_PERINTERVAL; count++)
		bStream << ",0";
	bStream << " }";
}

	dout << "<<---InsertDummyInterval" << '\n';

}

//-------------------------------------------------
//! insert dummy interval
//-------------------------------------------------
void OutFileGenerator::insertDummyTask(unsigned NoOfDummyTask,
		stringstream& bStream)
{
	dout << "-->>InsertDummyTask : " << NoOfDummyTask << '\n';

	for (unsigned i = 1; i < NoOfDummyTask; ++i) {
		bStream << "{ -1,-1,-1,-1,-1,-1,-1,-1 },";
	}
	//for last one
	bStream << "{ -1,-1,-1,-1,-1,-1,-1,-1 }";

	dout << "<<---InsertDummyTask" << '\n';

}

//-------------------------------------------------
//! insert dummy aperiodics indexes
//-------------------------------------------------
void OutFileGenerator::insertDummyAperiodicIndex(unsigned NoOfDummyTask,
		stringstream& bStream)
{
	dout << "-->>InsertDummyTask : " << NoOfDummyTask << '\n';

	for (unsigned i = 1; i < NoOfDummyTask; ++i) {
		bStream << "-1,";
	}
	//for last one
	bStream << "-1";

	dout << "<<---InsertDummyTask" << '\n';

}

void OutTxtFileGenerator::insertDummyAperiodicIndex(unsigned NoOfDummyTask,
		stringstream& bStream)
{

	for (unsigned i = 1; i < NoOfDummyTask; ++i) {
		bStream << "-1,";
	}
	//for last one
	bStream << "-1,";

}

void OutTxtFileGenerator::insertDummyPeriodicIndex(unsigned NoOfDummyTask,
                stringstream& bStream)
{
        dout << "-->>InsertDummyTask : " << NoOfDummyTask << '\n';

        for (unsigned i = 1; i < NoOfDummyTask; ++i) {
                bStream << "-1,";
        }
        //for last one
        bStream << "-1,";

}

//-------------------------------------------------
//! insert dummy periodics indexes
//-------------------------------------------------
void OutFileGenerator::insertDummyPeriodicIndex(unsigned NoOfDummyTask,
		stringstream& bStream)
{
	dout << "-->>InsertDummyTask : " << NoOfDummyTask << '\n';

	for (unsigned i = 1; i < NoOfDummyTask; ++i) {
		bStream << "-1,";
	}
	//for last one
	bStream << "-1";

	dout << "<<---InsertDummyTask" << '\n';

}


string OutTxtFileGenerator::generateTaskArray(){
	string body;
	stringstream bStream;

	unsigned maxTaskCount = m_pApp->getMaxTaskCount();
	unsigned size = 0;
	m_coreCount = m_pApp->getCoreCount();
	//bStream << "b" << ';' ;
	bStream << "{" ;

	std::vector<Task*>* pTaskVect = 0;

	std::vector<Task*>::reverse_iterator itr;

	unsigned taskId = 0;
	unsigned taskUid = 0;
	signed IntId = 0;
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) 
	{

		pTaskVect = m_pApp->getTaskVector(CoreID);
		assert(pTaskVect!=0);


		itr = (*pTaskVect).rbegin();

		size = (*pTaskVect).size();

		for (unsigned TaskCount = 1; itr != (*pTaskVect).rend(); itr++, ++TaskCount) {

			taskId = (*itr)->getId();
			taskUid = (*itr)->getUid();

			IntId = m_pApp->getIntervalID(taskId,CoreID);

			dout << taskUid << "," << (*itr)->getEst() << "," << (*itr)->getWcet() << ","
					<< (*itr)->getRemaining() << "," << (*itr)->getDl() << ","
					<< (*itr)->getType() << "," << CoreID << "," << IntId;

			bStream << "[";
			bStream << taskUid << "," << (*itr)->getEst() << "," << (*itr)->getWcet() << ","
					<< (*itr)->getRemaining() << "," << (*itr)->getDl() << ","
					<< (*itr)->getType() << "," << CoreID << "," << IntId;

			if (TaskCount == size) //last task
				bStream << "],";
			else
				bStream << "],";//if not last one

		}
		//if the tasks are less - fill the remaining ones with -1
		if (size < maxTaskCount) {
			insertDummyTask(maxTaskCount - size, bStream);

		}

	}

	bStream << "}";

	body += bStream.str();
	dout << body;

	return body;

}

//-------------------------------------------------
//! Generate Task array in the following order
/*
 struct ttask
 {
 short int type;                           // determines whether offline guaranteed task (1) or aperiodic (2)
 unsigned wcet, remaining, arrival_time, deadline;
 unsigned interval_id;
 };

 new order:
 struct ttask
 {
 short int uid; // is the same as the ID of the original input file
 unsigned arrival_time, wcet, remaining, deadline;
 short int type;                           // determines whether offline guaranteed task (1) or aperiodic (2)
 short int core_id;
 unsigned interval_id;
  };

 */
//-------------------------------------------------
//!struct tTask task_array[MAX_CORES][5]         =
//	{
//   	{	{1,10,10,0,50,1}, {1,10,10,50,100,2} },
//		{	{-1,0,0,0,0,3}	, {-1,0,0,0,0,4}	 },
//		{	{-1,0,0,0,0,5}	, {-1,0,0,0,0,6} 	 }
//	};
//-------------------------------------------------
string OutFileGenerator::generateTaskArray() {
	dout << "-->>GenerateTaskArray " << '\n';
	string body;
	stringstream bStream;

	unsigned maxTaskCount = m_pApp->getMaxTaskCount();
	unsigned size = 0;
	/*
	 bStream<<"struct tTask task_array["<< mCoreCount << "]"
	 << "["<< maxTaskCount<< "]  = ";

	 */
	bStream << "struct tTask g_task_array[MAX_CORES][MAX_TASKS_PER_CORE] = ";

	bStream << '\n' << "{" << '\n' << '\n';

	std::vector<Task*>* pTaskVect = 0;

	std::vector<Task*>::reverse_iterator itr;

	unsigned taskId = 0;
	unsigned taskUid = 0;
	signed IntId = 0;
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
	{

		dout << "-----------Core:" << CoreID << '\n';
		pTaskVect = m_pApp->getTaskVector(CoreID);
		assert(pTaskVect!=0);




//		m_pApp->sortTaskVector(pTaskVect);





		//m_pApp->printTasks(pTaskVect);

		itr = (*pTaskVect).rbegin();

		bStream << "	{	";

		//coloum
		for (unsigned TaskCount = 1; itr != (*pTaskVect).rend(); itr++, ++TaskCount) {

			taskId = (*itr)->getId();
			taskUid = (*itr)->getUid();
			dout << "TaskCount" << TaskCount << '\n';



			IntId = m_pApp->getIntervalID(taskId,CoreID);

			dout << taskUid << "," << (*itr)->getEst() << "," << (*itr)->getWcet() << ","
					<< (*itr)->getRemaining() << "," << (*itr)->getDl() << ","
					<< (*itr)->getType() << "," << CoreID << "," << IntId;

			bStream << "{";
			bStream << taskUid << "," << (*itr)->getEst() << "," << (*itr)->getWcet() << ","
					<< (*itr)->getRemaining() << "," << (*itr)->getDl() << ","
					<< (*itr)->getType() << "," << CoreID << "," << IntId;

			if (TaskCount == maxTaskCount) //last task
				bStream << "}";
			else
				bStream << "},";//if not last one


		}
		size = (*pTaskVect).size();
		//if the tasks are less - fill the remaining ones with -1
		if (size < maxTaskCount) {
			insertDummyTask(maxTaskCount - size, bStream);

		}

		if (CoreID == m_coreCount) //for last core
			bStream << "			}" << '\n';
		else
			bStream << "			}," << '\n';

	}

	bStream << '\n' << '\n' << "};" << '\n';

	body += bStream.str();
	dout << body;

	dout << "<<--GenerateTaskArray " << '\n';
	return body;
}


string OutTxtFileGenerator::generateAperiodicsArray()
{
	stringstream bStream;
	string body;

	//to store the number_of_aperiodics[MAX_CORES] ARRAY
	stringstream AperiodicsCountArrayStream;

	AperiodicsCountArrayStream << endl ;

	unsigned maxAperiodicsCount = m_pApp->getMaxAperiodicTaskCount();

    	//bStream <<  'q' << ';';
    	bStream <<  '{' ;
	int index = -1;
	std::vector<int>* pAperiodicsVect = 0;
	std::vector<int> :: iterator itr ;
	unsigned aperiodicTaskVecSize = 0;
	AperiodicsCountArrayStream<<m_coreCount<<";";
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
	{

		pAperiodicsVect = m_pApp->getAperiodicsVector(CoreID);

		assert(pAperiodicsVect != 0);

		itr= (*pAperiodicsVect).begin();
		bStream << "[";
		//column
		unsigned TaskCount = 1 ;
		for (; itr != (*pAperiodicsVect).end(); itr++, ++TaskCount)
		{
			index = (*itr);
			bStream << index << ",";
		}
		//bStream << "],";


		aperiodicTaskVecSize = (*pAperiodicsVect).size();
                //if the tasks are less - fill the remaining ones with -1
                if (aperiodicTaskVecSize < maxAperiodicsCount)
                {
                        //bStream <<'[';
                        insertDummyAperiodicIndex(maxAperiodicsCount - aperiodicTaskVecSize, bStream);
                        //bStream <<'],';

                }
		bStream << "],";

		//for the array
		AperiodicsCountArrayStream<<aperiodicTaskVecSize ;

		if(CoreID != m_coreCount)
			AperiodicsCountArrayStream<<";";
		else
			AperiodicsCountArrayStream<<endl;

	}
        bStream << '}';
	body += AperiodicsCountArrayStream.str();
	body += bStream.str();
	return body;

}

/*/
//-------------------------------------------------
// array of aperiodics INDEXes sorted according to arrival time
//! int arrived_aperiodics[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE] =
//-------------------------------------------------*/

string OutFileGenerator::generateAperiodicsArray()
{
	dout << "-->>generateAperiodicsArray " << '\n';
	string body;
	stringstream bStream;


	//to store the number_of_aperiodics[MAX_CORES] ARRAY
	stringstream AperiodicsCountArrayStream;

	AperiodicsCountArrayStream<<"int g_number_of_aperiodics[MAX_CORES] = {";//<<endl;

	unsigned maxAperiodicsCount = m_pApp->getMaxAperiodicTaskCount();

    bStream << "struct tTask* g_aperiodics_index[MAX_CORES][MAX_APERIODICS_ON_ANY_CORE] =  ";
	bStream << '\n' << "{" << '\n' << '\n';

	int index = -1;
	std::vector<int>* pAperiodicsVect = 0;
	std::vector<int> :: iterator itr ;
	unsigned aperiodicTaskVecSize = 0;
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
	{

		dout << "-----------Core: " << CoreID << '\n';
		pAperiodicsVect = m_pApp->getAperiodicsVector(CoreID);

		assert(pAperiodicsVect != 0);

		itr= (*pAperiodicsVect).begin();
		if(itr != (*pAperiodicsVect).end())
		{
			bStream << "	{	(struct tTask*)";
		}
		else
		{
			bStream << "	{	";
		}
		//column
		unsigned TaskCount = 1 ;
		for (; itr != (*pAperiodicsVect).end(); itr++, ++TaskCount)
		{
			index = (*itr);
			bStream << index;


		if (TaskCount == maxAperiodicsCount) //last task
			bStream << " ";
		else
			bStream << ",(struct tTask*)";//if not last one
		}

		aperiodicTaskVecSize = (*pAperiodicsVect).size();

		//if the tasks are less - fill the remaining ones with -1
		if (aperiodicTaskVecSize < maxAperiodicsCount)
		{
			insertDummyAperiodicIndex(maxAperiodicsCount - aperiodicTaskVecSize, bStream);

		}

		if (CoreID == m_coreCount) //for last core
			bStream << "	}" << '\n';
		else
			bStream << "	}," << '\n';


		//for the array
		AperiodicsCountArrayStream<<aperiodicTaskVecSize ;

		if(CoreID != m_coreCount)
			AperiodicsCountArrayStream<<",";
		else
			AperiodicsCountArrayStream<<"};"<<endl;

	}


	bStream << '\n' << '\n' << "};" << '\n';


	body += AperiodicsCountArrayStream.str();
	body += bStream.str();


	dout << body;

	dout << "<<--generateAperiodicsArray " << '\n';
	return body;
}

string OutTxtFileGenerator::generatePeriodicsArray()
{
	string body;
	stringstream bStream;


	//to store the number_of_periodics[MAX_CORES] ARRAY
	stringstream periodicsCountArrayStream;
	periodicsCountArrayStream << endl ;
	periodicsCountArrayStream<< m_coreCount << ";";
	unsigned maxperiodicsCount = m_pApp->getMaxPeriodicTaskCount();

    	//bStream <<  'p' << ';';
	bStream <<  '{';

	int index = -1;
	std::vector<int>* pPeriodicsVect = 0;
	std::vector<int> :: iterator itr ;
	unsigned periodicTaskVecSize = 0;
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
	{

		pPeriodicsVect = m_pApp->getPeriodicsVector(CoreID);

		assert(pPeriodicsVect != 0);



		itr= (*pPeriodicsVect).begin();
		bStream << "[";
		//coloum
		unsigned TaskCount = 1 ;
		for (; itr != (*pPeriodicsVect).end(); itr++, ++TaskCount)
		{
			index = (*itr);
			bStream << index;


		if (TaskCount == maxperiodicsCount) //last task
			bStream << ",";
		else
			bStream << ",";//if not last one
		}

		periodicTaskVecSize = (*pPeriodicsVect).size();

		//if the tasks are less - fill the remaining ones with -1
		if (periodicTaskVecSize < maxperiodicsCount)
		{
	               insertDummyPeriodicIndex(maxperiodicsCount - periodicTaskVecSize, bStream);

		}

		bStream << "]," ;


		//for the array
		periodicsCountArrayStream<<periodicTaskVecSize ;

		if(CoreID != m_coreCount)
			periodicsCountArrayStream<<";";
	}


	bStream << "}" ;

        periodicsCountArrayStream << endl;
	body += periodicsCountArrayStream.str();
	body += bStream.str();


	dout << body;
	return body;
}
/*/
//-------------------------------------------------
// array of periodics INDEXes sorted according to arrival time
//! int periodics_index[MAX_CORES][MAX_PERIODICS_ON_ANY_CORE] =
//-------------------------------------------------*/

string OutFileGenerator::generatePeriodicsArray()
{
	dout << "-->>generatePeriodicsArray " << '\n';
	string body;
	stringstream bStream;


	//to store the number_of_periodics[MAX_CORES] ARRAY
	stringstream periodicsCountArrayStream;

	periodicsCountArrayStream<<"int g_number_of_periodics[MAX_CORES] = {";//<<endl;

	unsigned maxperiodicsCount = m_pApp->getMaxPeriodicTaskCount();

    bStream << "struct tTask* g_periodics_index[MAX_CORES][MAX_PERIODICS_ON_ANY_CORE] =  ";
	bStream << '\n' << "{" << '\n' << '\n';

	int index = -1;
	std::vector<int>* pPeriodicsVect = 0;
	std::vector<int> :: iterator itr ;
	unsigned periodicTaskVecSize = 0;
	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
	{

		dout << "-----------Core: " << CoreID << '\n';
		pPeriodicsVect = m_pApp->getPeriodicsVector(CoreID);

		assert(pPeriodicsVect != 0);



		itr= (*pPeriodicsVect).begin();
		if(itr != (*pPeriodicsVect).end())
		{
			bStream << "	{	(struct tTask*)";
		}
		else
		{
			bStream << "	{	";
		}
		//coloum
		unsigned TaskCount = 1 ;
		for (; itr != (*pPeriodicsVect).end(); itr++, ++TaskCount)
		{
			index = (*itr);
			bStream << index;


		if (TaskCount == maxperiodicsCount) //last task
			bStream << " ";
		else
			bStream << ",(struct tTask*)";//if not last one
		}

		periodicTaskVecSize = (*pPeriodicsVect).size();

		//if the tasks are less - fill the remaining ones with -1
		if (periodicTaskVecSize < maxperiodicsCount)
		{
			insertDummyPeriodicIndex(maxperiodicsCount - periodicTaskVecSize, bStream);

		}

		if (CoreID == m_coreCount) //for last core
			bStream << "	}" << '\n';
		else
			bStream << "	}," << '\n';


		//for the array
		periodicsCountArrayStream<<periodicTaskVecSize ;

		if(CoreID != m_coreCount)
			periodicsCountArrayStream<<",";
		else
			periodicsCountArrayStream<<"};"<<endl;

	}


	bStream << '\n' << '\n' << "};" << '\n';


	body += periodicsCountArrayStream.str();
	body += bStream.str();


	dout << body;

	dout << "<<--generatePeriodicsArray " << '\n';
	return body;
}


//---------------------------------------------------------
//! Generate epilogue of file comments etc.
//---------------------------------------------------------
string
OutFileGenerator::generateEpilogue()
{
	dout << "-->>GenerateEpilogue " << '\n';

	string body;
	stringstream bStream;

#ifdef zero
	bStream << " /* "<<'\n'
			<< " struct tTask"<<'\n'
			<< " {\n"
			<< " short int uid;				// unique ID for each task (as in original input file)\n"
			<< " unsigned arrival_time, wcet, remaining, deadline;\n"
			<< " short int type;                           // determines whether offline guaranteed task (1) or aperiodic (2) or aperiodic already accepted (3) or aperiodic rejected (4) finished execution (99)\n"
			<< " unsigned core_id;\n"
			<< " signed interval_id;\n"
			<< " };\n"
			<< " */\n\n";


	bStream << "/*"<<'\n'
			<< "struct tInterval"<<'\n'
			<< "{"<<'\n'
			<< "   unsigned start, end;"<<'\n'
			<< "   int spare_capacity;"<<'\n'
			<< "   unsigned number_of_tasks;"<<'\n'
			<< "   signed tasks[MAX_TASKS_PER_INTERVAL]; // entries in this array contain task ids"<<'\n'
			<< "}; */"<<'\n';

#endif

#ifdef DEBUGDATA_INFILE

	bStream << " /* "<<'\n' ;

	bStream << "InputFile Name : "<<m_pApp->getIpFileName() <<'\n';
	bStream << "OutputFile Name : "<<m_pApp->getOpFileName() <<".h & .c "<<'\n';

	bStream << "##################### TaskDump ################# "<< '\n';
	std::vector<Task*>* pTaskVect = 0;
	std::vector<Task*>::reverse_iterator itr;
	bStream << "Id   Type   Est   dl   wcet   IntervalID"  << '\n';

	for (unsigned CoreID = 1; CoreID <= m_coreCount; ++CoreID) //row
		{

		bStream << "-----------Core: " << CoreID << '\n';
			pTaskVect = m_pApp->getTaskVector(CoreID);
			itr = (*pTaskVect).rbegin();
			assert(pTaskVect!=0);

			for (int TaskCount = 1; itr != (*pTaskVect).rend(); itr++, ++TaskCount)
			{

				//bStream << "itr" << TaskCount << '\n';


				bStream << (*itr)->getUid()<< ",   " << (*itr)->getType() << ",   " << (*itr)->getEst() << ",   "
						<< (*itr)->getDl() << ",   "
						<< (*itr)->getWcet()<< ",   "<<m_pApp->getIntervalID((*itr)->getId(),CoreID) << '\n';

			}
		}

			bStream << "##################### iNTERVAL dUMP ################# "<< '\n';


	tIntervalMap* pIntVectMap = m_pApp->getInterVectMap();
	map<unsigned, Task*>* pTaskMap = 0;
	map<unsigned, Task*>::iterator TaskItr;
	tIntervalVector::iterator itrIntervalVect;
	assert(pIntVectMap!=0);
	int size = 0;
	for (unsigned coreId = 1; coreId <= m_coreCount; coreId++) // rows
	{
		itrIntervalVect = (*pIntVectMap)[coreId]->begin();


	bStream << '\n' <<"-----------------------CORE "<< coreId<<"---------------- " << '\n' ;
	bStream << "Number of intervals : " <<  (*pIntVectMap)[coreId]->size() <<'\n';
				for (;itrIntervalVect!=(*pIntVectMap)[coreId]->end();itrIntervalVect++)
				{
					bStream<<'\n' << "---INTERVAL Id : "<<(*itrIntervalVect)->getId()<<'\n';
					bStream << " start:"<<(*itrIntervalVect)->getStart() << " end:" <<(*itrIntervalVect)->getEnd()<< " sc:"
							<<(*itrIntervalVect)->getSc()<<" wcet: " <<(*itrIntervalVect)->getSumWcet()<<'\n';

					pTaskMap = (*itrIntervalVect)->getTaskMap();
								assert(pTaskMap!=0);

					TaskItr = pTaskMap->begin();
					size = pTaskMap->size();
					bStream << "num of tasks : " << size<< "	with IDs:	 ";
					//for (int count = 0; count < size; count++, TaskItr++)
					for (;TaskItr != pTaskMap->end(); ++TaskItr)
						if(TaskItr->second != NULL)
							bStream  << (TaskItr)->second->getUid()<< "    ";//<<'\n';


				}
				bStream << '\n' <<"----------------------------------------------  ";


	}

		bStream<< " */"<<'\n' ;

#endif





	body += bStream.str();
	dout << body;
	return body;
}


