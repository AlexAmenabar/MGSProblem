#include "libMGSP.hpp"
#include "queue.hpp"
#include "eventQueue.hpp"
#include "jobQueue.hpp"
#include <cmath>

// Tcomp = C * S / P + (1-S) * C
unsigned int computeComputationTime(unsigned int C, unsigned int N, float S){

    unsigned int Cp = ceil((float)C * S);
    unsigned int Cs = ceil((float)C * (1.0 - S));

    return ceil((Cp + N - 1) / N + Cs);

    //return ((unsigned int)(ceil((float)Cp / (float)N + (float)Cs)));
    
    //return (ceil((float)C * S / (float)N + (float)C * ((float)1.0 - S)));
    //return (unsigned int)((float)C * S / (float)N + (float)C * (1.0 - S));
}

// Tcomm = 
unsigned int computeCommunicationTime(unsigned int M, allocation_t *alloc, cluster_t *cluster){

    // compute time required for communication (worst pair of GPUs)
    unsigned int commTime = 0, tmpCommTime = 0;
    unsigned int latency = 0;
    unsigned int BW = 0;

    // find the worst GPU pair (in terms of communication time)
    for(unsigned int gpu1 = 0; gpu1 < alloc->nGPUs - 1; gpu1++){

        for(unsigned int gpu2 = gpu1+1; gpu2 < alloc->nGPUs; gpu2++){

            // reinitialize latency
            latency = 0;

            // get global GPU identifiers (cluster index)
            unsigned int g1 = alloc->gpus[gpu1];
            unsigned int g2 = alloc->gpus[gpu2];

            // get the nodes of the GPUs
            unsigned int n1 = cluster->nodeOfGPU[g1];
            unsigned int n2 = cluster->nodeOfGPU[g2];

            // local indices
            unsigned int l1 = g1 - cluster->firstGPUPerNode[n1];
            unsigned int l2 = g2 - cluster->firstGPUPerNode[n2];            
            unsigned int index;

            // if both GPUs are in the same node, compute using intra-node latency and BW
            if(n1 == n2){

                index = l1 * cluster->nGPUsPerNode[n1] + l2;
                BW = cluster->nodeBW[n1][index];
                latency = cluster->nodeLatencies[n1][index];
            }
            // else, BW is the lower between nodes and inter-nodes, and latency the sum
            else{

                // get interconnect BW: lower in the [node - interconnect - node] path
                BW = cluster->interBW[n1 * cluster->N + n2];  // we can consider that the inter node BW is the bottleneck 

                // find the worst (max) intra-node latency for both nodes
                unsigned int tmpLatency = 0;
                for(unsigned int gpu = 0; gpu < cluster->nGPUsPerNode[n1]; gpu++){

                    if(gpu != l1){

                        if(cluster->nodeLatencies[n1][l1 * cluster->nGPUsPerNode[n1] + gpu] > tmpLatency){
                            tmpLatency = cluster->nodeLatencies[n1][l1 * cluster->nGPUsPerNode[n1] + gpu];
                        }
                    }
                }
                latency += tmpLatency;

                tmpLatency = 0;
                for(unsigned int gpu = 0; gpu < cluster->nGPUsPerNode[n2]; gpu++){

                    if(gpu != l2){

                        if(cluster->nodeLatencies[n2][l1 * cluster->nGPUsPerNode[n2] + gpu] > tmpLatency){
                            tmpLatency = cluster->nodeLatencies[n2][l1 * cluster->nGPUsPerNode[n2] + gpu];
                        }
                    }
                }
                latency += tmpLatency;
                
                // sum inter-node latency
                latency += cluster->interLatencies[n1 * cluster->N + n2];
            }

            // compute communication time

            printf(" gpu1 = %u | gpu2 = %u | latency = %u | M = %u | BW = %u | nGPUS = %u\n", gpu1, gpu2, latency, M, BW, alloc->nGPUs);
            fflush(stdout);

            //tmpCommTime = latency + (M / (BW * alloc->nGPUs));
            unsigned int PxBW = alloc->nGPUs * BW;
            tmpCommTime = ceil((M + PxBW - 1) / PxBW);
            //tmpCommTime = tmpCommTime / BW;
            //tmpCommTime = M / (BW * alloc->nGPUs);

            // find the maximum
            if(tmpCommTime > commTime)
                commTime = tmpCommTime;
        }
    }

    return commTime;
}

// compute time required to execute an iteration where:
// T = Tcomp + Tcomm

unsigned int computeTotalTime(job_t *job, cluster_t *cluster){

    // get job information
    allocation_t *alloc = &(job->allocation[0]); // only the first allocation is enough

    // compute C and M
    unsigned int C = 0;
    unsigned int M = 0;
    unsigned int compTime = 0;
    unsigned int commTime = 0;
    for(unsigned int it = 0; it < job->nIterations; it++){

        unsigned int tmpcompTime = computeComputationTime(job->C[it], alloc->nGPUs, job->S[it]);
        //unsigned int tmpcommTime = computeCommunicationTime(job->M[it], alloc, cluster);
        unsigned int tmpcommTime = computeCommunicationTime(job->M[it], alloc, cluster);
        
        compTime += tmpcompTime;
        commTime += tmpcommTime;
        //commTime += tmpcommTime;

        //printf("%u + %u = %u\n", tmpcompTime, tmpcommTime, tmpcompTime + tmpcommTime);
        //fflush(stdout);
    }

    //unsigned int C = job->totalC;
    //unsigned int M = job->totalM;
    //unsigned int S = job->S;

    // initialize computation and communication times
    //unsigned int compTime = computeComputationTime(C, alloc->nGPUs, S);


    // return execution time
    return compTime + commTime;
}

