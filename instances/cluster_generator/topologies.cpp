#include <cstdio>
#include <iostream>
#include <cmath>
#include <queue>
#include <limits>
#include <cstdio>

#include "topologies.hpp"

// compile: g++ -g -I../cJSON -o topologyGen topologies.cpp clusterTopologies.cpp IO.cpp gpuTopologies.cpp ../cJSON/cJSON.c -lm
// execute: ./topologyGen cluster1.json out_cluster.json


int main(int argc, char* argv[]){

    // laod input paramters
    char *in = argv[1];
    char *out = argv[2];

    // read JSON file
    cJSON *json = readClusterDescription(in);
    topology_t *clusterTopology = parseClusterDescription(json);

    // build topology
    buildTopology(clusterTopology);
    buildGPUNodes(clusterTopology);

    printTopology(clusterTopology, clusterTopology->interconnect);
    printTopology(clusterTopology, clusterTopology->latencies);

    topologyToJSON(clusterTopology, out);

    // storeTopologyInFile();
    //topologyToJSON(&clusterTopology, out);
}