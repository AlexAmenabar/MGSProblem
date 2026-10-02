#include "topologies.hpp"
#include <cstdio>

#define PCIE_4 0
#define PCIE_4_SWITCH 1
#define NVLINK_FC_4 2
#define NVLINK_PCIE_4 3
#define PCIE_8 4
#define PCIE_8_2 5
#define PCIE_8_3 6
#define NVLINK_8_FC 7
#define NVLINK_8_SWITCH 8
#define NVLINK_PCIE_8 9


// GPU - CPU - GPU // same connection hierarchy for all GPUs
void buildPCIE_4_or_PCIE_8(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){
            
                gpusInterconnect[gpu][gpu2] = 2;
                gpusLatencies[gpu][gpu2] = nodeHopLatencyPCIe * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandPCIe;
            }
        }
    }
}

// 2 GPUs - PCIeSwitch - CPU - PCIeSwitch - 2 GPUs
void buildPCIE_4_SWITCH(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    gpusInterconnect[0][1] = 2;
    gpusInterconnect[1][0] = 2;

    gpusInterconnect[2][3] = 2;
    gpusInterconnect[3][2] = 2;


    gpusInterconnect[0][2] = 4;
    gpusInterconnect[0][3] = 4;
    gpusInterconnect[1][2] = 4;
    gpusInterconnect[1][3] = 4;

    gpusInterconnect[2][0] = 4;
    gpusInterconnect[3][0] = 4;
    gpusInterconnect[2][1] = 4;
    gpusInterconnect[3][1] = 4;

    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){

                gpusLatencies[gpu][gpu2] = nodeHopLatencyPCIe * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandPCIe;
            }
        }
    }
}

void buildNVLINK_FC_4(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){

                gpusInterconnect[gpu][gpu2] = 1;
                gpusLatencies[gpu][gpu2] = nodeHopLatencyNVLink;
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandNVLink;
            }
        }
    }
}

void buildNVLINK_PCIE_4(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    gpusInterconnect[0][1] = 1;
    gpusInterconnect[1][0] = 1;
    gpusLatencies[0][1] = nodeHopLatencyNVLink;
    gpusLatencies[1][0] = nodeHopLatencyNVLink;
    gpusBandwidthMatrix[0][1] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[1][0] = nodeLinkBandNVLink;

    gpusInterconnect[2][3] = 1;
    gpusInterconnect[3][2] = 1;
    gpusLatencies[2][3] = nodeHopLatencyNVLink;
    gpusLatencies[3][2] = nodeHopLatencyNVLink;
    gpusBandwidthMatrix[2][3] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[3][2] = nodeLinkBandNVLink;

    gpusInterconnect[0][2] = 2;
    gpusInterconnect[2][0] = 2;
    gpusLatencies[0][2] = nodeHopLatencyPCIe * 2;
    gpusLatencies[2][0] = nodeHopLatencyPCIe * 2;
    gpusBandwidthMatrix[0][2] = nodeLinkBandPCIe;
    gpusBandwidthMatrix[2][0] = nodeLinkBandPCIe;

    gpusInterconnect[0][3] = 2;
    gpusInterconnect[3][0] = 2;
    gpusLatencies[0][3] = nodeHopLatencyPCIe * 2;
    gpusLatencies[3][0] = nodeHopLatencyPCIe * 2;
    gpusBandwidthMatrix[0][3] = nodeLinkBandPCIe;
    gpusBandwidthMatrix[3][0] = nodeLinkBandPCIe;

    gpusInterconnect[1][2] = 2;
    gpusInterconnect[2][1] = 2;
    gpusLatencies[1][2] = nodeHopLatencyPCIe * 2;
    gpusLatencies[2][1] = nodeHopLatencyPCIe * 2;
    gpusBandwidthMatrix[1][2] = nodeLinkBandPCIe;
    gpusBandwidthMatrix[2][1] = nodeLinkBandPCIe;

    gpusInterconnect[1][3] = 2;
    gpusInterconnect[3][1] = 2;
    gpusLatencies[1][3] = nodeHopLatencyPCIe * 2;
    gpusLatencies[3][1] = nodeHopLatencyPCIe * 2;
    gpusBandwidthMatrix[1][3] = nodeLinkBandPCIe;
    gpusBandwidthMatrix[3][1] = nodeLinkBandPCIe;
}

