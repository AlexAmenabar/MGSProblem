#pragma once


#include <cstdlib>
#include <cstdio>
#include <string>

// forwarded declarations
struct job_t; 
struct user_t; 
struct workflow_t;
struct event_t;
struct cluster_t;
class EventQueue;
class JobQueue;


/* Simulation configuration */

enum SIMULATION_TYPES {

    FIXED,
    EVOLVING,
    MALLEABLE
};

/* 
Policies to order the pending queue
- FCFS (First-Come First-Served): order just as jobs reach to the system
*/
enum SCHED_POLICIES{

    eQUEUE_FCFS_SCHED
};

/* 
Policies to order the queue of running jobs
- FCFS (First-Come First-Served): order just as jobs reach to the system
- UTIL: place first the jobs with higher utilization values
- NET: place first the jobs with higher network utilization
*/
enum RECONF_POLICIES {

    eQUEUE_NOP,
    eQUEUE_FCFS,
    eQUEUE_UTIL,
    eQUEUE_NET
};

/* 
Policies to decide how much GPUs to allocate for launching the job. Although jobs can be RIGID, the input data always
contains a range of possible GPUs, so we can decide which value to use when launching the job.
- MIN_ALOC: use the minimum value in the range (RIGID)
- MIN_1_ALLOC: allocate only 1 GPU (RIGID)
- MAX_ALLOC: use the maximum value in the range (RIGID)
- MAX_POSSIBLE_ALLOC: allocate the maximum possible in the range (given availability)
- MAX_NODE_ALLOC: allocate the maximum inside a node
- MAX_TOPO_ALLOC: allocate the maximum number of GPUs that also gets the maximum topology score
*/
enum SCHED_ALLOCATION_N_POLICIES{

    e_MIN_ALLOC,
    e_MIN_1_ALLOC,
    e_MAX_ALLOC,
    e_MAX_POSSIBLE_ALLOC,
    e_MAX_NODE_ALLOC,
    e_MAX_TOPO_ALLOC
};

/* 
Policies to decide how much GPUs to allocate for launching the job. Although jobs can be RIGID, the input data always
contains a range of possible GPUs, so we can decide which value to use when launching the job.
*/
enum RECONF_ALLOCATION_N_POLICIES{

    e_EXPAND_SHRINK,
    e_EXPAND,
    e_EXPAND_SHRINK_MIGRATE,
    e_EXPAND_SHRINK_UNLIMITED,
    e_EXPAND_UNLIMITED,
    e_EXPAND_SHRINK_MIGRATE_UNLIMITED
};

/* 
Policies to decide which GPUs will execute the job
- SEQUENTIAL: allocate GPUs sequentially
- TOPO_BEST_EFFORT: allocate GPUs with the best topological score
- TOPO_OPT: launch only if the optimal topological score is achieved
*/
enum ALLOCATION_PLACEMENT_POLICIES{

    e_SEQUENTIAL,
    e_TOPO_BEST_EFFORT,
    e_TOPO_OPT
};


// simulation configuration structure
struct simulationConf_t{

    SIMULATION_TYPES simType;

    // queue orderings
    SCHED_POLICIES schedPolicy;
    RECONF_POLICIES reconfPolicy;

    // Number of GPUs selection
    SCHED_ALLOCATION_N_POLICIES schedAllocationN;
    RECONF_ALLOCATION_N_POLICIES reconfAllocationN;

    // placement selection
    ALLOCATION_PLACEMENT_POLICIES schedAllocationPlace;
    ALLOCATION_PLACEMENT_POLICIES reconfAllocationPlace;
};


/* Events */
enum EVENT_ENUM {

    ARRIVAL,
    END,
    END_IT,
    END_RECONF

};

struct event_t {

    job_t *job; // job related to the event
    EVENT_ENUM eventEnum; // event 
    unsigned int t; // time step in which the event occur
};

// create an event
event_t* createEvent(job_t *job, EVENT_ENUM eventEnum, unsigned int t);

// print event data
void printEvent(event_t *event);

// process all events of a given time step
bool processEventsTimeStep(EventQueue *evQueue, unsigned int t, JobQueue *pQueue, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf); 

// process an event
bool processEvent(event_t *event, EventQueue *evQueue, JobQueue *pQueue, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf);


/* Jobs */

// resource allocation(s) for job
struct allocation_t{

    job_t *job; // job related to allocation
    unsigned int nGPUs;
    unsigned int *gpus; // [nGPUs] 
    unsigned int topoScore; // topology score of the allocation
    unsigned int start, end; // [nIterations]

