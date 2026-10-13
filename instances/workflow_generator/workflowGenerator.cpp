#include "cJSON.h"
#include "workflowGenerator.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <time.h>
#include <cmath>
#include <algorithm>

/*
For evolving workloads, we take the min resources request as the one requested by the job.
For malleable workloads, we use all the information
For fixed workloads, we use the minimum as the job request, 
*/

// g++ -g -I../cJSON -o workflowGen workflowGenerator.cpp ../cJSON/cJSON.c -lm 
// ./workflowGen workflowSpec1.json workflow1.json




// Gaussian N(mean, stddev) using Box-Muller
double gaussian(double mean, double stddev) {

    double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
    double u2 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);

    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

    return mean + stddev * z;
}


cJSON *readWorkflowJSON(char *in){
    
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


workflowSpec_t *parseWorkflowJSON(cJSON *json) {

    if (json == NULL) {
        fprintf(stderr, "Error: NULL JSON object\n");
        return NULL;
    }

    workflowSpec_t *spec = (workflowSpec_t*)calloc(1, sizeof(workflowSpec_t));
    if (spec == NULL) {
        fprintf(stderr, "Error: could not allocate workflowSpec\n");
        return NULL;
    }

    // number of workflows
    cJSON *item = cJSON_GetObjectItem(json, "nWorkflows");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'nWorkflows' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->nWorkflows = (unsigned int)item->valueint;

    // percentage of workflows
    item = cJSON_GetObjectItem(json, "pWorkflows");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'pWorkflows' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pWorkflows = (unsigned int)item->valueint;

    //dependencies
    item = cJSON_GetObjectItem(json, "deps");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'deps' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->deps = (unsigned int)item->valueint;


    // range of jobs per workflow
    item = cJSON_GetObjectItem(json, "minJobs");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'minJobs' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minJobs = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxJobs");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'maxJobs' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxJobs = (unsigned int)item->valueint;


    // computation range
    item = cJSON_GetObjectItem(json, "minC");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'minC' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minC = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxC");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: 'maxC' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxC = (unsigned int)item->valueint;


    // communication range
    item = cJSON_GetObjectItem(json, "minM");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minM' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minM = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxM");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxM' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxM = (unsigned int)item->valueint;

    // reconfiguration overhead
    item = cJSON_GetObjectItem(json, "minO");

    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minO' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minO = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxO");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxO' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxO = (unsigned int)item->valueint;


    // iterations range per job
    item = cJSON_GetObjectItem(json, "minIT");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minIT' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minIT = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxIT");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxIT' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxIT = (unsigned int)item->valueint;

    // phase range per job
    item = cJSON_GetObjectItem(json, "minPh");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minPh' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minPH = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxPh");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxPh' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxPH = (unsigned int)item->valueint;


    // launch times range per job
    item = cJSON_GetObjectItem(json, "minL");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minL' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minL = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxL");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxL' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxL = (unsigned int)item->valueint;


    // range of resources
    item = cJSON_GetObjectItem(json, "minR");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minR = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxR");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxR = (unsigned int)item->valueint;


    // range of resources
    item = cJSON_GetObjectItem(json, "minS");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'minS' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->minS = (unsigned int)item->valueint;

    item = cJSON_GetObjectItem(json, "maxS");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxS' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->maxS = (unsigned int)item->valueint;


    item = cJSON_GetObjectItem(json, "iterative");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pIterative = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "iterativephases");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pIterativePhases = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "phasesiterative");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pPhasesIterative = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "moldable");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pMoldable = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "malleable");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pMalleable = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "evolving");
    if (!cJSON_IsNumber(item)) {
        fprintf(stderr, "Error: 'maxR' must be a number\n");
        free(spec);
        return NULL;
    }
    spec->pEvolving = (float)item->valuedouble;


    spec->pRigid = 1.0 - spec->pEvolving - spec->pMalleable - spec->pMoldable;

    // communication patterns
    cJSON *cpts = cJSON_GetObjectItem(json, "cpts");
    if (!cJSON_IsArray(cpts)) {
        fprintf(stderr, "Error: 'cpts' must be an array\n");
        free(spec);
        return NULL;
    }
    spec->nCPTs = cJSON_GetArraySize(cpts);

    spec->cpts = (CPT*)malloc(spec->nCPTs * sizeof(CPT));
    if (spec->cpts == NULL) {
        fprintf(stderr, "Error: could not allocate CPT array\n");
        free(spec);
        return NULL;
    }

    for (unsigned int i = 0; i < spec->nCPTs; i++) {

        cJSON *cpt = cJSON_GetArrayItem(cpts, i);

        if (!cJSON_IsString(cpt)) {
            fprintf(stderr, "Error: cpts[%u] must be a string\n", i);

            free(spec->cpts);
            free(spec);

            return NULL;
        }

        if (strcmp(cpt->valuestring, "RING") == 0) {

            spec->cpts[i] = RING;

        } else if (strcmp(cpt->valuestring, "TREE") == 0) {

            spec->cpts[i] = TREE;

        } else if (strcmp(cpt->valuestring, "ALL2ALL") == 0) {

            spec->cpts[i] = ALL2ALL;

        } else {

            fprintf(stderr,
                    "Error: unknown communication pattern '%s'\n",
                    cpt->valuestring);

            free(spec->cpts);
            free(spec);

            return NULL;
        }
    }

    return spec;
}


