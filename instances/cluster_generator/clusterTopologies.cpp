#include <cstdio>
#include <iostream>
#include <cmath>
#include <queue>
#include <limits>
#include <cstdio>

#include "topologies.hpp"

// define clusters
#define MESH 0
#define TORUS 1
#define HYPERCUBE 2
#define FATTREE 3
#define DRAGONFLY 4


/* Topology building functions */

// compute the number of hops required from node A to node B for mesh and torus
void computeHopsMeshTorus(topology_t *clusterTopology){

    unsigned int N = clusterTopology->N;
    unsigned int **interconnect = clusterTopology->interconnect;
    unsigned int hopLatency = clusterTopology->hopLatency;

    unsigned int n = sqrt(N); // nodes on each row of the mesh

    // set first level latencies
    for(unsigned int node = 0; node < N; node++){

        for(unsigned int goal = 0; goal<N; goal++){

            if(node == goal){

                continue;
            }

            // if directly connected, set latency
            if(interconnect[node][goal]){
                
                continue;
            }

            // BFS

            // initialize queue to store nodes
            std::queue<unsigned int> queue;

            // number of hops from source to goal
            unsigned int *distance =
                (unsigned int*)calloc(N, sizeof(unsigned int));

            // Whether each node has already been visited
            unsigned int *explored = new unsigned int[N];

            for(unsigned int i = 0; i < N; i++){
                explored[i] = 0;
            }

            // current node
            queue.push(node);
            explored[node] = 1;
            distance[node] = 0;

            // final number of hops
            unsigned int hops = 0;

            
            while(!queue.empty()){

                // get first element in the queue
                unsigned int v = queue.front();
                queue.pop();

                // goal found (the first that finds it will be the shortest path)
                if(v == goal){

                    hops = distance[goal];
                    break;
                }

                // loop over nodes connected to v
                for(unsigned int i = 0; i<N; i++){

                    if(interconnect[v][i] && !explored[i]){

                        explored[i] = 1;
                        distance[i] = distance[v] + 1;
                        queue.push(i);
                    }
                }
            }

            delete(explored);
            free(distance);

            interconnect[node][goal] = hops;
        }
    }
}

// compute bandwidth for mesh and torus
void computeBandwidthMeshTorus(topology_t *clusterTopology){

    unsigned int N = clusterTopology->N;
    unsigned int **bandwidthMatrix = clusterTopology->bandwidthMatrix;

    // set first level latencies
    for(unsigned int i = 0; i < N; i++){

        for(unsigned int j = 0; j<N; j++){

            if(i!=j){
                bandwidthMatrix[i][j] = clusterTopology->linkBand;
            }
        }
    }
}

void computeLatenciesMeshTorus(topology_t *clusterTopology){

    unsigned int N = clusterTopology->N;
    unsigned int **interconnect = clusterTopology->interconnect;
    unsigned int **latencies = clusterTopology->latencies;

    // set first level latencies
    for(unsigned int i = 0; i < N; i++){

        for(unsigned int j = 0; j<N; j++){

            latencies[i][j] = clusterTopology->hopLatency * interconnect[i][j];
        }
    }
}

// cluster topology
void buildMesh(topology_t *clusterTopology){

    unsigned int i,j;
    unsigned int N = clusterTopology->N;
    unsigned int **interconnect = clusterTopology->interconnect;

    unsigned int n = sqrt(N); // nodes on each row of the mesh

    // loop over nodes
    for(i = 0; i<N; i++){
        
        // check boundaries
        if(i >= n){

            // connected to the node in the previous row
            interconnect[i][i-n] = 1;
        }
        if(i < N-n){

            // connected to the following row
            interconnect[i][i+n] = 1;
        }

        if(i % n > 0){

            // connected to the node in the left
            interconnect[i][i-1] = 1;
        }
        if(i % n < n-1){

            // connected to the node in the right
            interconnect[i][i+1] = 1;
        }
    }

    // compute number of hops, bandwidths and latencies
    computeHopsMeshTorus(clusterTopology);
    computeBandwidthMeshTorus(clusterTopology);
    computeLatenciesMeshTorus(clusterTopology);
}

void buildTorus(topology_t *clusterTopology){

    unsigned int i,j;
    unsigned int N = clusterTopology->N;
    unsigned int **interconnect = clusterTopology->interconnect;

    unsigned int n = sqrt(N); // nodes on each row of the mesh

    // loop over nodes
    for(i = 0; i<N; i++){
        
        // elements in the firts row
        if(i < n){

            // connected to the node in the last row
            interconnect[i][i + n*(n-1)] = 1;
        }

        // elements in the last row
        if(i >= N-n){

            // connected to the node in the first row
            interconnect[i][i - n*(n-1)] = 1;
        }

        // nodes in the left
        if(i % n == 0){

            // connected to the node in the right of the torus
            interconnect[i][i+n-1] = 1;
        }

        if(i % n >= n-1){

            interconnect[i][i-n+1] = 1;
        }
    }

    buildMesh(clusterTopology);
}