    // metrics
    unsigned int throughput;
    unsigned int util; // GPU utilization
};


// job structure
typedef struct job_t{

    // job description
    unsigned int jobId; // job identifier
    unsigned int  *C; // units to compute on each iteration
    unsigned int  *M; // units to communicate on each iteration
    unsigned int  O; // overhead related to reconfigurations
    float *S; // scalability of the job (parallel computation) on each iteration
    unsigned int  CPT; // communication pattern
    unsigned int  nIterations; // number of iterations
    unsigned int m; // whether job is or not malleable

    unsigned int *minR, *maxR, *meanR;
    unsigned int totalC, totalM;

    workflow_t *workflow; // workflow associated to the job

    // job state description
    allocation_t *allocation; // [nIterations]
    unsigned int currentIteration; // iteration being executed
    bool rcf; // reconfiguration programmed for job

    // job data
    unsigned int sPending, ePending, sRunning, eRunning; // time steps in which job started and ended being pending and running

} job_t;

// workflow structure
typedef struct workflow_t{

    job_t *jobs; // jobs in the workflow
    unsigned int  nJobs; // number of jobs in the workflow

    unsigned int  *dp; // dependencies between the jobs
    unsigned int  launchT; // launch time of the workflow
    unsigned int  priority; // workflow priority

    user_t *user; // user that sent the job

} workflow_t;

// user structure 
typedef struct user_t{

    float  priority; // user priority

} user_t;


// workload structure
struct workload_t {

    unsigned int nWorkflows; // each job can be a workflow composed of only one job
    workflow_t *workflows;
};

// launch a job or continue with its execution
void launchJob(EventQueue *evQueue, unsigned int t, job_t *job, cluster_t *cluster, simulationConf_t *simConf);

// compute total job time for RIGID jobs
unsigned int computeTotalTime(job_t *job, cluster_t *cluster);

// compute iteration time for MALLEABLE and EVOLVING jobs
unsigned int computeIterationTime(job_t *job, cluster_t *cluster);

// Compute computation related time | Tcomp = C * S / P + (1-S) * C
unsigned int computeComputationTime(unsigned int C, unsigned int N, float S);

// compute communication related time |
unsigned int computeCommunicationTime(unsigned int M, allocation_t *alloc, cluster_t *cluster);

// reconfigure a job
void reconfigureJob(EventQueue *evQueue, unsigned int t, job_t *job, cluster_t *cluster, simulationConf_t *simConf);

// compute reconfiguration time for a job
unsigned int computeReconfigurationTime(job_t *job);

unsigned int getUtilization(job_t* job, unsigned int computationTime, unsigned int communicationTime);

unsigned int getNetworkThroughput(job_t* job, unsigned int communicationTime);


/* Cluster */

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
    unsigned int *nAvPerNode;
    unsigned int *avGPUs; // [nGPUs]: 1 if available, 0 else


    // topological properties 
};


/* Register resource allocations and deallocations in the system */
void allocateResources(allocation_t *alloc, cluster_t *cluster);
void deallocateResources(allocation_t *alloc, cluster_t *cluster);

/* allocation policies for scheduling */

// allocate the minimum number of resources in the range
allocation_t* allocateResourcesRIGID_min(job_t *job, cluster_t *cluster, simulationConf_t *simConf);
allocation_t* allocateResourcesRIGID_min1(job_t *job, cluster_t *cluster, simulationConf_t *simConf);
allocation_t* allocateResourcesRIGID_max(job_t *job, cluster_t *cluster, simulationConf_t *simConf);
allocation_t* allocateResourcesMOLDABLE_max(job_t *job, cluster_t *cluster, simulationConf_t *simConf);
allocation_t* allocateResourcesMOLDABLE_maxNode(job_t *job, cluster_t *cluster, simulationConf_t *simConf);
allocation_t* allocateResourcesTOPO(job_t *job, cluster_t *cluster, simulationConf_t *simConf);

/* Reconfiguration policies */
allocation_t *rcfExpandShrink(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);
allocation_t *rcfExpand(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);
allocation_t *rcfExpandShrinkMigrate(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);
allocation_t *rcfExpandShrinkUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);
allocation_t *rcfExpandUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);
allocation_t *rcfExpandShrinkMigrateUnlimited(job_t *job, JobQueue *jQueue, cluster_t *cluster, simulationConf_t *simConf);