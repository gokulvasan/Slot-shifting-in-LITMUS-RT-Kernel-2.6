#ifndef TASK_H
#define TASK_H


#include <map>
#include "edge.h"
#include <assert.h>

using namespace std;





class Task
{
        public:
                Task(unsigned id, unsigned type , unsigned core, unsigned wcet)
                :m_id(id)
                ,m_type(type)
                ,m_core(core)
                ,m_est(1000000)
                ,m_wcet(wcet)
                ,m_dl(1000000)
                {
                         m_predecessorMap.clear();
                         m_successorMap.clear();
                }

                Task(Task* pTask)
                {
                	assert(pTask != 0);

                	m_id 	=	pTask->getId();
                	m_uid   =   pTask->getUid();
                	m_type	=	pTask->getType();
                    m_core	=	pTask->getCore();
                	m_est	=	pTask->getEst();
                    m_wcet	=	pTask->getWcet();
                    m_dl	=	pTask->getDl();

                	m_predecessorMap.clear();
                	m_successorMap.clear();
                }

                Task(unsigned id, unsigned est,unsigned dl,
                	 unsigned wcet, unsigned core, unsigned type  )
                                :m_id(id),m_type(type)
                                ,m_core(core),m_est(est)
                                ,m_wcet(wcet) ,m_dl(dl)

                                {
                                         m_predecessorMap.clear();
                                         m_successorMap.clear();
                                }

                ~Task() {;}
                unsigned getId() const               {return m_id;}
                unsigned getUid() const               {return m_uid;}
                unsigned getType() const             {return m_type;}
                unsigned getCore() const             {return m_core;}
                unsigned getEst() const              {return m_est;}
                unsigned getPeriod() const           {return m_period;}
                unsigned getWcet() const             {return m_wcet;}
                unsigned getDl() const               {return m_dl;}
                unsigned getRemaining() const         {return m_wcet;}


                map<unsigned, Edge*>& getPredecessorMap()       {return m_predecessorMap;}
                map<unsigned, Edge*>& getSuccessorMap()         {return m_successorMap;}
                void setEst(unsigned x)                         {m_est = x;}
                void setPeriod(unsigned x)                      {m_period = x;}
                void setDl(unsigned x)                          {m_dl = x;}
                void setId(unsigned x)                          {m_id = x;}
                void setUid(unsigned x)                          {m_uid = x;}


        private:
                Task();  // that's not the way to define a task
                unsigned m_id;
                unsigned m_uid;
                unsigned m_type;  // 0 for "normal" task, 1 for aperiodic, etc.
                unsigned m_core;
                unsigned m_est, m_period, m_wcet, m_dl;
                map<unsigned, Edge*> m_predecessorMap, m_successorMap;
};

#endif