void buildHypercube(topology_t *clusterTopology){


}

void buildFatTree(topology_t *clusterTopology){

    unsigned int i,j;
    unsigned int N = clusterTopology->N;
    unsigned int k = clusterTopology->k;
    unsigned int l = clusterTopology->l;
    unsigned int **interconnect = clusterTopology->interconnect;
    unsigned int **latencies = clusterTopology->latencies;
    unsigned int **bandwidthMatrix = clusterTopology->bandwidthMatrix;
    unsigned int linkBand = clusterTopology->linkBand;

    // number of compute nodes connected to each leaf switch
    unsigned int groupSize = N / (pow(k, l-1));
    unsigned int groupIndex;
        
    // compute hops between nodes
    for(i = 0; i<N; i++){

        // compute group index
        groupIndex = i / groupSize;

        // nodes in the same group have a distance of 2
        for(j = 0; j<groupSize; j++){

            if(i != groupIndex * groupSize + j){

                interconnect[i][groupIndex * groupSize + j] = 2;
            }
        }

        // loop over levels and fill
        for(int level = l-2; level >= 0; level --){

            // compute previous group size (previous layer)
            unsigned int prevGroupSize = N / pow(k, level + 1);

            // compute group size in the current layer
            unsigned int lvlGroupSize = N / pow(k, level);

            // compute the subgroup index of the node on each group
            unsigned int prevGroupIndex = i / prevGroupSize % k; // local index in its gro
            unsigned int lvlGroupIndex = i / lvlGroupSize;

            //printf(" i = %u, lvlGroupSize = %u, lvlGroupIndex = %u, prevGroupSize = %u, prevGroupIndex = %u, level = %d\n", 
            //        i, lvlGroupSize, lvlGroupIndex, prevGroupSize, prevGroupIndex, level);

            // loop over groups and set the distance for current nodes
            for(unsigned int g = 0; g < k; g++){

                if(g != prevGroupIndex){

                    for(j = g * prevGroupSize + lvlGroupIndex * lvlGroupSize; j<g * prevGroupSize + prevGroupSize + lvlGroupIndex * lvlGroupSize; j++){

                        interconnect[i][j] = (l - level) * 2; 
                    }
                }
            }
        }
    }

    for(unsigned int i = 0; i<N; i++){
        
        for(unsigned int j = 0; j<N; j++){

            if(i!=j){
                bandwidthMatrix[i][j] = linkBand;
                latencies[i][j] = clusterTopology->hopLatency * interconnect[i][j];
            }
        }
    }
}

void buildDragonfly(topology_t *clusterTopology){


}

void buildTopology(topology_t *clusterTopology){

    // select the topology to build
    switch(clusterTopology->topo){
        case MESH:
            buildMesh(clusterTopology);
            break;
        case TORUS:
            buildTorus(clusterTopology);
            break;
        case HYPERCUBE:
            buildHypercube(clusterTopology);
            break;
        case FATTREE:
            buildFatTree(clusterTopology);
            break;
        case DRAGONFLY:
            buildDragonfly(clusterTopology);
            break;
    }
}



void printMesh(topology_t *clusterTopology, unsigned int **matrix){

    unsigned int N = clusterTopology->N;

    unsigned int n = sqrt(N);

    for(unsigned int node = 0; node<N; node++){

        printf("-- Printing node %u\n==============\n", node);
        for(unsigned int i = 0; i<n; i++){

            for(unsigned int j = 0; j<n; j++){

                printf("%u ", matrix[node][i * n + j]);
            }
            printf("\n");
        }
        printf("\n");
    }
}

void printTorus(topology_t *clusterTopology, unsigned int **matrix){
    

}

void printHyperCube(topology_t *clusterTopology, unsigned int **matrix){
    

}

void printFatTree(topology_t *clusterTopology, unsigned int **matrix){

    unsigned int N = clusterTopology->N;
    for(unsigned int i = 0; i<N; i++){

        printf("-- Printing node %u\n==============\n", i);

        for(unsigned int j = 0; j<N; j++){

            printf("%u ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    
    return;
}

void printDragonfly(topology_t *clusterTopology, unsigned int **matrix){
    return;
}

void printTopology(topology_t *clusterTopology, unsigned int **matrix){

    switch(clusterTopology->topo){
        case MESH:
            printMesh(clusterTopology, matrix);
            break;
        case TORUS:
            printMesh(clusterTopology, matrix); // mesh and torus are printed equally
            break;
        case HYPERCUBE:
            printHyperCube(clusterTopology, matrix);
            break;
        case FATTREE:
            printFatTree(clusterTopology, matrix);
            break;
        case DRAGONFLY:
            printDragonfly(clusterTopology, matrix);
            break;
    }
}