unsigned int computeIterationTime(job_t *job, cluster_t *cluster){

    // get job information
    unsigned int cIt = job->currentIteration;
    unsigned int C = job->C[cIt];
    unsigned int M = job->M[cIt];
    float S = job->S[cIt];
    allocation_t *alloc = &(job->allocation[cIt]);

    // initialize computation and communication times
    unsigned int compTime = computeComputationTime(C, alloc->nGPUs, S);
    unsigned int commTime = computeCommunicationTime(M, alloc, cluster);

    printf("%u + %u = %u\n", compTime, commTime, compTime + commTime);

    // return execution time
    return compTime + commTime;
}

unsigned int computeIterationComputationTime(job_t *job, cluster_t *cluster){

    // get job information
    unsigned int cIt = job->currentIteration;
    unsigned int C = job->C[cIt];
    unsigned int M = job->M[cIt];
    float S = job->S[cIt];
    allocation_t *alloc = &(job->allocation[cIt]);

    // initialize computation and communication times
    unsigned int compTime = computeComputationTime(C, alloc->nGPUs, S);

    // return execution time
    return compTime;
}

unsigned int computeIterationCommunicationTime(job_t *job, cluster_t *cluster){

    // get job information
    unsigned int cIt = job->currentIteration;
    unsigned int C = job->C[cIt];
    unsigned int M = job->M[cIt];
    float S = job->S[cIt];
    allocation_t *alloc = &(job->allocation[cIt]);

    // initialize computation and communication times
    unsigned int commTime = computeCommunicationTime(M, alloc, cluster);

    // return execution time
    return commTime;
}


void launchJob(EventQueue *evQueue, unsigned int t, job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    // calculate time required to execute the iteration / job
    unsigned int computeTime, computationTime, communicationTime;
    unsigned int nIterations = job->nIterations;

    // initialize event node
    Node<event_t*> *eventNode = (Node<event_t*>*)calloc(1, sizeof(Node<event_t*>));
    event_t *endEvent;


    job->allocation[job->currentIteration].start = t;

    // compute [END] or [END_IT] event and time required for it
    //switch(simConf->simType){
        /*case FIXED:

            // Fixed: compute total job execution time
            computeTime = computeTotalTime(job, cluster);
            endEvent = createEvent(job, END, t + computeTime);
            break;

        // compute time required only to execute one iteration and create an END_IT or END evenr
        case EVOLVING:

            computationTime = computeIterationComputationTime(job,cluster);
            communicationTime = computeIterationCommunicationTime(job,cluster);
            computeTime = computationTime + communicationTime;

            // compute monitorization data
            getUtilization(job, computationTime, communicationTime);
            getNetworkThroughput(job, communicationTime);

            // if it is the last iteration, the next event is an [END], else, [END_IT]
            if(job->currentIteration + 1 == nIterations)
                endEvent = createEvent(job, END, t + computeTime);
            else
                endEvent = createEvent(job, END_IT, t + computeTime);
            break;

            // compute system metrics for this iteration
            

        case MALLEABLE:*/

    computationTime = computeIterationComputationTime(job,cluster);
    communicationTime = computeIterationCommunicationTime(job,cluster);
    computeTime = computationTime + communicationTime;

    printf("%u + %u = %u\n", computationTime, communicationTime, computationTime + communicationTime);

    // compute monitorization data
    unsigned int util = getUtilization(job, computationTime, communicationTime);
    unsigned int net = getNetworkThroughput(job, communicationTime);

    printf("Utilization of job %u at iteration %u: %u %u\n", 
        job->jobId, job->currentIteration, util, net);
    fflush(stdout);

    // if it is the last iteration, the next event is an [END], else, [END_IT]
    if(job->currentIteration + 1 == nIterations)
        endEvent = createEvent(job, END, t + computeTime);
    else
        endEvent = createEvent(job, END_IT, t + computeTime);
        //break;
    //}

    // link event to event node
    eventNode->data = endEvent;
    job->allocation[job->currentIteration].end = t + computeTime;

    printf(" -- Job %u (%u) launched with %u at %u (%u)\n", job->jobId, job->currentIteration, job->allocation[job->currentIteration].nGPUs, t, eventNode->data->t);
    fflush(stdout);

    // ad event to the event queue
    evQueue->addElementByTimeStep(eventNode);
    printf("\n");
}

void reconfigureJob(EventQueue *evQueue, unsigned int t, job_t *job, cluster_t *cluster, simulationConf_t *simConf){

    unsigned int O = job->O; // reconfiguration time

    // create end reconfiguration event
    Node<event_t*> *eventNode = (Node<event_t*>*)calloc(1, sizeof(Node<event_t*>));
    event_t *endEvent = createEvent(job, END_RECONF, t + O);

    // add event to the event queue
    eventNode->data = endEvent;

    evQueue->addElementByTimeStep(eventNode);
    printf("\n");

    return;
}


// monitorization

unsigned int getUtilization(job_t* job, unsigned int computationTime, unsigned int communicationTime){

    float futil = (float)computationTime / (float)(computationTime + communicationTime);
    job->allocation[job->currentIteration].util = (unsigned int)(futil * 100);

    return job->allocation[job->currentIteration].util;
}

unsigned int getNetworkThroughput(job_t* job, unsigned int communicationTime){

    job->allocation[job->currentIteration].throughput = job->M[job->currentIteration];

    return job->M[job->currentIteration];
}