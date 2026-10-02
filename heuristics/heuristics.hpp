#ifndef HEURISTICS_HPP
#define HEURISTICS_HPP

#include "instance.hpp"
#include "queue.hpp"
#include "jobQueue.hpp"
#include "eventQueue.hpp"

void FCFS(EventQueue *evQueue, event_t *event, cluster_t *cluster, simulationConf_t *simConf);
void deallocateResources(job_t *job, cluster_t *cluster);


#endif