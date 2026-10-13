// simulator

#include <cstdlib>
#include <cstdio>
#include <string>

#include "libMGSP.hpp"
#include "queue.hpp"
#include "eventQueue.hpp"
#include "jobQueue.hpp"
#include "IO.hpp"

#include "cJSON.h"


// g++ -g -I../instances/cJSON/ -o sim sim.cpp ../instances/cJSON/cJSON.c resources.cpp jobQueue.cpp events.cpp job.cpp rcfPolicies.cpp IO.cpp
cJSON* readJSONFile(char *in){
    
    // open file 
    FILE *file = fopen(in, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: could not open file '%s'\n", in);
        return NULL;
    }

    // get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fprintf(stderr, "Error: input file is empty\n");
        fclose(file);
        return NULL;
    }

    // read file and load to buffer
    char *buffer = (char *)malloc(fileSize + 1);

    if (buffer == NULL) {
        fprintf(stderr, "Error: could not allocate memory for JSON file\n");
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    buffer[bytesRead] = '\0';

    fclose(file);


    // parse the file content to JSON
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);

    return root;
}


// open workload file and load the JSON information to a workload_t structure
workload_t* loadWorkload(char *in){

    // load file information into a JSON structure
    cJSON *root = readJSONFile(in);

    // allocate memory for the workload
    workload_t *workload = (workload_t*)calloc(1, sizeof(workload_t));

    // read number of jobs/workflows in the workload
    workload->nWorkflows = (unsigned int)cJSON_GetObjectItem(root, "nWorkflows")->valueint;

    // allocate memory for each workflow/job
    workload->workflows = (workflow_t*)calloc(workload->nWorkflows, sizeof(workflow_t));

    unsigned int priority = 0; 

    // loop over workflows/jobs and load each of them
    cJSON *workflowsItem = cJSON_GetObjectItem(root, "workflows");
    for(unsigned int i = 0; i<workload->nWorkflows; i++){

        // get workflow/job item
        cJSON *workflowItem = cJSON_GetArrayItem(workflowsItem, i);
        workflow_t *workflow = &(workload->workflows[i]);

        // load the number of jobs in the workflow
        workflow->nJobs = (unsigned int)cJSON_GetObjectItem(workflowItem, "nJobs")->valueint;
        workflow->launchT = (unsigned int)cJSON_GetObjectItem(workflowItem, "l")->valueint;
        workflow->priority = priority;
        priority ++;

        // allocate memory for jobs
        workflow->jobs = (job_t*)calloc(workflow->nJobs, sizeof(job_t));

        // loop over workflow jobs and load them
        cJSON *jobsItem = cJSON_GetObjectItem(workflowItem, "jobs");
        for(unsigned int j = 0; j<workflow->nJobs; j++){

            // load job data
            cJSON *jobItem = cJSON_GetArrayItem(jobsItem, j);
            job_t *job = &(workflow->jobs[j]);

            // load job data
            job->nIterations = (unsigned int)cJSON_GetObjectItem(jobItem, "IT")->valueint;
            job->m = (unsigned int)cJSON_GetObjectItem(jobItem, "m")->valueint;

            //job->CPT = (unsigned int)cJSON_GetObjectItem(jobItem, "CPT")->valuestring;
            job->totalC = (unsigned int)cJSON_GetObjectItem(jobItem, "totalC")->valueint;
            job->totalM = (unsigned int)cJSON_GetObjectItem(jobItem, "totalM")->valueint;
            job->O = (unsigned int)cJSON_GetObjectItem(jobItem, "O")->valueint;

            // allocate memory for job iteration features
            job->C = (unsigned int*)calloc(job->nIterations, sizeof(unsigned int));
            job->M = (unsigned int*)calloc(job->nIterations, sizeof(unsigned int));
            job->minR = (unsigned int*)calloc(job->nIterations, sizeof(unsigned int));
            job->maxR = (unsigned int*)calloc(job->nIterations, sizeof(unsigned int));
            job->meanR = (unsigned int*)calloc(job->nIterations, sizeof(unsigned int));
            job->S = (float*)malloc(job->nIterations * sizeof(float));

            for(unsigned int k = 0; k<job->nIterations; k++){

                job->C[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "C"), k)->valueint;
                job->M[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "M"), k)->valueint;
                job->minR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "minR"), k)->valueint;
                job->maxR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "maxR"), k)->valueint;
                job->meanR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "meanR"), k)->valueint;
                job->S[k] = (float)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "S"), k)->valuedouble;
            }
        }
    }


    return workload;
}