static const char *cptToString(CPT cpt){
    switch (cpt) {
        case RING:
            return "RING";

        case TREE:
            return "TREE";

        case ALL2ALL:
            return "ALL2ALL";

        default:
            return "UNKNOWN";
    }
}


int writeWorkflowJSON(const char *filename, const workflow_t *workflow){
    if (filename == NULL || workflow == NULL) {
        fprintf(stderr, "Error: NULL filename or workflow\n");
        return 0;
    }

    // create root json object
    cJSON *root = cJSON_CreateObject();

    // global workflow information
    cJSON_AddNumberToObject(root, "nWorkflows", workflow->nWorkflows);
    cJSON_AddNumberToObject(root,"pWorkflows",workflow->pWorkflows);
    cJSON_AddNumberToObject(root,"deps",workflow->deps);


    // workflows as arrays (arrays of jobs)
    cJSON *workflows = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "workflows", workflows);


    // loop over workflows and store their information
    for (unsigned int i = 0; i < workflow->nWorkflows; i++) {

        cJSON *workflowJSON = cJSON_CreateObject();
        cJSON_AddNumberToObject(workflowJSON,"nJobs",workflow->nJobs[i]);

        cJSON_AddNumberToObject(workflowJSON,"l",workflow->l[i]);

        // array of jobs
        cJSON *jobs = cJSON_CreateArray();
        cJSON_AddItemToObject(workflowJSON, "jobs", jobs);


        // iterate over jobs and store their values
        for (unsigned int j = 0; j < workflow->nJobs[i]; j++) {

            cJSON *job = cJSON_CreateObject();
            cJSON_AddNumberToObject(job,"O",workflow->O[i][j]);
            cJSON_AddNumberToObject(job,"IT",workflow->IT[i][j]);
            cJSON_AddNumberToObject(job,"Y",workflow->malleable[i][j]);

            // array of minR and max R per iteration
            cJSON *C = cJSON_CreateArray();
            cJSON *M = cJSON_CreateArray();
            cJSON *minR = cJSON_CreateArray();
            cJSON *maxR = cJSON_CreateArray();
            cJSON *jCPT = cJSON_CreateArray();
            cJSON *jS = cJSON_CreateArray();

            cJSON_AddItemToObject(job, "C", C);
            cJSON_AddItemToObject(job, "M", M);
            cJSON_AddItemToObject(job, "minR", minR);
            cJSON_AddItemToObject(job, "maxR", maxR);
            cJSON_AddItemToObject(job, "CPT", jCPT);
            cJSON_AddItemToObject(job, "S", jS);

            // resource request for each iteration
            for (unsigned int k = 0; k < workflow->IT[i][j]; k++) {

                cJSON_AddNumberToObject(C,"C",workflow->C[i][j][k]);
                cJSON_AddNumberToObject(M,"M",workflow->M[i][j][k]);
                cJSON_AddNumberToObject(jS,"S",workflow->S[i][j][k]);
                cJSON_AddItemToArray(minR,cJSON_CreateNumber(workflow->minR[i][j][k]));
                cJSON_AddItemToArray(maxR,cJSON_CreateNumber(workflow->maxR[i][j][k]));
                cJSON_AddStringToObject(jCPT,"CPT",cptToString((CPT)workflow->CPT[i][j][k]));
            }

            // add job to workflow
            cJSON_AddItemToArray(jobs, job);


            // for evolving and fixed
            unsigned int totalC = 0, totalM = 0, *meanR;
            meanR = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));

            cJSON *jMeanR = cJSON_CreateArray();
            cJSON_AddItemToObject(job, "meanR", jMeanR);

            for (unsigned int k = 0; k < workflow->IT[i][j]; k++) {
                
                totalC += workflow->C[i][j][k];
                totalM += workflow->M[i][j][k];
                meanR[k] = (workflow->maxR[i][j][k] + workflow->minR[i][j][k]) / 2;

                
                cJSON_AddNumberToObject(jMeanR,"meanR",meanR[k]);
            }

            cJSON_AddNumberToObject(job, "totalC", totalC);
            cJSON_AddNumberToObject(job, "totalM", totalM);
        }

        // add workflow to global json
        cJSON_AddItemToArray(workflows, workflowJSON);
    }

    // convert json tree to text
    char *jsonString = cJSON_Print(root);

    // write content to a file
    FILE *file = fopen(filename, "w");
    fprintf(file, "%s\n", jsonString);
    fclose(file);

    // free memory
    free(jsonString);
    cJSON_Delete(root);

    return 1;
}


