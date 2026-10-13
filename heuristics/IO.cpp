
#include <cstdio>
#include <cstdlib>
#include "libMGSP.hpp"
#include "jobQueue.hpp"

// store allocated GPUs
static void writeGPUList(FILE *file, allocation_t *alloc){
    for(unsigned int i = 0; i < alloc->nGPUs; i++){

        if(i > 0)
            fprintf(file, ";");

        fprintf(file, "%u", alloc->gpus[i]);
    }
}


void writeSimulationResults(const char *allocationFilename, const char *summaryFilename, JobQueue *fQueue, unsigned int makespan){
    
    FILE *allocFile = fopen(allocationFilename, "w");

    if(!allocFile){

        perror("Could not open allocation results file");
        return;
    }

    FILE *summaryFile = fopen(summaryFilename, "w");

    if(!summaryFile){

        perror("Could not open simulation summary file");
        fclose(allocFile);
        return;    
    }

    /* Header */
    fprintf(allocFile,"job_id,iteration,start,end,duration,n_gpus,gpus,util,throughput\n");



    /* Traverse all finished jobs and store the allocations */
    // also compute different metrics such as Average Response Time (AF), Average Bounded Slowdown (BSLD) and Average wait time (AW), AR
    Node<job_t*> *node = fQueue->getFirst();

    double AF = 0.0, BSLD = 0.0, AW = 0.0, AR = 0.0;

    while(node){

        job_t *job = node->data;

        unsigned int runTime = job->eRunning - job->sRunning;
        unsigned int waitTime = job->ePending - job->sPending;
        
        // compute metrics
        AF += (double)(job->eRunning - job->sPending);
        AW += (double)waitTime;
        AR += (double)runTime;
        BSLD += std::max(1.0, AF / std::max(AR, 10.0));

        printf("Start pending = %u, end running = %u, turn = %u (%lf)\n", 
            job->sPending, job->eRunning, job->eRunning - job->sPending, (double)job->eRunning - job->sPending);

        for(unsigned int i = 0; i < job->currentIteration; i++){

            allocation_t *alloc = &job->allocation[i];

            /*
             * Ignore iterations that were never allocated.
             *
             * This assumes that start == end means that the
             * allocation was not used.
             */
            if(alloc->nGPUs == 0)
                continue;


            fprintf(allocFile,"%u,%u,%u,%u,%u,%u,\"",job->jobId,i,alloc->start,alloc->end, alloc->end-alloc->start,alloc->nGPUs);

            writeGPUList(allocFile, alloc);

            fprintf(allocFile,"\",%u,%u\n", alloc->util, alloc->throughput);
        }

        node = node->next;
    }

    
    printf("AF %lf, N = %u, div = %lf, %f\n", AF, fQueue->getN(), AF / (double)fQueue->getN(), AF / (double)fQueue->getN());

    AF /= (double)fQueue->getN();
    AR /= (double)fQueue->getN();
    AW /= (double)fQueue->getN();
    BSLD /= (double)fQueue->getN();


    /* Summary */

    fprintf(summaryFile, "makespan,AF,AR,AW,BSLD\n");
    fprintf(summaryFile, "%u,%lf,%lf,%lf,%lf\n", makespan, AF,AR,AW,BSLD);


    fclose(allocFile);
    fclose(summaryFile);
}