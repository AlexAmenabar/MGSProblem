#include "cJSON.h"
#include <cstdlib>

struct topology_t {

    // cluster information
    unsigned int N;
    unsigned int **interconnect; // N x N: matrix of direct connectivity (topology)
    unsigned int **bandwidthMatrix; // N x N: matrix with bandwidths
    unsigned int **latencies; // N x N: matrix of latencies
    unsigned int topo;
    unsigned int hopLatency; 
    unsigned int linkBand;
    unsigned int k, l; // fat-tree specific, k = number of ports, l = number of levels in the tree

    // nodes information
    unsigned int nGPUs; // number of GPUs in the cluster
    unsigned int *nGPUsNode; // number of GPUs on each node
    unsigned int ***gpusInterconnect;
    unsigned int ***gpusBandwidthMatrix;
    unsigned int ***gpusLatencies;
    unsigned int *intraNodeTopo;
    unsigned int **CP; // processing capacity of each GPU on each node 
    unsigned int *nodeHopLatencyNVLink;
    unsigned int *nodeHopLatencyPCIe;
    unsigned int *nodeLinkBandNVLink;
    unsigned int *nodeLinkBandPCIe;

};

// load cluster description
cJSON *readClusterDescription(char *in);

topology_t* parseClusterDescription(cJSON *root);

// cluster topologies
void buildTopology(topology_t *clusterTopology);
void printTopology(topology_t *clusterTopology, unsigned int **matrix);

void buildGPUNodes(topology_t *topology);


cJSON *topologyToJSON(topology_t *clusterTopology, char *out);