// instance generator
int main(int argc, char*argv[]){

    srand(time(0));

    // input file name
    char *in = argv[1];
    char *out = argv[2];

    // read input file and parse from JSON to workflow structure
    cJSON *json = readWorkflowJSON(in);
    workflowSpec_t *spec = parseWorkflowJSON(json);

    /*
    Create three copies of the workload:
        - Fixed version: min and max resources are equals and only one iteration with all
                        the computation and communication
        - Malleable version: iterations, and ranges
        - Evolving version: iterations, but at each iteration a fixed number of resources
    */


    // allocate memory for the workflow
    workflow_t *workflow = (workflow_t*)calloc(1,sizeof(workflow_t));
    workflow->nWorkflows = spec->nWorkflows;
    workflow->pWorkflows = spec->pWorkflows;
    workflow->deps = spec->deps;

    workflow->nJobs = (unsigned int*)calloc(workflow->nWorkflows, sizeof(unsigned int));
    workflow->C = (unsigned int***)calloc(workflow->nWorkflows, sizeof(unsigned int**));
    workflow->M = (unsigned int***)calloc(workflow->nWorkflows, sizeof(unsigned int**));
    workflow->S = (double***)calloc(workflow->nWorkflows, sizeof(double**));
    workflow->O = (unsigned int**)calloc(workflow->nWorkflows, sizeof(unsigned int*));
    workflow->malleable = (unsigned int**)calloc(workflow->nWorkflows, sizeof(unsigned int*));
    workflow->IT = (unsigned int**)calloc(workflow->nWorkflows, sizeof(unsigned int*));
    workflow->PH = (unsigned int**)calloc(workflow->nWorkflows, sizeof(unsigned int*));
    workflow->l = (unsigned int*)calloc(workflow->nWorkflows, sizeof(unsigned int));
    workflow->CPT = (unsigned int***)calloc(workflow->nWorkflows, sizeof(unsigned int**));
    
    workflow->maxR = (unsigned int***)calloc(workflow->nWorkflows, sizeof(unsigned int**));
    workflow->minR = (unsigned int***)calloc(workflow->nWorkflows, sizeof(unsigned int**));


    // calculate how much jobs of each type will be generated
    unsigned int rigid, moldable, malleable, evolving;
    unsigned int iterative, iterativePhases, phasesIterative;

    rigid = (unsigned int)((float)100.0 * spec->pRigid);
    moldable = (unsigned int)((float)100.0 * spec->pMoldable) + rigid;
    malleable = (unsigned int)((float)100.0 * spec->pMalleable) + moldable;
    evolving  = 100;

    iterative = (unsigned int)((float)100.0 * spec->pIterative);
    iterativePhases = (unsigned int)((float)100.0 * spec->pIterativePhases) + iterative;
    phasesIterative = 100;
    

    // loop over jobs/workflows and generate them
    unsigned int value, range;
    for(unsigned int i = 0; i<workflow->nWorkflows; i++){

        // decide if it is a workflow
        workflow->nJobs[i] = 1;
        if(rand() % 100 < workflow->pWorkflows){

            // compute number of jobs in the workflow
            range = spec->maxJobs - spec->minJobs + 1;
            workflow->nJobs[i] = rand() % range + spec->minJobs;
        }

        // launch time
        range = spec->maxL - spec->minL + 1;
        workflow->l[i] = rand() % range + spec->minL;


        // allocate memory for the jobs in the workflow
        workflow->C[i] = (unsigned int**)calloc(workflow->nJobs[i], sizeof(unsigned int*));
        workflow->M[i] = (unsigned int**)calloc(workflow->nJobs[i], sizeof(unsigned int*));
        workflow->S[i] = (double**)calloc(workflow->nJobs[i], sizeof(double*));
        workflow->O[i] = (unsigned int*)calloc(workflow->nJobs[i], sizeof(unsigned int));
        workflow->malleable[i] = (unsigned int*)calloc(workflow->nJobs[i], sizeof(unsigned int));
        workflow->IT[i] = (unsigned int*)calloc(workflow->nJobs[i], sizeof(unsigned int));
        workflow->PH[i] = (unsigned int*)calloc(workflow->nJobs[i], sizeof(unsigned int));
        workflow->maxR[i] = (unsigned int**)calloc(workflow->nJobs[i], sizeof(unsigned int*));
        workflow->minR[i] = (unsigned int**)calloc(workflow->nJobs[i], sizeof(unsigned int*));
        workflow->CPT[i] = (unsigned int**)calloc(workflow->nJobs[i], sizeof(unsigned int*));

        // loop over jobs in the workflow and generate them
        for(unsigned int j = 0; j<workflow->nJobs[i]; j++){

            // reconfiguration overhead for each iteration
            range = spec->maxO - spec->minO + 1;
            workflow->O[i][j] = rand() % range + spec->minO;

            // compute app type
            unsigned int appType = rand() % 100;
            unsigned int jobType = rand() % 100;
            

            if(jobType <= moldable){
                
                workflow->malleable[i][j] = 0;
            }
            else{
                workflow->malleable[i][j] = 1;
            }

            // only iterative
            if(appType <= iterative){

                // compute the number of iterations
                range = (spec->maxIT * spec->maxPH) - (spec->minIT * spec->minPH) + 1;
                workflow->IT[i][j] = rand() % range + (spec->minIT * spec->minPH);

                // CPT of the job

                // allocate memory for the job properties per iteration
                workflow->C[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->M[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->S[i][j] = (double*)calloc(workflow->IT[i][j], sizeof(double));
                workflow->maxR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->minR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->CPT[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));


                // compute mean C and M and the std
                unsigned int meanC, meanM;
                double stdC, stdM; 
                range = spec->maxC - spec->minC + 1;
                meanC = rand() % range + spec->minC;
                stdC = sqrt((double)meanC * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));                

                range = spec->maxM - spec->minM + 1;
                meanM = rand() % range + spec->minM;
                stdM = sqrt((double)meanM * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));     

                // loop over job iterations
                for(unsigned int k = 0; k<workflow->IT[i][j]; k++){

                    // compute C and M for the iteration
                    double dC = gaussian((double)meanC, stdC);
                    if(dC < 0.0) 
                        dC = 0.0;
                    workflow->C[i][j][k] = (unsigned int)dC;
                    double dM = gaussian((double)meanM, stdM);
                    if(dM < 0.0) 
                        dM = 0.0;
                    workflow->M[i][j][k] = (unsigned int)dM;

                    workflow->CPT[i][j][k] = rand() % spec->nCPTs;
                }

                // scalability and resource request are constants during the job execution

                // resources per each iteration
                range = spec->maxR - spec->minR + 1;
                unsigned int minR = rand() % range + spec->minR;
                
                range = spec->maxR - minR + 1;
                unsigned int maxR = rand() % range + minR;

                // bound max
                if(maxR > spec->maxR){
                    
                    maxR = spec->maxR;
                }

                // scalability
                range = spec->maxS - spec->minS + 1;
                double S = (double)(rand() % range + spec->minS) / 100.0;

                for(unsigned int k = 0; k<workflow->IT[i][j]; k++){
                
                    workflow->minR[i][j][k] = minR;
                    workflow->maxR[i][j][k] = maxR;

                    if(jobType <= rigid){

                        workflow->maxR[i][j][k] = workflow->minR[i][j][k];
                    }

                    workflow->S[i][j][k] = S;
                }
            }
            // iterative phases
            else if(appType <= iterativePhases){
                
                // compute the number of iterations and the number of phases
                range = spec->maxIT - spec->minIT + 1;
                unsigned int iterations = rand() % range + spec->minIT;

                range = spec->maxPH - spec->minPH + 1;
                unsigned int phases = rand() % range + spec->minPH;

                workflow->IT[i][j] = iterations * phases;


                // allocate memory for the job properties per iteration
                workflow->C[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->M[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->S[i][j] = (double*)calloc(workflow->IT[i][j], sizeof(double));
                workflow->maxR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->minR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->CPT[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));


                // each iteration has several phases
                for (unsigned int ph = 0; ph < phases; ph++){

                    // compute mean C and M and the std
                    unsigned int meanC, meanM;
                    double stdC, stdM; 
                    range = spec->maxC - spec->minC + 1;
                    meanC = rand() % range + spec->minC;
                    stdC = sqrt((double)meanC * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));                

                    range = spec->maxM - spec->minM + 1;
                    meanM = rand() % range + spec->minM;
                    stdM = sqrt((double)meanM * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));     

                    // scalability
                    range = spec->maxS - spec->minS + 1;
                    double S = (double)(rand() % range + spec->minS) / 100.0;

                    // loop over the iterations of the phase
                    for(unsigned int k = 0; k < iterations; k++){

                        unsigned int iterationIndex = k * phases + ph;

                        // compute C and M for the iteration
                        double dC = gaussian((double)meanC, stdC);
                        if(dC < 0.0) 
                            dC = 0.0;

                        workflow->C[i][j][iterationIndex] = (unsigned int)dC;
                        
                        
                        double dM = gaussian((double)meanM, stdM);
                        if(dM < 0.0) 
                            dM = 0.0;

                        workflow->M[i][j][iterationIndex] = (unsigned int)dM;

                        workflow->CPT[i][j][iterationIndex] = rand() % spec->nCPTs;

                        workflow->S[i][j][iterationIndex] = S;
                    }
                }

                // resources per each iteration
                range = spec->maxR - spec->minR + 1;
                unsigned int minR = rand() % range + spec->minR;
                
                range = spec->maxR - minR + 1;
                unsigned int maxR = rand() % range + minR;

                // bound max
                if(maxR > spec->maxR){
                    
                    maxR = spec->maxR;
                }

                if(jobType <= rigid){

                    maxR = minR;
                }

                for(unsigned int it = 0; it<iterations * phases; it++){

                    workflow->maxR[i][j][it] = maxR;
                    workflow->minR[i][j][it] = minR;
                }
            }
            // phases iterative
            else{

                // compute the number of phases
                range = spec->maxPH - spec->minPH + 1;
                unsigned int phases = rand() % range + spec->minPH;

                // compute the number of iterations
                unsigned int iterations = 0;
                unsigned int *iterationsPerPhase = (unsigned int*)calloc(phases, sizeof(unsigned int));
                // for each phase, compute the number of iterations
                for(unsigned int ph = 0; ph < phases; ph++){

                    range = spec->maxIT - spec->minIT + 1;
                    unsigned int it = rand() % range + spec->minIT;
                    iterations += it;
                    iterationsPerPhase[ph] = it;
                }

                workflow->IT[i][j] = iterations;


                // allocate memory for the job properties per iteration
                workflow->C[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->M[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->S[i][j] = (double*)calloc(workflow->IT[i][j], sizeof(double));
                workflow->maxR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->minR[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));
                workflow->CPT[i][j] = (unsigned int*)calloc(workflow->IT[i][j], sizeof(unsigned int));

                // loop over phases
                unsigned int nextIteration = 0;
                for(unsigned int ph = 0; ph < phases; ph++){

                    // compute mean C and M and the std
                    unsigned int meanC, meanM;
                    double stdC, stdM; 
                    range = spec->maxC - spec->minC + 1;
                    meanC = rand() % range + spec->minC;
                    stdC = sqrt((double)meanC * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));                

                    range = spec->maxM - spec->minM + 1;
                    meanM = rand() % range + spec->minM;
                    stdM = sqrt((double)meanM * (0.10 + ((double)rand() / (double)RAND_MAX) * (0.4 - 0.10)));     


                    // scalability
                    range = spec->maxS - spec->minS + 1;
                    double S = (double)(rand() % range + spec->minS) / 100.0;

                    // loop over job iterations
                    for(unsigned int k = 0; k<iterationsPerPhase[ph]; k++){

                        // compute C and M for the iteration
                        double dC = gaussian((double)meanC, stdC);
                        if(dC < 0.0) 
                            dC = 0.0;

                        workflow->C[i][j][nextIteration] = (unsigned int)dC;
                        
                        
                        double dM = gaussian((double)meanM, stdM);
                        if(dM < 0.0) 
                            dM = 0.0;

                        workflow->M[i][j][nextIteration] = (unsigned int)dM;

                        workflow->CPT[i][j][nextIteration] = rand() % spec->nCPTs;

                        workflow->S[i][j][nextIteration] = S;
                



                        nextIteration ++;
                    }
                }


                // resources per each iteration
                range = spec->maxR - spec->minR + 1;
                unsigned int minR = rand() % range + spec->minR;
                
                range = spec->maxR - minR + 1;
                unsigned int maxR = rand() % range + minR;

                // bound max
                if(maxR > spec->maxR){
                    
                    maxR = spec->maxR;
                }

                if(jobType <= rigid){

                    maxR = minR;
                }

                for(unsigned int it = 0; it<iterations; it++){

                    workflow->maxR[i][j][it] = maxR;
                    workflow->minR[i][j][it] = minR;
                }
            }
        }
    }

    // write to json
    writeWorkflowJSON(out, workflow);

    return 0;
}