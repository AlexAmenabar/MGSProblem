
#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <cstdlib>
#include <cstdio>
#include <string>

struct job_t; 
struct user_t; 
struct workflow_t;


struct allocation_t{

    job_t *job; // job related to allocation
    unsigned int nGPUs;
    unsigned int *gpus; // [nGPUs] 
    unsigned int topoScore; // topology score of the allocation
    unsigned int start, end; // [nIterations]
};

typedef struct job_t{

    // job description
    unsigned int jobId; // job identifier
    unsigned int  *C; // units to compute on each iteration
    unsigned int  *M; // units to communicate on each iteration
    unsigned int  O; // overhead related to reconfigurations
    float S; // scalability of the job (parallel computation)
    unsigned int  CPT; // communication pattern
    unsigned int  nIterations; // number of iterations

    unsigned int *minR, *maxR, *meanR;
    unsigned int totalC, totalM;

    workflow_t *workflow; // workflow associated to the job

    // job state description
    allocation_t *allocation; // [nIterations]
    unsigned int currentIteration; // iteration being executed

} job_t;

typedef struct workflow_t{

    job_t *jobs; // jobs in the workflow
    unsigned int  nJobs; // number of jobs in the workflow

    unsigned int  *dp; // dependencies between the jobs
    unsigned int  launchT; // launch time of the workflow
    unsigned int  priority; // workflow priority

    user_t *user; // user that sent the job

} workflow_t;

typedef struct user_t{

    float  priority; // user priority

} user_t;



struct workload_t {

    unsigned int nWorkflows; // each job can be a workflow composed of only one job
    workflow_t *workflows;
};


// cluster structure
struct cluster_t {

    // cluster description
    unsigned int N; // number of nodes in the system
    unsigned int nGPUs; // total number of GPUs in the system
    
    unsigned int *nGPUsPerNode; // number of GPUs per each node
    unsigned int *firstGPUPerNode; // index of the first GPU per each node
    unsigned int *nodeOfGPU; // node associated to each GPU

    unsigned int *interLatencies; // latencies between GPUs
    unsigned int *interBW; // bandwidths between GPUs

    unsigned int **nodeLatencies;
    unsigned int **nodeBW;

    // cluster state
    unsigned int nAv;
    unsigned int *avGPUs; // [nGPUs]: 1 if available, 0 else
};


enum SIMULATION_TYPES {

    FIXED,
    EVOLVING,
    MALLEABLE
};

struct simulationConf_t{

    SIMULATION_TYPES simType;
};



#endif