// open cluster file and load the JSON information to a cluster_t structure
cluster_t* loadCluster(char *in){
    
    // load file information into a JSON structure
    cJSON *root = readJSONFile(in);

    // allocate memory for the workload
    cluster_t *cluster = (cluster_t*)calloc(1, sizeof(cluster_t));

    // load general cluster information
    cluster->N = (unsigned int)cJSON_GetObjectItem(root, "N")->valueint;
    cluster->nGPUs = (unsigned int)cJSON_GetObjectItem(root, "nGPUs")->valueint;

    // load inter-node interconnect data from JSON
    cJSON *latenciesJSON = cJSON_GetObjectItem(root, "latencies");
    cJSON *BWJSON = cJSON_GetObjectItem(root, "bandwidths");

    cluster->interLatencies = (unsigned int*)calloc(cluster->N*cluster->N, sizeof(unsigned int));
    cluster->interBW = (unsigned int*)calloc(cluster->N*cluster->N, sizeof(unsigned int));

    for(unsigned int node1 = 0; node1 < cluster->N; node1++){

        cJSON *Node1latenciesJSON = cJSON_GetArrayItem(latenciesJSON, node1);
        cJSON *Node1BWJSON = cJSON_GetArrayItem(latenciesJSON, node1);

        for(unsigned int node2 = 0; node2 < cluster->N; node2++){

            cJSON *Node2latenciesJSON = cJSON_GetArrayItem(Node1latenciesJSON, node2);
            cJSON *Node2BWJSON = cJSON_GetArrayItem(Node1BWJSON, node2);

            if(node1 != node2){

                cluster->interLatencies[node1 * cluster->N + node2] = (unsigned int)Node2latenciesJSON->valueint;
                cluster->interBW[node1 * cluster->N + node2] = (unsigned int)Node2BWJSON->valueint;
            }
        }
    }

    // allocate memory for interconnection network data
    cluster->nGPUsPerNode = (unsigned int*)calloc(cluster->N, sizeof(unsigned int));
    cluster->firstGPUPerNode = (unsigned int*)calloc(cluster->N, sizeof(unsigned int));

    
    cluster->nodeLatencies = (unsigned int**)calloc(cluster->N*cluster->N, sizeof(unsigned int*));
    cluster->nodeBW = (unsigned int**)calloc(cluster->N*cluster->N, sizeof(unsigned int*));


    // load nodes
    cJSON *nodesJSON = cJSON_GetObjectItem(root, "nodes");
    unsigned int gpuCounter = 0;
    for(unsigned int node = 0; node < cluster->N; node++){

        cJSON *nodeJSON = cJSON_GetArrayItem(nodesJSON, node);

        // load global node information
        cluster->firstGPUPerNode[node] = gpuCounter;
        cluster->nGPUsPerNode[node] = (unsigned int)cJSON_GetObjectItem(nodeJSON, "nGPUs")->valueint;

        // load intra-node interconnect data from JSON
        cJSON *intraLatenciesJSON = cJSON_GetObjectItem(nodeJSON, "gpusLatencies");
        cJSON *intraBWJSON = cJSON_GetObjectItem(nodeJSON, "gpusBandwidths");

        // allocate memory for intra-node interconnect data
        unsigned int nGPUsPerNode = cluster->nGPUsPerNode[node];
        cluster->nodeLatencies[node] = (unsigned int*)calloc(nGPUsPerNode * nGPUsPerNode, sizeof(unsigned int));
        cluster->nodeBW[node] = (unsigned int*)calloc(nGPUsPerNode * nGPUsPerNode, sizeof(unsigned int));

        // load interconnects
        for(unsigned int gpu1 = 0; gpu1 < nGPUsPerNode; gpu1++){

            cJSON *GPU1latenciesJSON = cJSON_GetArrayItem(intraLatenciesJSON, gpu1);
            cJSON *GPU1BWJSON = cJSON_GetArrayItem(intraBWJSON, gpu1);

            for(unsigned int gpu2 = 0; gpu2 < nGPUsPerNode; gpu2++){

                cJSON *GPU2latenciesJSON = cJSON_GetArrayItem(GPU1latenciesJSON, gpu2);
                cJSON *GPU2BWJSON = cJSON_GetArrayItem(GPU1BWJSON, gpu2);

                if(gpu1 != gpu2){

                    cluster->nodeLatencies[node][gpu1 * nGPUsPerNode + gpu2] = (unsigned int)GPU2latenciesJSON->valueint;
                    cluster->nodeBW[node][gpu1 * nGPUsPerNode + gpu2] = (unsigned int)GPU2BWJSON->valueint;
                }
            }
        }
        
        gpuCounter += cluster->nGPUsPerNode[node];
    }

    // set the node corresponding to each GPU
    cluster->nodeOfGPU = (unsigned int*)calloc(gpuCounter, sizeof(unsigned int));
    gpuCounter = 0;
    for(unsigned int node = 0; node < cluster->N; node++){

        for(unsigned int gpu = 0; gpu < cluster->nGPUsPerNode[node]; gpu++){
            
            cluster->nodeOfGPU[gpuCounter] = node;
            gpuCounter ++;
        }
    }

    return cluster;
}

