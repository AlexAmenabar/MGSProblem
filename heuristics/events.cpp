#include "libMGSP.hpp"
#include "queue.hpp"
#include "eventQueue.hpp"
#include "jobQueue.hpp"

event_t* createEvent(job_t *job, EVENT_ENUM eventEnum, unsigned int t){

    // allocate memory for event structure
    event_t *event = (event_t*)calloc(1,sizeof(event_t));

    // initialize event 
    event->job = job;
    event->eventEnum = eventEnum;
    event->t = t;

    // return event
    return event;
}

void printEvent(event_t *event){

    printf(" > [Event] Job id: %u | Job priority: %u | Time step: %u | Enum: ", event->job->jobId, event->job->workflow->priority, event->t);

    switch(event->eventEnum){
        case 0:
            printf("ARRIVAL");
            break;
        case 1:
            printf("END");
            break;
        case 2:
            printf("END_IT");
            break;
        case 3:
            printf("END_RCF");
            break;
    }
    printf("\n");
}


bool processEventsTimeStep(EventQueue *evQueue, unsigned int t, JobQueue *pQueue, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf){

    // get first node in the queue
    Node<event_t*> *eventNode = evQueue->getElementByIndex(0);
    bool p = false;

    // process while the time step of the event is the same of t 
    while(eventNode && eventNode->data->t == t){

        // get event from node
        event_t *event = eventNode->data;

        // process event
        bool r = processEvent(event, evQueue, pQueue, rQueue, fQueue, cluster, simConf);

        // keep true if p is already true
        p = p | r;

        // remove first node from the queue and get the next
        evQueue->removeElementByIndex(0);
        eventNode = evQueue->getElementByIndex(0);
    }

    evQueue->printEvents();
    printf("\n");
    fflush(stdout);

    return p;
} 

bool processEvent(event_t *event, EventQueue *evQueue, JobQueue *pQueue, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf){

    switch(event->eventEnum){

        case ARRIVAL: {

            // create job node
            Node<job_t*> *jobNode = (Node<job_t*>*)calloc(1,sizeof(Node<job_t*>));
            jobNode->data = event->job; 
            
            // job is pending
            jobNode->data->sPending = event->t;

            // add the job to the pending queue
            pQueue->addElementPolicy(jobNode);

            return true;
            break;
        }
        case END: {

            Node<job_t*> *jobNode = (Node<job_t*>*)calloc(1,sizeof(Node<job_t*>));
            jobNode->data = event->job; 
            job_t *job = event->job;
            
            // job finished running
            jobNode->data->eRunning = event->t;

            // deallocate resources of the current iteration
            deallocateResources(&job->allocation[job->currentIteration], cluster);
            
            // if job had a programmed reconfiguration, deallocate more resources
            if(job->rcf){

                deallocateResources(&job->allocation[job->currentIteration + 1], cluster);
            }

            // take out job from running queue and add it to finished jobs queue
            rQueue->removeJob(job);
            fQueue->addElement(jobNode);

            job->currentIteration ++;

            return true;
            break;
        }
        case END_IT: {

            job_t *job = event->job;

            // if there is a pending reconfiguration, perform it. Else, execute next iteration
            if(job->rcf){

                // call for reconfiugration
                reconfigureJob(evQueue, event->t, job, cluster, simConf);
            }
            else{

                // execute the next iteration of the job
                job->allocation[job->currentIteration].end = event->t;

                job->currentIteration ++;
                job->allocation[job->currentIteration] = job->allocation[job->currentIteration-1];
                launchJob(evQueue, event->t + 1, job, cluster, simConf);
            }

            return false;
            break;
        }
        case END_RECONF: {

            // job finished reconfiguration, so move to the next iteration
            job_t *job = event->job;

            // deallocate resources of the current iteration
            deallocateResources(&job->allocation[job->currentIteration], cluster);
            job->allocation[job->currentIteration].end = event->t;

            // move to the next iteration
            job->currentIteration ++;
            job->rcf = false;

            // allocate the resources of the next iteration
            allocateResources(&job->allocation[job->currentIteration], cluster);

            // launch iteration
            launchJob(evQueue, event->t + 1, job, cluster, simConf);

            return true;
            break;
        }
    }
    
    return false;
}
