#ifndef INTERVAL_H
#define INTERVAL_H

#include "task.h"

using namespace std;





class Interval
{
        public:
                Interval(unsigned id)
                :m_id(id)
                {
                        m_core  = 1000000;
                        m_start = 1000000;
                        m_end   = 0;
                        m_sc    = -1000000;
                        m_sumWcet = 100000;
                        m_taskMap.clear();
                }
                Interval(unsigned id,unsigned core,unsigned start,unsigned end,unsigned sc = -1000000)
					:m_id(id),m_core(core),m_start(start),m_end(end),m_sc(sc)
					{
                        m_taskMap.clear();
                        m_sumWcet = 0 ;

					}

                ~Interval()
                {
                        m_taskMap.clear();


                }
                unsigned getId() const               {return m_id;}
                unsigned getStart() const            {return m_start;}
                unsigned getEnd() const              {return m_end;}
                int getSc() const                    {return m_sc;}
                int getCore() const                  {return m_core;}
                map<unsigned, Task*>* getTaskMap()   {return &m_taskMap;}

                void setId(int x)                               {m_id = x;}
                void setCore(int x)                             {m_core = x;}
                void setSc(int x)                               {m_sc = x;}
                void setStart(unsigned x)                       {m_start = x;}
                void setEnd(unsigned x)                         {m_end = x;}
                void setTaskMap(map<unsigned, Task*>* x)        {m_taskMap = *x;}

                void setSumWcet(unsigned sumWcet) { m_sumWcet = sumWcet; };
                unsigned getSumWcet() { return m_sumWcet; };

               void addTask(unsigned index, Task* pTask)
               {
//TODO  Interval::addTask() -  no safety check here!
            	   m_taskMap[index] = pTask ;
               }


        private:
                Interval();  // that's not the way to define an Interval
                unsigned m_id;
                unsigned m_core;
                unsigned m_start;
                unsigned m_end;
                int m_sc; //  spare capacity
                map<unsigned, Task*> m_taskMap;
                unsigned m_sumWcet ;  //sum of all wcets in the interval
};

#endif