void buildPCIE_8_2(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    gpusInterconnect[0][1] = 2;
    gpusInterconnect[0][2] = 2;
    gpusInterconnect[0][3] = 2;

    gpusInterconnect[1][0] = 2;
    gpusInterconnect[1][2] = 2;
    gpusInterconnect[1][3] = 2;

    gpusInterconnect[2][0] = 2;
    gpusInterconnect[2][1] = 2;
    gpusInterconnect[2][3] = 2;

    gpusInterconnect[3][0] = 2;
    gpusInterconnect[3][1] = 2;
    gpusInterconnect[3][2] = 2;


    gpusInterconnect[4][5] = 2;
    gpusInterconnect[4][6] = 2;
    gpusInterconnect[4][7] = 2;

    gpusInterconnect[5][4] = 2;
    gpusInterconnect[5][6] = 2;
    gpusInterconnect[5][7] = 2;

    gpusInterconnect[6][4] = 2;
    gpusInterconnect[6][5] = 2;
    gpusInterconnect[6][7] = 2;

    gpusInterconnect[7][4] = 2;
    gpusInterconnect[7][5] = 2;
    gpusInterconnect[7][6] = 2;



    gpusInterconnect[0][4] = 4;
    gpusInterconnect[0][5] = 4;
    gpusInterconnect[0][6] = 4;
    gpusInterconnect[0][7] = 4;

    gpusInterconnect[1][4] = 4;
    gpusInterconnect[1][5] = 4;
    gpusInterconnect[1][6] = 4;
    gpusInterconnect[1][7] = 4;

    gpusInterconnect[2][4] = 4;
    gpusInterconnect[2][5] = 4;
    gpusInterconnect[2][6] = 4;
    gpusInterconnect[2][7] = 4;

    gpusInterconnect[3][4] = 4;
    gpusInterconnect[3][5] = 4;
    gpusInterconnect[3][6] = 4;
    gpusInterconnect[3][7] = 4;


    gpusInterconnect[4][0] = 4;
    gpusInterconnect[4][1] = 4;
    gpusInterconnect[4][2] = 4;
    gpusInterconnect[4][3] = 4;

    gpusInterconnect[5][0] = 4;
    gpusInterconnect[5][1] = 4;
    gpusInterconnect[5][2] = 4;
    gpusInterconnect[5][3] = 4;

    gpusInterconnect[6][0] = 4;
    gpusInterconnect[6][1] = 4;
    gpusInterconnect[6][2] = 4;
    gpusInterconnect[6][3] = 4;

    gpusInterconnect[7][0] = 4;
    gpusInterconnect[7][1] = 4;
    gpusInterconnect[7][2] = 4;
    gpusInterconnect[7][3] = 4;



    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){

                gpusLatencies[gpu][gpu2] = nodeHopLatencyPCIe * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandPCIe;
            }
        }
    }
}

