#include "libMGSP.hpp"
#include "jobQueue.hpp"


/* Initialize cluster topology score */
//TODO
void initializeTopologyRank(cluster_t *cluster){


}

// TODO
float getTopoScore(allocation_t *alloc, cluster_t *cluster){

    unsigned int score = 0;

    return 0.0;
}


/* 
    Register resources allocations and deallocations 
*/

void allocateResources(allocation_t *alloc, cluster_t *cluster){

    // loop over GPUs in the allocation
    for(unsigned int i = 0; i<alloc->nGPUs; i++){

        cluster->avGPUs[alloc->gpus[i]] = 0; // GPU is not available

        cluster->nAvPerNode[cluster->nodeOfGPU[alloc->gpus[i]]] ++;
    }

    cluster->nAv -= alloc->nGPUs;
}

void deallocateResources(allocation_t *alloc, cluster_t *cluster){

    // loop over GPUs in the allocation
    for(unsigned int i = 0; i<alloc->nGPUs; i++){

        cluster->avGPUs[alloc->gpus[i]] = 1; // GPU is again available

        cluster->nAvPerNode[cluster->nodeOfGPU[alloc->gpus[i]]] --;
    }

    cluster->nAv += alloc->nGPUs;
}


/* 
    Helper functions: find available resources sequentially 
*/
allocation_t* findResourcesSequential(unsigned int n, cluster_t *cluster){

    // allocate resources for allocation
    allocation_t *alloc = (allocation_t*)calloc(1, sizeof(allocation_t));
    alloc->nGPUs = n;
    alloc->gpus = (unsigned int*)calloc(n, sizeof(unsigned int));

    // loop over GPUs in the cluster and find the available ones
    unsigned int nAllocated = 0, gpu = 0;
    while(nAllocated < n && gpu < cluster->nGPUs){

        // add GPU to allocation if available
        if(cluster->avGPUs[gpu]){

            alloc->gpus[nAllocated] = gpu;
            nAllocated ++;
        }

        gpu++;
    }

    // no enough resources available, return NULL
    if(nAllocated < n){
        free(alloc->gpus);
        free(alloc);
        return nullptr;
    }

    return alloc;
}


/* 
    Expansions and shrinkages 
*/

allocation_t* reconfigureResourcesSequential(job_t *job, allocation_t *prevAlloc, unsigned int n, cluster_t *cluster){

    // new resource allocation
    allocation_t *alloc = (allocation_t*)calloc(1, sizeof(allocation_t));
    alloc->nGPUs = n;
    alloc->gpus = (unsigned int*)calloc(n, sizeof(unsigned int));


    unsigned int nAllocated = prevAlloc->nGPUs; // allocated from previous allocation
    unsigned int prevGPUIndex = 0;
    unsigned int gpuIndex = 0;

    // loop over GPUs in the cluster and find the available ones

    if(n > prevAlloc->nGPUs){
    
        for(unsigned int g = 0; g<prevAlloc->nGPUs; g++){

            alloc->gpus[g] = prevAlloc->gpus[g];
        }

        while(nAllocated < n && gpuIndex < cluster->nGPUs){

            // add GPU to allocation if available
            if(cluster->avGPUs[gpuIndex]){

                alloc->gpus[nAllocated] = gpuIndex;
                nAllocated ++;
            }

            gpuIndex++;
        }
    }
    else if(n < prevAlloc->nGPUs){
        
        for(unsigned int i = 0; i < n; i ++){
            
            alloc->gpus[i] = prevAlloc->gpus[i];  
        }
    }

    // no enough resources available, return NULL
    if(nAllocated < n){
        free(alloc->gpus);
        free(alloc);
        return NULL;
    }

    return alloc;
}


/*  
    Possible resource allocation methods:
    - Sequential: GPUs are allocated sequentially, as they are found
    - Topology-aware best-effort: allocation is done topology-aware, but it is not necessarily the optimal placement | launch if enough GPUs are available using the best possible topological placement
    - Topology optimal: launch only if the optimal topological score is achievable for the number of GPUs
*/


/* 
    Resource allocation functions for RIGID jobs. They use the maxR[0] as the resource request of the job 
*/

allocation_t *findAllocation(unsigned int N, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    switch(simConf->schedAllocationPlace){

        case e_SEQUENTIAL:
            alloc = findResourcesSequential(N, cluster);
            break;
        // TODO
        case e_TOPO_BEST_EFFORT:
            break;
        case e_TOPO_OPT:
            break;
    }

    return alloc;
}


// allocate the minimum number of resources in the range
allocation_t* allocateResourcesRIGID_min(job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // rigid jobs, use the mean of the first iteration
    unsigned int reqN = job->minR[0];

    // find resources
    return findAllocation(reqN, cluster, simConf);
    
}