void initializeJobsState(workload_t *workload, simulationConf_t *simConf){

    unsigned int jobId = 0;
    unsigned int nWorkflows = workload->nWorkflows;

    for(unsigned int w = 0; w < nWorkflows; w++){

        workflow_t *workflow = &(workload->workflows[w]);
        unsigned int nJobs = workflow->nJobs;

        // loop over workflow jobs
        for(unsigned int j = 0; j<nJobs; j++){

            job_t *job = &(workflow->jobs[j]);
            job->workflow = workflow;
            job->currentIteration = 0;
            job->jobId = jobId;
            job->rcf = false; // no reconfiguration scheduled for now

            job->sPending = 0;
            job->sRunning = 0;
            job->ePending = 0;
            job->eRunning = 0;

            jobId ++;

            switch(simConf->simType){
                
                // if FIXED, all iterations of the job uses the same number of resources, so only 1 allocation
                case FIXED:
                    //job->allocation = (allocation_t*)calloc(1,sizeof(allocation_t));
                    job->allocation = (allocation_t*)calloc(job->nIterations + 1,sizeof(allocation_t));
                    break;
                // for EVOLVING and MALLEABLE executions, each iteration requires an allocation 
                case EVOLVING:
                    job->allocation = (allocation_t*)calloc(job->nIterations + 1,sizeof(allocation_t));
                    break;
                case MALLEABLE:
                    job->allocation = (allocation_t*)calloc(job->nIterations + 1,sizeof(allocation_t));
                    break;
            }
        }
    }
}

void initializeClusterState(cluster_t *cluster){

    // initialize GPUs state (all GPUs are available)
    cluster->nAv = cluster->nGPUs;

    // available GPUs per node
    cluster->nAvPerNode = (unsigned int*)calloc(cluster->N, sizeof(unsigned int));
    for(unsigned int node = 0; node<cluster->N; node++){

        cluster->nAvPerNode[node] = cluster->nGPUsPerNode[node];
    }

    cluster->avGPUs = (unsigned int*)calloc(cluster->nAv, sizeof(unsigned int));
    for(unsigned int gpu = 0; gpu<cluster->nAv; gpu++)
        cluster->avGPUs[gpu] = 1; // GPU available
}



void createJobArrivalEvents(EventQueue *evQueue, workload_t *workload){

    for(unsigned int i = 0; i<workload->nWorkflows; i++){

        unsigned int launchTime = workload->workflows[i].launchT;

        for(unsigned int j = 0; j<workload->workflows[i].nJobs; j++){

            // create job arrival event and add job to it
            event_t *event = createEvent(&(workload->workflows[i].jobs[j]), ARRIVAL, launchTime);

            // create event node
            Node<event_t*> *evNode = (Node<event_t*>*)calloc(1,sizeof(Node<event_t*>));
            evNode->data = event;

            // add element in the queue by the corresponding time step
            evQueue->addElementByTimeStep(evNode);
        }
    }
}

void storeResults(char *out){

    // TODO
    return;
}