void buildPCIE_8_3(topology_t *topology, unsigned int node){

   // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU

    // switch groups
    gpusInterconnect[0][1] = 2;
    gpusInterconnect[1][0] = 2;

    gpusInterconnect[2][3] = 2;
    gpusInterconnect[3][2] = 2;

    gpusInterconnect[4][5] = 2;
    gpusInterconnect[5][4] = 2;

    gpusInterconnect[6][7] = 2;
    gpusInterconnect[7][6] = 2;

    // CPU groups
    gpusInterconnect[0][2] = 3;
    gpusInterconnect[2][0] = 3;
    gpusInterconnect[0][3] = 3;
    gpusInterconnect[3][0] = 3;

    gpusInterconnect[1][2] = 3;
    gpusInterconnect[2][1] = 3;
    gpusInterconnect[1][3] = 3;
    gpusInterconnect[3][1] = 3;

    gpusInterconnect[4][6] = 3;
    gpusInterconnect[6][4] = 3;
    gpusInterconnect[4][7] = 3;
    gpusInterconnect[7][4] = 3;

    gpusInterconnect[5][6] = 3;
    gpusInterconnect[6][5] = 3;
    gpusInterconnect[5][7] = 3;
    gpusInterconnect[7][5] = 3;

    // max distance
    gpusInterconnect[0][4] = 4;
    gpusInterconnect[0][5] = 4;
    gpusInterconnect[0][6] = 4;
    gpusInterconnect[0][7] = 4;

    gpusInterconnect[1][4] = 4;
    gpusInterconnect[1][5] = 4;
    gpusInterconnect[1][6] = 4;
    gpusInterconnect[1][7] = 4;

    gpusInterconnect[2][4] = 4;
    gpusInterconnect[2][5] = 4;
    gpusInterconnect[2][6] = 4;
    gpusInterconnect[2][7] = 4;

    gpusInterconnect[3][4] = 4;
    gpusInterconnect[3][5] = 4;
    gpusInterconnect[3][6] = 4;
    gpusInterconnect[3][7] = 4;

    gpusInterconnect[4][0] = 4;
    gpusInterconnect[4][1] = 4;
    gpusInterconnect[4][2] = 4;
    gpusInterconnect[4][3] = 4;

    gpusInterconnect[5][0] = 4;
    gpusInterconnect[5][1] = 4;
    gpusInterconnect[5][2] = 4;
    gpusInterconnect[5][3] = 4;

    gpusInterconnect[6][0] = 4;
    gpusInterconnect[6][1] = 4;
    gpusInterconnect[6][2] = 4;
    gpusInterconnect[6][3] = 4;

    gpusInterconnect[7][0] = 4;
    gpusInterconnect[7][1] = 4;
    gpusInterconnect[7][2] = 4;
    gpusInterconnect[7][3] = 4;


    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){

                gpusLatencies[gpu][gpu2] = nodeHopLatencyPCIe * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandPCIe;
            }
        }
    } 
}

void buildNVLINK_FC_8(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){
            
                gpusInterconnect[gpu][gpu2] = 1;
                gpusLatencies[gpu][gpu2] = nodeHopLatencyNVLink * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandNVLink;
            }
        }
    }
}

void buildNVLINK_8_SWITCH(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // FC of 4 GPUs
    for(unsigned int gpu = 0; gpu<nGPUsNode / 2; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode / 2; gpu2++){
    
            if(gpu != gpu2){
            
                gpusInterconnect[gpu][gpu2] = 1;
            }
        }
    }

    // FC of 4 GPUs
    for(unsigned int gpu = nGPUsNode / 2; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = nGPUsNode / 2; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){
            
                gpusInterconnect[gpu][gpu2] = 1;
            }
        }
    }

    // switch (two hops)
    gpusInterconnect[0][4] = 2;
    gpusInterconnect[0][5] = 2;
    gpusInterconnect[0][6] = 2;
    gpusInterconnect[0][7] = 2;

    gpusInterconnect[1][4] = 2;
    gpusInterconnect[1][5] = 2;
    gpusInterconnect[1][6] = 2;
    gpusInterconnect[1][7] = 2;

    gpusInterconnect[2][4] = 2;
    gpusInterconnect[2][5] = 2;
    gpusInterconnect[2][6] = 2;
    gpusInterconnect[2][7] = 2;

    gpusInterconnect[3][4] = 2;
    gpusInterconnect[3][5] = 2;
    gpusInterconnect[3][6] = 2;
    gpusInterconnect[3][7] = 2;

    gpusInterconnect[4][0] = 2;
    gpusInterconnect[4][1] = 2;
    gpusInterconnect[4][2] = 2;
    gpusInterconnect[4][3] = 2;

    gpusInterconnect[5][0] = 2;
    gpusInterconnect[5][1] = 2;
    gpusInterconnect[5][2] = 2;
    gpusInterconnect[5][3] = 2;

    gpusInterconnect[6][0] = 2;
    gpusInterconnect[6][1] = 2;
    gpusInterconnect[6][2] = 2;
    gpusInterconnect[6][3] = 2;

    gpusInterconnect[7][0] = 2;
    gpusInterconnect[7][1] = 2;
    gpusInterconnect[7][2] = 2;
    gpusInterconnect[7][3] = 2;

    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){
            
                gpusLatencies[gpu][gpu2] = nodeHopLatencyNVLink * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandNVLink;
            }
        }
    }
}

