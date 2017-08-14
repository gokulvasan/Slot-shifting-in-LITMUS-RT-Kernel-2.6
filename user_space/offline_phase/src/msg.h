#ifndef MSG_H
#define MSG_H

#include "task.h"

using namespace std;





class Msg 
{
        public:
                Msg(unsigned id, Task* sender, unsigned start, unsigned length, unsigned from, unsigned to)
                :m_id(id)
                ,m_sender(sender)
                ,m_start(start)
                ,m_length(length)
                ,m_from(from)
                ,m_to(to)
                {
                        ;
                }
                
                
                ~Msg() {;}
                unsigned getId() const               {return m_id;}
                unsigned getStart() const            {return m_start;}
                unsigned getLength() const           {return m_length;}
                unsigned getFrom() const             {return m_from;}
                unsigned getTo() const               {return m_to;}
                Task* getSender() const              {return m_sender;}


                
                
        private:
                Msg();  // that's not the way to define a Msg
                unsigned m_id;
                Task* m_sender;
                unsigned m_start;  
                unsigned m_length;
                unsigned m_from, m_to;  //core numbers
};

#endif
