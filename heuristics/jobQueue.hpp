#include "queue.hpp"
#include "instance.hpp"

#ifndef JOBQUEUE_HPP
#define JOBQUEUE_HPP

class JobQueue : public LinkedList<job_t*> {
public:
    void addElementByPriority(Node<job_t*> *node){

        Node<job_t*> *aux = first;
        unsigned int i = 0;

        while(aux && aux->data->workflow->priority >= node->data->workflow->priority){

            aux = aux->next;
            i++;
        }

        // if not aux, add as the last element
        if(!aux){

            addElement(node);
        }
        else{

            node->next = aux;
            node->prev = aux->prev;
            aux->prev = node;
            
            if(aux->prev)
                aux->prev->next = node;
            else
                first = node;

            N++;
        }
    }

};

#endif