void buildNVLINK_PCIE_8(topology_t *topology, unsigned int node){

    // load information for the node
    unsigned int nGPUsNode = topology->nGPUsNode[node];
    unsigned int **gpusInterconnect = topology->gpusInterconnect[node];
    unsigned int **gpusBandwidthMatrix = topology->gpusBandwidthMatrix[node];
    unsigned int **gpusLatencies = topology->gpusLatencies[node];
    unsigned int intraNodeTopo = topology->intraNodeTopo[node];
    unsigned int nodeHopLatencyNVLink = topology->nodeHopLatencyNVLink[node];
    unsigned int nodeHopLatencyPCIe = topology->nodeHopLatencyPCIe[node];
    unsigned int nodeLinkBandNVLink = topology->nodeLinkBandNVLink[node];
    unsigned int nodeLinkBandPCIe = topology->nodeLinkBandPCIe[node];

    // two hops from any GPU to any other GPU
    for(unsigned int gpu = 0; gpu<nGPUsNode; gpu++){

        for(unsigned int gpu2 = 0; gpu2<nGPUsNode; gpu2++){
    
            if(gpu != gpu2){
            
                gpusInterconnect[gpu][gpu2] = 2;
                gpusLatencies[gpu][gpu2] = nodeHopLatencyPCIe * gpusInterconnect[gpu][gpu2];
                gpusBandwidthMatrix[gpu][gpu2] = nodeLinkBandPCIe;
            }
        }
    }

    gpusInterconnect[0][1] = 1;
    gpusInterconnect[1][0] = 1;
    gpusInterconnect[2][3] = 1;
    gpusInterconnect[3][2] = 1;

    gpusInterconnect[4][5] = 1;
    gpusInterconnect[5][4] = 1;
    gpusInterconnect[6][7] = 1;
    gpusInterconnect[7][6] = 1;

    gpusLatencies[0][1] = nodeHopLatencyNVLink * gpusInterconnect[0][1];
    gpusLatencies[1][0] = nodeHopLatencyNVLink * gpusInterconnect[1][0];
    gpusLatencies[2][3] = nodeHopLatencyNVLink * gpusInterconnect[2][3];
    gpusLatencies[3][2] = nodeHopLatencyNVLink * gpusInterconnect[3][2];

    gpusLatencies[4][5] = nodeHopLatencyNVLink * gpusInterconnect[4][5];
    gpusLatencies[5][4] = nodeHopLatencyNVLink * gpusInterconnect[5][4];
    gpusLatencies[6][7] = nodeHopLatencyNVLink * gpusInterconnect[6][7];
    gpusLatencies[7][6] = nodeHopLatencyNVLink * gpusInterconnect[7][6];

    gpusBandwidthMatrix[0][1] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[1][0] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[2][3] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[3][2] = nodeLinkBandNVLink;

    gpusBandwidthMatrix[4][5] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[5][4] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[6][7] = nodeLinkBandNVLink;
    gpusBandwidthMatrix[7][6] = nodeLinkBandNVLink;
}


void buildGPUNodes(topology_t *topology){

    for(unsigned int node = 0; node < topology->N; node ++){

        switch(topology->intraNodeTopo[node]){
            case PCIE_4:
                buildPCIE_4_or_PCIE_8(topology, node);
                break;
            case PCIE_4_SWITCH:
                buildPCIE_4_SWITCH(topology, node);
                break;
            case NVLINK_FC_4:
                buildNVLINK_FC_4(topology, node);
                break;
            case NVLINK_PCIE_4:
                buildNVLINK_PCIE_4(topology, node);
                break;
            case PCIE_8:
                buildPCIE_4_or_PCIE_8(topology,node);
                break;
            case PCIE_8_2:
                buildPCIE_8_2(topology, node);
                break;
            case PCIE_8_3:
                buildPCIE_8_3(topology, node);
                break;
            case NVLINK_8_FC:
                buildNVLINK_FC_8(topology, node);
                break;
            case NVLINK_8_SWITCH:
                buildNVLINK_8_SWITCH(topology, node);
                break;
            case NVLINK_PCIE_8:
                buildNVLINK_PCIE_8(topology, node);
                break;
        }
    }
}