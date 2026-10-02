#include "heuristics.hpp"

#include "instance.hpp"
#include "queue.hpp"
#include "jobQueue.hpp"
#include "eventQueue.hpp"
#include "job.hpp"

// helper functions
allocation_t* allocateResourcesNaive(job_t *job, unsigned int reqN, cluster_t *cluster){

    // allocate resources for allocation
    allocation_t *alloc = (allocation_t*)calloc(1, sizeof(allocation_t));
    alloc->nGPUs = reqN;
    alloc->gpus = (unsigned int*)calloc(reqN, sizeof(unsigned int));

    // loop over GPUs in the cluster and allocate them
    unsigned int nAllocated = 0, gpu = 0;
    while(nAllocated < reqN){

        // add GPU to allocation if available
        if(cluster->avGPUs[gpu]){

            alloc->gpus[nAllocated] = gpu;
            cluster->avGPUs[gpu] = 0; // gpu not available until job finishes
            nAllocated ++;
        }

        gpu++;
    }

    //
    cluster->nAv -= reqN;

    // link allocation and job
    alloc->job = job;
    job->allocation = alloc;

    return alloc;
}

void deallocateResources(job_t *job, cluster_t *cluster){

    // get allocation
    allocation_t *alloc = &(job->allocation[job->currentIteration]);

    // loop over GPUs in the allocation
    for(unsigned int i = 0; i<alloc->nGPUs; i++){

        cluster->avGPUs[alloc->gpus[i]] = 1; // GPU is again available
    }

    cluster->nAv += alloc->nGPUs;
}

// execute a job iteration
event_t* executeIteration(event_t *event, job_t *job, cluster_t *cluster){

    // 
    unsigned int itTime = computeIterationTime(job, cluster);

    // create the event corresponding to the end of the job
    event_t *endEvent = createEvent(job, END, event->t + itTime); // job will end at event->t + iteration time

    // return event
    return endEvent;
}


// Scheduling and reconfiguration policies
void FCFS(EventQueue *evQueue, event_t *event, cluster_t *cluster, simulationConf_t *simConf){

    // get job from event
    job_t *job = event->job;
    unsigned int it = job->currentIteration;

    // min and max GPUs job can use
    unsigned int minR, maxR;
    switch(simConf->simType){
        
        // the number of resources is not a range, and always the first is used
        case FIXED:
            minR = job->meanR[0];
            maxR = job->meanR[0];
            break;
        // the number of resources is fixed, but different on each iteration
        case EVOLVING:
            minR = job->meanR[it];
            maxR = job->meanR[it];
            break;
        // the number of resources its a range, the same for all iterations
        case MALLEABLE:
            minR = job->minR[0];
            maxR = job->maxR[0];
            break;
    }

    // JUST THE FIXED VERSION BY NOW

    // launch job if there are enough resources
    Node<event_t*> *eventNode = (Node<event_t*>*)calloc(1, sizeof(Node<event_t*>));

    if(cluster->nAv >= minR){

        // allocate resources for the job
        allocation_t *alloc = allocateResourcesNaive(job, minR, cluster);
        
        //
        event_t *endEvent = executeIteration(event, job, cluster);
        eventNode->data = endEvent;

        printf(" -- Job %u launched with %u at %u (%u)\n", job->jobId, alloc->nGPUs, event->t, eventNode->data->t);
        fflush(stdout);
    }
    else{

        // find next end event

        // create reschedule event and add it to the next end event
        event_t *rescheduleEvent = createEvent(job, RESCHEDULE, evQueue->findNextEndEvent()->t);
        eventNode->data = rescheduleEvent;


        //printf(" -- Job %u rescheduled from %u to %u\n", job->jobId, event->t, evQueue->findNextEndEvent()->t);
        //fflush(stdout);
    }
    evQueue->addElementByTimeStep(eventNode);

    evQueue->printEvents();
    printf("\n");
}