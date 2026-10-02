#include "instance.hpp"
#include "job.hpp"


// Tcomp = C * S / P + (1-S) * C
unsigned int computeComputationTime(unsigned int C, unsigned int N, float S){

    return (unsigned int)((float)C * S / (float)N + (float)C * (1.0 - S));
}

// Tcomm = 
unsigned int computeCommunicationTime(unsigned int M, allocation_t *alloc, cluster_t *cluster){

    // compute time required for communication (worst pair of GPUs)
    unsigned int commTime = 0, tmpCommTime = 0;
    unsigned int latency = 0;
    unsigned int BW = 0;

    // find the worst GPU pair (in terms of communication time)
    for(unsigned int gpu1 = 0; gpu1 < alloc->nGPUs; gpu1++){

        for(unsigned int gpu2 = gpu1+1; gpu2 < alloc->nGPUs - 1; gpu2++){

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
            tmpCommTime = M / (BW * alloc->nGPUs);

            // find the maximum
            if(tmpCommTime > commTime)
                commTime = tmpCommTime;
        }
    }

    return commTime;
}

// compute time required to execute an iteration where:
// T = Tcomp + Tcomm
unsigned int computeIterationTime(job_t *job, cluster_t *cluster){

    // get job information
    unsigned int cIt = job->currentIteration;
    unsigned int C = job->C[cIt];
    unsigned int M = job->M[cIt];
    unsigned int S = job->S;
    allocation_t *alloc = &(job->allocation[cIt]);

    // initialize computation and communication times
    unsigned int compTime = computeComputationTime(C, alloc->nGPUs, S);
    unsigned int commTime = computeCommunicationTime(M, alloc, cluster);


    // return execution time
    return compTime + commTime;
}

unsigned int computeReconfigurationTime(job_t *job){

    return 0;
}