int main(int argc, char *argv[]){

    // get simulation type
    unsigned int simType = std::stoul(argv[3]);

    // pending and running queues orderings
    unsigned int schedPol = std::stoul(argv[4]);
    unsigned int reconfPol = std::stoul(argv[5]);
    
    // number of GPUs selection policy
    unsigned int sched_allocation_n_pol = std::stoul(argv[6]);
    unsigned int reconf_allocation_n_pol = std::stoul(argv[7]);

    // placement policies
    unsigned int sched_place = std::stoul(argv[8]);
    unsigned int reconf_place = std::stoul(argv[9]);

    /* initialize simulation configuration structure */
    simulationConf_t simConf;
    switch(simType){
        case 0:
            simConf.simType = FIXED;
            break;
        case 1:
            simConf.simType = EVOLVING;
            break;
        case 2:
            simConf.simType = MALLEABLE;
            break;
    }

    /* Sched, reconf queue ordering policies */
    switch(schedPol){
        case 0:
            simConf.schedPolicy = eQUEUE_FCFS_SCHED;
            break;
    }

    switch(reconfPol){
        case 0:
            simConf.reconfPolicy = eQUEUE_NOP;
            break;
        case 1:
            simConf.reconfPolicy = eQUEUE_FCFS;
            break;
        case 2:
            simConf.reconfPolicy = eQUEUE_UTIL;
            break;
        case 3:
            simConf.reconfPolicy = eQUEUE_NET;
            break;
    }

    switch(sched_allocation_n_pol){
        case 0: 
            simConf.schedAllocationN = e_MIN_ALLOC;
            break;
        case 1: 
            simConf.schedAllocationN = e_MIN_1_ALLOC;
            break;
        case 2: 
            simConf.schedAllocationN = e_MAX_ALLOC;
            break;
        case 3: 
            simConf.schedAllocationN = e_MAX_POSSIBLE_ALLOC;
            break;
        case 4: 
            simConf.schedAllocationN = e_MAX_NODE_ALLOC;
            break;
        case 5: 
            simConf.schedAllocationN = e_MAX_TOPO_ALLOC;
            break;
    }

    switch(reconf_allocation_n_pol){
        case 0:
            simConf.reconfAllocationN = e_EXPAND_SHRINK;
            break;
        case 1:
            simConf.reconfAllocationN = e_EXPAND;
            break;
        case 2:
            simConf.reconfAllocationN = e_EXPAND_SHRINK_MIGRATE;
            break;
        case 3:
            simConf.reconfAllocationN = e_EXPAND_SHRINK_UNLIMITED;
            break;
        case 4:
            simConf.reconfAllocationN = e_EXPAND_UNLIMITED;
            break;
        case 5:
            simConf.reconfAllocationN = e_EXPAND_SHRINK_MIGRATE_UNLIMITED;
            break;
    }

    switch(sched_place){
        case 0:
            simConf.schedAllocationPlace = e_SEQUENTIAL;
            break;
        case 2:
            simConf.schedAllocationPlace = e_TOPO_BEST_EFFORT;
            break;
        case 3:
            simConf.schedAllocationPlace = e_TOPO_OPT;
            break;
    }

    switch(reconf_place){
        case 0:
            simConf.reconfAllocationPlace = e_SEQUENTIAL;
            break;
        case 2:
            simConf.reconfAllocationPlace = e_TOPO_BEST_EFFORT;
            break;
        case 3:
            simConf.reconfAllocationPlace = e_TOPO_OPT;
            break;
    }


    /* load workload from file */
    workload_t *workload = loadWorkload(argv[1]);
    initializeJobsState(workload, &simConf);

    /* load cluster from file */
    cluster_t *cluster = loadCluster(argv[2]);
    initializeClusterState(cluster);


    /* Initialize event queue */
    EventQueue *evQueue = new EventQueue();
    // create jobs arrival time events
    createJobArrivalEvents(evQueue, workload);    
    // update job priorities (temporal solution)
    evQueue->initializeJobPriorities();
    evQueue->printEvents();

    /* Initialize job queues */
    JobQueue *pQueue = initSchedQueue(&simConf);
    JobQueue *rQueue = initReconfQueue(&simConf);
    JobQueue *fQueue = new JobQueueFCFS(); // this queue does not have any policy


    unsigned int makespan = 0;
    // process events until no events are left to process
    while(!evQueue->isEmpty()){
        
        // get first event time step
        unsigned int t = evQueue->getElementByIndex(0)->data->t;
 
        // process all events in time step t
        bool p = processEventsTimeStep(evQueue, t, pQueue, rQueue, fQueue, cluster, &simConf); 

        // invoque the scheduler and reconfigurer if necessary
        if(p){

            rQueue->reconfigure(evQueue, t, rQueue, fQueue, cluster, &simConf);
            pQueue->schedule(evQueue, t, pQueue, rQueue, cluster, &simConf);
        }

        makespan = t;
    }

    writeSimulationResults(argv[10], argv[11], fQueue, makespan);
    
    return 0;
}