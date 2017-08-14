#ifndef EDGE_H
#define EDGE_H


#include <map>


using namespace std;



class Edge 
{
        public:
                Edge(unsigned id, unsigned cost, unsigned msgId)
                :m_id(id)
                ,m_cost(cost)
                ,m_msgId(msgId)
                {
                        ;
                }
                
                
                ~Edge() {;}
                unsigned getId() const               {return m_id;}
                unsigned getCost() const             {return m_cost;}
                unsigned getMsgId() const            {return m_msgId;}

               
                
        private:
                Edge();  // that's not the way to define an edge
                unsigned m_id;
                unsigned m_cost;
                unsigned m_msgId;
};

#endif
