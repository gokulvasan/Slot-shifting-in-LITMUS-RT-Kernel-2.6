#ifndef PG_H
#define PG_H


#include <map>
#include "msg.h"
#include "task.h"
#include <vector>

using namespace std;



class PG
{
        public:
                PG(unsigned id, unsigned est, unsigned dl)
                :m_id(id)
                ,m_est(est)
                ,m_dl(dl)
                {
                        m_taskMap.clear();
                        m_msgMap.clear();
                        m_taskVector.clear();
                }


                ~PG() {;}
                unsigned getId() const               {return m_id;}
                unsigned getEst() const              {return m_est;}
                unsigned getDl() const               {return m_dl;}
                map<unsigned, Task*>& getTaskMap()   {return m_taskMap;}
                map<unsigned, Msg*>& getMsgMap()     {return m_msgMap;}

                vector<Task*>& getTaskVector()     {return m_taskVector;}

        private:
                PG();  // that's not the way to define a PG
                unsigned m_id;
                unsigned m_est;
                unsigned m_dl;
                map<unsigned, Task*> m_taskMap;
                map<unsigned, Msg*> m_msgMap;

                vector<Task*> m_taskVector;
};

#endif
