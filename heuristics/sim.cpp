// simulator

#include <cstdlib>
#include <cstdio>
#include <string>

#include "queue.hpp"
#include "jobQueue.hpp"
#include "eventQueue.hpp"
#include "instance.hpp"
#include "heuristics.hpp"
#include "cJSON.h"

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
            job->S = (float)cJSON_GetObjectItem(jobItem, "S")->valuedouble;
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

            for(unsigned int k = 0; k<job->nIterations; k++){

                job->C[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "C"), k)->valueint;
                job->M[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "M"), k)->valueint;
                job->minR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "minR"), k)->valueint;
                job->maxR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "maxR"), k)->valueint;
                job->meanR[k] = (unsigned int)cJSON_GetArrayItem(cJSON_GetObjectItem(jobItem, "meanR"), k)->valueint;
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
            jobId ++;

            switch(simConf->simType){
                
                // if FIXED, all iterations of the job uses the same number of resources, so only 1 allocation
                case FIXED:
                    job->allocation = (allocation_t*)calloc(1,sizeof(allocation_t));
                    break;
                // for EVOLVING and MALLEABLE executions, each iteration requires an allocation 
                case EVOLVING:
                    job->allocation = (allocation_t*)calloc(job->nIterations,sizeof(allocation_t));
                    break;
                case MALLEABLE:
                    job->allocation = (allocation_t*)calloc(job->nIterations,sizeof(allocation_t));
                    break;
            }
        }
    }
}

void initializeClusterState(cluster_t *cluster){

    // initialize GPUs state (all GPUs are available)
    cluster->nAv = cluster->nGPUs;

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

            printf(" -- Creating event at time step %u\n", launchTime);
            fflush(stdout);

            // create event node
            Node<event_t*> *evNode = (Node<event_t*>*)calloc(1,sizeof(Node<event_t*>));
            evNode->data = event;


            // add element in the queue by the corresponding time step
            evQueue->addElementByTimeStep(evNode);
        }
    }
}


// schedule job 
void schedule(event_t *event, EventQueue *evQueue, cluster_t *cluster, simulationConf_t *simConf){

    // select job to launch following the scheduling policy
    FCFS(evQueue, event, cluster, simConf);
}

// reconfigure job
void reconfigure(){

    return;
}

void finishJob(event_t *event, EventQueue *evQueue, cluster_t *cluster, simulationConf_t *simConf){

    // job finished, so deallocate resources
    deallocateResources(event->job, cluster);
}

void processEvent(event_t *event, EventQueue *evQueue, cluster_t *cluster, simulationConf_t *simConf){

    switch(event->eventEnum){

        case ARRIVAL:
            schedule(event, evQueue, cluster, simConf);
            break;
        case RESCHEDULE:
            schedule(event, evQueue, cluster, simConf);
            break;
        case END:
            finishJob(event, evQueue, cluster, simConf);
            break;
        case END_IT:
            break;
        case END_RECONF:
            break;
    }
}


int main(int argc, char *argv[]){

    // get simulation type
    unsigned int simType = std::stoul(argv[3]);
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

    // load workdload
    workload_t *workload = loadWorkload(argv[1]);
    initializeJobsState(workload, &simConf);

    // load cluster
    cluster_t *cluster = loadCluster(argv[2]);
    initializeClusterState(cluster);

    // initialize event queue
    EventQueue evQueue;

    // create jobs arrival time events
    createJobArrivalEvents(&evQueue, workload);

    // update job priorities (temporal solution)
    evQueue.updateJobPriorities();
    evQueue.printEvents();


    // loop over queues and execute heuristics
    while(!evQueue.isEmpty()){
        
        Node<event_t*> *eventNode = evQueue.removeElementByIndex(0);
        processEvent(eventNode->data, &evQueue, cluster, &simConf);
    }

    storeResults();

    return 0;
}