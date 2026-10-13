#pragma once


#include "libMGSP.hpp"
#include "queue.hpp"

/* 
This file just defines and implements the job queue general class and specific policy related classes, where queues are ordered according the
the SCHED or RECONF policy when inserting elements
*/

class JobQueue : public LinkedList<job_t*> {

protected:
    unsigned int WINDOW_SIZE = 1;
    unsigned int remWindow = 0;

public:

    allocation_t* (*schedAllocator)(job_t*,cluster_t*,simulationConf_t*);
    allocation_t* (*reconfAllocator)(job_t*, JobQueue*, cluster_t*,simulationConf_t*);

    unsigned int getRemWindow(){

        return remWindow;
    }

    void addElement(Node<job_t*> *node) override {

        LinkedList::addElement(node);

        // sum to remaining elements in window if necessary
        if(remWindow < WINDOW_SIZE)
            remWindow ++;
    }

    void removeJob(job_t* job) {

        Node<job_t*> *jobNode = first;

        while(jobNode && jobNode->data->jobId != job->jobId){

            jobNode = jobNode->next;
        }

        removeElement(jobNode);
    }

    void removeElement(Node<job_t*> *node) override{

        if(N==0)
            return;

        if(node->prev){

            node->prev->next = node->next;
        }
        else{

            first = node->next;
        }

        if(node->next){

            node->next->prev = node->prev;
        }
        else{

            last = node->prev;
        }

        node->next = nullptr;
        node->prev = nullptr;
        N --;

        // remove one element from remaining elements in the window
        if(remWindow > 0)
            remWindow --;

        // update remaining window elements
        if(remWindow == 0 && N >= WINDOW_SIZE)
            remWindow = WINDOW_SIZE;

        if(remWindow == 0 && N > 0)
            remWindow = N;
    }

    // add a element to the queue following the queue policy
    virtual void addElementPolicy(Node<job_t*> *node) = 0;
    
    allocation_t* allocationPolicy(Node<job_t*> *node, cluster_t *cluster, simulationConf_t *simConf) {

        job_t *job = node->data;
        allocation_t *alloc = schedAllocator(job, cluster,simConf);

        return alloc;
    }

    virtual void schedule(EventQueue *evQueue, unsigned int t, JobQueue *pQueue, JobQueue *rQueue, cluster_t *cluster, simulationConf_t *simConf) {

        // get remaining elements in the queue window
        unsigned int nWindow = pQueue->getRemWindow();

        // get first node
        Node<job_t*> *node = pQueue->getElementByIndex(0);

        // loop while jobs are launched or there are jobs in the launch window
        bool launched = true;
        unsigned int jobCounter = 0;
        unsigned int jobIndex = 0;
        while(node && (launched || jobCounter < nWindow)){

            // try to allocate resources 
            //allocation_t *alloc = pQueue->allocationPolicy(node, cluster, simConf);
            allocation_t *alloc = pQueue->schedAllocator(node->data, cluster, simConf);

            // if there exists a possible allocation, allocate and launch job
            if(alloc){
            
                // allocate resources in the cluster
                allocateResources(alloc, cluster);

                // get job from node
                job_t *job = node->data;

                // link allocation and job
                alloc->job = job;
                job->allocation[job->currentIteration] = *(alloc);
                
                // indicate job started running
                job->ePending = t;
                job->sRunning = t;

                // launchJob: create end event and allocate resources
                launchJob(evQueue, t, job, cluster, simConf);

                // remove element from pending queue
                pQueue->removeElementByIndex(jobIndex);

                // add the job to the running queue
                rQueue->addElementPolicy(node);
            }
            else{

                launched = false;
                jobIndex++; 
            }

            // get next pending job
            node = pQueue->getElementByIndex(jobIndex);

            jobCounter ++;
        }
    }

    virtual void reconfigure(EventQueue *evQueue, unsigned int t, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf){

        // get remaining elements in the queue window
        unsigned int nWindow = rQueue->getRemWindow();

        // get first node
        Node<job_t*> *node = rQueue->getElementByIndex(0);

        // loop while jobs are launched or there are jobs in the launch window
        bool reconfigured = true;
        unsigned int jobCounter = 0;
        unsigned int jobIndex = 0;
        while(node) { //&& (reconfigured || jobCounter < nWindow) && !node->data->rcf){

            // decide whether a reconfiguration is necessary and try to allocate
            allocation_t *alloc = rQueue->reconfAllocator(node->data, rQueue, cluster, simConf);

            // if there exists a possible allocation, allocate and launch job
            if(alloc){
            
                // allocate resources in the cluster
                allocateResources(alloc, cluster);

                // get job from node
                job_t *job = node->data;

                // link allocation and job
                alloc->job = job;
                job->rcf = true;

                // allocate for next iteration
                job->allocation[job->currentIteration + 1] = (*alloc);
            }
            //else{

                //reconfigured = false; 
            //}

            jobIndex ++;
            node = rQueue->getElementByIndex(jobIndex);

            jobCounter ++;
        }
    }

    // move to utilization queue
    virtual bool expandCriteria(job_t *job) {

        // monitored data
        unsigned int util, net;
        allocation_t *jobAllocation = &job->allocation[job->currentIteration];
        util = jobAllocation->util;
        net = jobAllocation->throughput;


        allocation_t *alloc = NULL;
        unsigned int jobGPUs = jobAllocation->nGPUs;
        
        return util >= 90;
    }

    virtual bool shrinkCriteria(job_t *job) {

        // monitored data
        unsigned int util, net;
        allocation_t *jobAllocation = &job->allocation[job->currentIteration];
        util = jobAllocation->util;
        net = jobAllocation->throughput;


        allocation_t *alloc = NULL;
        unsigned int jobGPUs = jobAllocation->nGPUs;
        
        return util <= 70;
    }
};


// Windowed FCFS: order jobs following the FCFS policy, where jobs are grouped in windows of W jobs which can be scheduled in any order
class JobQueueFCFS: public JobQueue {
public:

    // FCFS: add at the end
    void addElementPolicy(Node<job_t*>* node) override {

        // add element at the end of the queue
        addElement(node);
    }
};

class JobQueueNOP: public JobQueue {
public:

    // FCFS: add at the end
    void addElementPolicy(Node<job_t*>* node) override {

        // add element at the end of the queue
        addElement(node);
    }

    void schedule(EventQueue *evQueue, unsigned int t, JobQueue *pQueue, JobQueue *rQueue, cluster_t *cluster, simulationConf_t *simConf) override {
        
        ;
    }

    void reconfigure(EventQueue *evQueue, unsigned int t, JobQueue *rQueue, JobQueue *fQueue, cluster_t *cluster, simulationConf_t *simConf) override {
        
        ;
    }
};


// initialize 
JobQueue* initSchedQueue(simulationConf_t *simConf);

// reconfiguration policies (queue orderings)
JobQueue* initReconfQueue(simulationConf_t *simConf);
