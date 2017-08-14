/*
 * OutFileGenerator.h
 *
 *  Created on: Oct 21, 2011
 *      Original-Author: Don Kuzhiyelil
 *	Modified by: Stefan Schorr
 */

#ifndef OutFileGenerator_H_
#define OutFileGenerator_H_
#include "app.h"

class OutFileGenerator
{
public:
	OutFileGenerator(App* pApp);
	virtual ~OutFileGenerator();
	void setFileName(string fName);
	int generateOutFile();
private:
	string m_fileName;
	unsigned m_slotsize;
	unsigned m_lcm;
	unsigned m_coreCount;
	unsigned m_taskNum;
	App* m_pApp;

	//! Generate header file content
	string generateHeaderFileContent();

	//! Generate header for C file
	string generateHeader();
	//! Generate Interval array for C file
	string generateIntervalArray();
	//! Generate Task array for C file
	string generateTaskArray();
	//! Generate Aperiodics array for C file
	string generateAperiodicsArray();
	//! Generate periodics index array for C file
	string generatePeriodicsArray();
	//! Generate epilogue of file comments etc. for C file
	string generateEpilogue();

	//! insert dummy interval and task { -1,-1,-1,-1,-1,-1,-1,-1 }
	void insertDummyInterval(unsigned NoOfDummyInterval,stringstream& bStream) ;
	void insertDummyTask(unsigned NoOfDummyTask,stringstream& bStream) ;
	void insertDummyAperiodicIndex(unsigned NoOfDummyTask,stringstream& bStream) ;
	void insertDummyPeriodicIndex(unsigned NoOfDummyTask,stringstream& bStream) ;


};


class OutTxtFileGenerator
{
public:
        OutTxtFileGenerator(App* pApp);
        virtual ~OutTxtFileGenerator();
        void dummyWrite(void);
	string m_fileName;
	void setFileName(string fName);
	
private:
	string generateIntervalArray(void);
	string generateTaskArray(void);
	string tasksOnCore(void);
	string numOfIntervals(void);
	string generalData(void);
	string generateAperiodicsArray(void);
	string generatePeriodicsArray(void);
        void insertDummyInterval(unsigned NoOfDummyInterval, stringstream& bStream);
        void insertDummyTask(unsigned NoOfDummyTask, stringstream& bStream);
	void insertDummyAperiodicIndex(unsigned NoOfDummyTask,stringstream& bStream) ;
	void insertDummyPeriodicIndex(unsigned NoOfDummyTask,stringstream& bStream) ;
        unsigned m_slotsize;
        unsigned m_lcm;
        unsigned m_coreCount;
        unsigned m_taskNum;
        App* m_pApp;



};


#endif /* OutFileGenerator_H_ */
