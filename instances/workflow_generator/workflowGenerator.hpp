enum CPT {

    RING,
    TREE,
    ALL2ALL
};

struct workflowSpec_t{

    unsigned int nWorkflows; // number of workflows/jobs in the workload
    unsigned int pWorkflows; // percentage of workflows in the workload 
    unsigned int deps; // whether there are dependencies or no in the workflows

    unsigned int minJobs, maxJobs; // min and max numbers of jobs per each workflow
    unsigned int maxC, minC; // range of possible computation units of each job in the workload
    unsigned int maxM, minM; // range of bytes communicated by each job
    unsigned int maxO, minO; // range of overhead values for each job in the workload
    unsigned int maxIT, minIT; // range of min and max iterations per each job
    unsigned int maxPH, minPH; // range of max and min phases per each job
    unsigned int maxR, minR; // range of possible resource amounts used by each job
    unsigned int minL, maxL;
    unsigned int minS, maxS;

    unsigned int nCPTs;
    CPT *cpts; // possible communication patterns used by the jobs in the workflow

    float pIterative, pIterativePhases, pPhasesIterative;
    float pRigid, pMoldable, pMalleable, pEvolving;
};

struct workflow_t{

    unsigned int nWorkflows; // number of workflows/jobs in the workload
    unsigned int pWorkflows; // percentage of workflows in the workload 
    unsigned int deps; // whether there are dependencies or no in the workflows
    
    unsigned int *nJobs, ***C, ***M, **O, **IT, **PH, ***maxR, ***minR, ***CPT, *l;
    double ***S;
    unsigned int **malleable;
};
