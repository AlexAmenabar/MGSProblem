#pragma once

#include "queue.hpp"
#include "libMGSP.hpp"


/* Event queue */

class EventQueue : public LinkedList<event_t*> {
public:

    // add element to the queue in the corresponding position depending on the time step and eventEnum related to the event
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

    // find the first EVENT in the queue
    event_t* findNextEvent(EVENT_ENUM EVENT){
        
        Node<event_t*> *node = first;

        while(node && node->data->eventEnum != EVENT){

            node = node->next;
        }

        return node->data;
    }

    // initialize job priorities when only arrivals are loaded in the queue
    void initializeJobPriorities(){

        Node<event_t*> *evNode = first;
        unsigned int priority = 0;

        while(evNode){

            evNode->data->job->workflow->priority = priority;
            evNode = evNode->next;
            priority ++;
        }
    }

    // print all events data in the queue
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