allocation_t* allocateResourcesRIGID_min1(job_t *job, cluster_t *cluster, simulationConf_t *simConf){
    
    // rigid jobs, use the mean of the first iteration
    unsigned int reqN = 1;

    // find resources
    return findAllocation(reqN, cluster, simConf);
}

allocation_t* allocateResourcesRIGID_max(job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // rigid jobs, use the mean of the first iteration
    unsigned int reqN = job->maxR[0];

    // find resources
    return findAllocation(reqN, cluster, simConf);

}

allocation_t* allocateResourcesMOLDABLE_max(job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // [TODO]
    return nullptr;
}

allocation_t* allocateResourcesMOLDABLE_maxNode(job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // [TODO]
    return nullptr;
}

allocation_t* allocateResourcesTOPO(job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // [TODO]
    return nullptr;
}


/* 
Reconfigurations related decisions
*/

allocation_t *allocateReconfigurationResources(job_t *job, allocation_t *prevAlloc, unsigned int n, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;

    switch(simConf->reconfAllocationPlace){

        case e_SEQUENTIAL:
            alloc = reconfigureResourcesSequential(job, prevAlloc, n, cluster);
            break;
        case e_TOPO_BEST_EFFORT:
            //TODO
            break;
        case e_TOPO_OPT:
            //TODO
            break;
    }

    return alloc;
}

allocation_t *rcfExpandShrink(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    unsigned int jobGPUs = job->allocation[job->currentIteration].nGPUs;
    
    // expand if utilization is high
    if(jQueue->expandCriteria(job) && jobGPUs < job->maxR[0]){
        
        unsigned int nGPUs = jobGPUs * 2;

        if(cluster->nAv + jobGPUs < nGPUs){
            
            nGPUs = cluster->nAv + jobGPUs;
        }

        if(nGPUs > job->maxR[0])
            nGPUs = job->maxR[0];

        if(nGPUs == jobGPUs)
            return alloc;

        printf("Job %u expanding from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }
    else if(jQueue->shrinkCriteria(job) && jobGPUs > job->minR[0]){

        unsigned int nGPUs = (unsigned int)((float)jobGPUs / (float)2);

        if(nGPUs < job->minR[0])
            nGPUs = job->minR[0];

        if(nGPUs == jobGPUs)
            return alloc;

        printf("Job %u expanding from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }

    return alloc;
}

allocation_t *rcfExpand(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    unsigned int jobGPUs = job->allocation[job->currentIteration].nGPUs;
    
    // expand if utilization is high
    if(jQueue->expandCriteria(job) && jobGPUs < job->maxR[0]){
        
        unsigned int nGPUs = jobGPUs * 2;

        if(cluster->nAv + jobGPUs < nGPUs){
            
            nGPUs = cluster->nAv + jobGPUs;
        }

        if(nGPUs > job->maxR[0])
            nGPUs = job->maxR[0];

        if(nGPUs == jobGPUs)
            return alloc;

        printf("Job %u expanding from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }
    
    return alloc;
}

allocation_t *rcfExpandShrinkMigrate(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    
    // TODO
    
    return alloc;
}

allocation_t *rcfExpandShrinkUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    unsigned int jobGPUs = job->allocation[job->currentIteration].nGPUs;
    
    // expand if utilization is high
    if(jQueue->expandCriteria(job) && cluster->nAv > 0){
        
        // try to double the number of GPUs
        unsigned int nGPUs = jobGPUs * 2;

        if(cluster->nAv + jobGPUs < nGPUs){
            
            nGPUs = cluster->nAv + jobGPUs;
        }

        printf("Job %u expanding from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }
    else if(jQueue->shrinkCriteria(job) && jobGPUs > 1){

        unsigned int nGPUs = (unsigned int)((float)jobGPUs / (float)2);

        printf("Job %u expanding from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }

    return alloc;
}

allocation_t *rcfExpandUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    unsigned int jobGPUs = job->allocation[job->currentIteration].nGPUs;
    
    // expand if utilization is high
    if(jQueue->expandCriteria(job) && cluster->nAv > 0){
        
        // try to double the number of GPUs
        unsigned int nGPUs = jobGPUs * 2;

        if(cluster->nAv + jobGPUs < nGPUs){
            
            nGPUs = cluster->nAv + jobGPUs;
        }

        printf("Job %u expanding (expand unlimited) from %u to %u\n", job->jobId, jobGPUs, nGPUs);
        fflush(stdout);

        // try to allocate resources
        alloc = allocateReconfigurationResources(job, &job->allocation[job->currentIteration], nGPUs, cluster, simConf);
    }
    
    return alloc;
}


allocation_t *rcfExpandShrinkMigrateUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf){

    allocation_t *alloc = nullptr;
    
    // TODO
    
    return alloc;
}