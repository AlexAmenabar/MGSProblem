#include "eventQueue.hpp"


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
            printf("RESCHEDULE");
            break;
        case 2:
            printf("END");
            break;
        case 3:
            printf("END_IT");
            break;
        case 4:
            printf("END_RECONF");
            break;
    }
    printf("\n");
}
