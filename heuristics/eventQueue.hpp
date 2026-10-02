#include "queue.hpp"
#include "instance.hpp"

#ifndef EVENTQUEUE_HPP
#define EVENTQUEUE_HPP

enum EVENT_ENUM {

    ARRIVAL,
    RESCHEDULE,
    END,
    END_IT,
    END_RECONF

};

struct event_t {

    job_t *job; // job related to the event
    EVENT_ENUM eventEnum; // event 
    unsigned int t; // time step in which the event occur
};


event_t* createEvent(job_t *job, EVENT_ENUM eventEnum, unsigned int t);
void printEvent(event_t *event);

class EventQueue : public LinkedList<event_t*> {

public:

    void addElementByTimeStep(Node<event_t*> *node){

        Node<event_t*> *aux = first;
        unsigned int i = 0;

        while(aux && // if higher time, lower priority or event not END
                (node->data->t > aux->data->t || 
                (node->data->t == aux->data->t && node->data->job->workflow->priority >= aux->data->job->workflow->priority) ||
                (node->data->t == aux->data->t && node->data->eventEnum != END && aux->data->eventEnum == END))){

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
            
            if(aux->prev)
                aux->prev->next = node;
            else
                first = node;

            aux->prev = node;

            N++;
        }
    }

    event_t* findNextEndEvent(){

        Node<event_t*> *node = first;

        while(node && node->data->eventEnum != END){

            node = node->next;
        }

        return node->data;
    }

    void updateJobPriorities(){

        Node<event_t*> *evNode = first;
        unsigned int priority = 0;

        while(evNode){

            evNode->data->job->workflow->priority = priority;
            evNode = evNode->next;
            priority ++;
        }
    }

    void printEvents(){

        // get node
        Node<event_t*> *node = first;

        while(node){

            printEvent(node->data);
            node = node->next;
        }
        
        fflush(stdout);
    }
};

#endif