#include "libMGSP.hpp"
#include "jobQueue.hpp"

void setQueueAllocationPolicy(JobQueue *queue, simulationConf_t *simConf){


    switch(simConf->schedAllocationN){
        
        case e_MIN_ALLOC:
            queue->schedAllocator = &allocateResourcesRIGID_min;
            break;
        case e_MIN_1_ALLOC:
            queue->schedAllocator = &allocateResourcesRIGID_min1;
            break;
        case e_MAX_ALLOC:
            queue->schedAllocator = &allocateResourcesRIGID_max;
            break;
        case e_MAX_POSSIBLE_ALLOC:
            queue->schedAllocator = &allocateResourcesMOLDABLE_max;
            break;    
        case e_MAX_NODE_ALLOC:
            queue->schedAllocator = &allocateResourcesMOLDABLE_maxNode;
            break; 
        case e_MAX_TOPO_ALLOC:
            queue->schedAllocator = &allocateResourcesTOPO;
            break; 
    }
}

void setQueueReconfigurationPolicy(JobQueue *queue, simulationConf_t *simConf){

    switch(simConf->reconfAllocationN){
        case e_EXPAND_SHRINK:
                queue->reconfAllocator = &rcfExpandShrink;
            break;
        case e_EXPAND:
                queue->reconfAllocator = &rcfExpand;
            break;
        case e_EXPAND_SHRINK_MIGRATE:
                queue->reconfAllocator = &rcfExpandShrinkMigrate;
            break;
        case e_EXPAND_SHRINK_UNLIMITED:
                queue->reconfAllocator = &rcfExpandShrinkUnlimited;
            break;
        case e_EXPAND_UNLIMITED:
                queue->reconfAllocator = &rcfExpandUnlimited;
            break;
        case e_EXPAND_SHRINK_MIGRATE_UNLIMITED:
            queue->reconfAllocator = &rcfExpandShrinkMigrateUnlimited;
            break;
    }
}

// initialize 
JobQueue* initSchedQueue(simulationConf_t *simConf){

    switch(simConf->schedPolicy){

        case eQUEUE_FCFS_SCHED:
            JobQueueFCFS *queue = new JobQueueFCFS();
            setQueueAllocationPolicy(queue, simConf);
            return queue;
    }

    printf("Non valid scheduling policy!\n");
    return NULL;
}

// reconfiguration policies (queue orderings)
JobQueue* initReconfQueue(simulationConf_t *simConf){

    switch(simConf->reconfPolicy){

        case eQUEUE_NOP: {
            JobQueueNOP *nopqueue = new JobQueueNOP();
            return nopqueue;
            break;
        }

        case eQUEUE_FCFS: {
            JobQueueFCFS *fcfsqueue = new JobQueueFCFS();
            setQueueReconfigurationPolicy(fcfsqueue, simConf);
            return fcfsqueue;
            break;
        }
        /*case eQUEUE_UTIL:
            break;
        case eQUEUE_NET:
            break;*/
    }

    printf("Non valid scheduling policy!\n");
    return NULL; 
}