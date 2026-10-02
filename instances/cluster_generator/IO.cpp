#include "topologies.hpp"
#include <cstdio>

cJSON *readClusterDescription(char *in){
    
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

    if (root == NULL) {
        const char *error = cJSON_GetErrorPtr();

        if (error != NULL) {
            fprintf(stderr,
                    "Error parsing JSON near: %s\n",
                    error);
        } else {
            fprintf(stderr, "Error parsing JSON\n");
        }

        return NULL;
    }

    return root;
}


topology_t* parseClusterDescription(cJSON *root){
    
    // allocate memory for the cluster topology
    topology_t *clusterTopology = (topology_t*)calloc(1, sizeof(topology_t));
    
    if (root == NULL) {

        fprintf(stderr, "Error: invalid arguments\n");
        return NULL;
    }

    // load cluster information
    cJSON *cluster = cJSON_GetObjectItemCaseSensitive(root, "cluster");
    if (!cJSON_IsObject(cluster)) {
        
        fprintf(stderr, "Error: 'cluster' object not found in JSON\n");
        return NULL;
    }

    // auxiliary item
    cJSON *item;

    // load N
    item = cJSON_GetObjectItemCaseSensitive(cluster, "N");
    if (!cJSON_IsNumber(item)) {

        fprintf(stderr, "Error: invalid or missing 'N'\n");
        return NULL;
    }
    clusterTopology->N = (unsigned int)item->valuedouble;

    clusterTopology->interconnect = (unsigned int **)calloc(clusterTopology->N, sizeof(unsigned int*));
    clusterTopology->latencies = (unsigned int **)calloc(clusterTopology->N, sizeof(unsigned int*));
    clusterTopology->bandwidthMatrix = (unsigned int **)calloc(clusterTopology->N, sizeof(unsigned int*));
    for(unsigned int node  = 0; node < clusterTopology->N; node++){
    
        clusterTopology->interconnect[node] = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
        clusterTopology->latencies[node] = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
        clusterTopology->bandwidthMatrix[node] = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    }


    // topology
    item = cJSON_GetObjectItemCaseSensitive(cluster, "topology");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: invalid or missing 'topology'\n");
        return NULL;
    }
    clusterTopology->topo = (unsigned int)item->valuedouble;


    // linkBand
    item = cJSON_GetObjectItemCaseSensitive(cluster, "linkBand");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: invalid or missing 'linkBand'\n");
        return NULL;
    }
    clusterTopology->linkBand = (unsigned int)item->valuedouble;


    // hopLatency
    item = cJSON_GetObjectItemCaseSensitive(cluster, "hopLatency");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: invalid or missing 'hopLatency'\n");
        return NULL;
    }
    clusterTopology->hopLatency = (unsigned int)item->valuedouble;


    // k
    item = cJSON_GetObjectItemCaseSensitive(cluster, "k");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: invalid or missing 'k'\n");
        return NULL;
    }
    clusterTopology->k = (unsigned int)item->valuedouble;


    // l
    item = cJSON_GetObjectItemCaseSensitive(cluster, "l");
    if (!cJSON_IsNumber(item)) {
        
        fprintf(stderr, "Error: invalid or missing 'l'\n");
        return NULL;
    }
    clusterTopology->l = (unsigned int)item->valuedouble;


    // intra node information
    cJSON *intraNodeTopologies = cJSON_GetObjectItemCaseSensitive(root, "intraNodeTopologies");
    if (!cJSON_IsArray(intraNodeTopologies)) {
        
        fprintf(stderr, "Error: 'intraNodeTopologies' array not found\n");
        return NULL;
    }


    // allocate memory for intranode information
    clusterTopology->nGPUsNode = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    clusterTopology->intraNodeTopo = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    clusterTopology->nodeHopLatencyNVLink = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    clusterTopology->nodeHopLatencyPCIe = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    clusterTopology->nodeLinkBandNVLink = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));
    clusterTopology->nodeLinkBandPCIe = (unsigned int *)calloc(clusterTopology->N, sizeof(unsigned int));

    clusterTopology->CP = (unsigned int **)calloc(clusterTopology->N, sizeof(unsigned int*));


    // parse node ranges
    unsigned int numRanges = (unsigned int)cJSON_GetArraySize(intraNodeTopologies); // get number of ranges

    // check if the node is defined in a range
    bool *nodeDefined = (bool *)calloc(clusterTopology->N, sizeof(bool));
    if (nodeDefined == NULL) {
        
        fprintf(stderr, "Error: could not allocate node validation array\n");
        return NULL;
    }

    // loop over ranges
    for (unsigned int r = 0; r < numRanges; r++) {

        // get range r
        cJSON *range = cJSON_GetArrayItem(intraNodeTopologies, r);
        if (!cJSON_IsObject(range)) {

            fprintf(stderr, "Error: intra-node topology %u is not an object\n", r);
            free(nodeDefined);
            return NULL;
        }


        // load first node of the range
        cJSON *firstNode = cJSON_GetObjectItemCaseSensitive(range, "firstNode");
        if (!cJSON_IsNumber(firstNode)) {

            fprintf(stderr, "Error: invalid or missing 'firstNode' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }

        // load last node of the range
        cJSON *lastNode = cJSON_GetObjectItemCaseSensitive(range, "lastNode");
        if (!cJSON_IsNumber(lastNode)) {

            fprintf(stderr, "Error: invalid or missing 'lastNode' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }

        unsigned int first = (unsigned int)firstNode->valuedouble;
        unsigned int last = (unsigned int)lastNode->valuedouble;

        // validate nodes in the range
        if (first > last) {

            fprintf(stderr, "Error: invalid node range %u-%u\n", first, last);
            free(nodeDefined);
            return NULL;
        }
        if (last >= clusterTopology->N) {
            
            fprintf(stderr, "Error: node %u is outside cluster (N = %u)\n", last, clusterTopology->N);
            free(nodeDefined);
            return NULL;
        }

        // load nodes topology information
        cJSON *topology = cJSON_GetObjectItemCaseSensitive(range, "topology");
        if (!cJSON_IsNumber(topology)) {

            fprintf(stderr, "Error: invalid or missing 'topology' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }

        // NVlink latency for nodes in range
        cJSON *nvlinkLat = cJSON_GetObjectItemCaseSensitive(range, "nvlinkLat");
        if (!cJSON_IsNumber(nvlinkLat)) {
            
            fprintf(stderr, "Error: invalid or missing 'nvlinkLat' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }

        // NVLink band.
        cJSON *nvlinkBand = cJSON_GetObjectItemCaseSensitive(range,"nvlinkBand");
        if (!cJSON_IsNumber(nvlinkBand)) {

            fprintf(stderr, "Error: invalid or missing 'nvlinkBand' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }

        // PCIe latency
        cJSON *pcieLat = cJSON_GetObjectItemCaseSensitive(range, "pcieLat");
        if (!cJSON_IsNumber(pcieLat)) {

            fprintf(stderr, "Error: invalid or missing 'pcieLat' in topology %u\n", r);
            free(nodeDefined);
            return NULL;
        }


        // PCIe band.
        cJSON *pcieBand = cJSON_GetObjectItemCaseSensitive(range,"pcieBand");
        if (!cJSON_IsNumber(pcieBand)) {

            fprintf(stderr,"Error: invalid or missing 'pcieBand' in topology %u\n",r);
            free(nodeDefined);
            return NULL;
        }

        // convert json values to C++ variables
        unsigned int topologyValue = (unsigned int)topology->valuedouble;
        unsigned int nvlinkLatency = (unsigned int)nvlinkLat->valuedouble;
        unsigned int nvlinkBandwidth = (unsigned int)nvlinkBand->valuedouble;
        unsigned int pcieLatency = (unsigned int)pcieLat->valuedouble;
        unsigned int pcieBandwidth = (unsigned int)pcieBand->valuedouble;


        // assign the values to all nodes in the range
        for (unsigned int node = first;node <= last;node++) {

            // check overlaps in ranges

            if (nodeDefined[node]) {

                fprintf(stderr,"Error: node %u appears in more than one intra-node topology range\n",node);
                free(nodeDefined);
                return NULL;
            }

            nodeDefined[node] = true;
            clusterTopology->intraNodeTopo[node] = topologyValue;
            clusterTopology->nodeHopLatencyNVLink[node] = nvlinkLatency;
            clusterTopology->nodeHopLatencyPCIe[node] = pcieLatency;
            clusterTopology->nodeLinkBandNVLink[node] = nvlinkBandwidth;
            clusterTopology->nodeLinkBandPCIe[node] = pcieBandwidth;

            unsigned int nGPUs = 4;
            if(topologyValue > 3){
            
                nGPUs = 8;
            }

            // allocate memory for GPU CP values
            clusterTopology->CP[node] = (unsigned int*)calloc(nGPUs, sizeof(unsigned int));


            // get CP
            cJSON *cp = cJSON_GetObjectItem(range, "CP");

            // check if cp is an aray
            if (cJSON_IsArray(cp)) {

                int size = cJSON_GetArraySize(cp);

                for (int i = 0; i < size; i++) {
                    cJSON *item = cJSON_GetArrayItem(cp, i);

                    if (cJSON_IsNumber(item)) {
                        
                        int value = item->valueint;

                        clusterTopology->CP[node][i] = (unsigned int)value;
                    }
                }
            }
            else{

                // convert json values to C++ variables                
                for(unsigned int i = 0; i<nGPUs; i++){
                 
                    clusterTopology->CP[node][i] = (unsigned int)cp->valueint;
                }
            }
        }
    }


    // check that all nodes are in some range
    for (unsigned int node = 0;node < clusterTopology->N;node++) {

        if (!nodeDefined[node]) {

            fprintf(stderr,"Error: node %u has no intra-node topology definition\n",node);
            free(nodeDefined);
            return NULL;
        }
    }

    free(nodeDefined);


    // allocate memory for clusterTopology structure arrays
    clusterTopology->gpusInterconnect = (unsigned int ***)calloc(clusterTopology->N,sizeof(unsigned int **));
    clusterTopology->gpusBandwidthMatrix = (unsigned int ***)calloc(clusterTopology->N,sizeof(unsigned int **));
    clusterTopology->gpusLatencies = (unsigned int ***)calloc(clusterTopology->N,sizeof(unsigned int **));

    // count number of GPUs
    for(unsigned int node = 0; node < clusterTopology->N; node++){

        if(clusterTopology->intraNodeTopo[node] > 3){

            clusterTopology->nGPUsNode[node] = 8;
        }
        else{

            clusterTopology->nGPUsNode[node] = 4;
        }

        // sum the number of GPUs in the node to the total number of GPUs in the cluster
        clusterTopology->nGPUs += clusterTopology->nGPUsNode[node];


        // allocate memory for interconnection matrices
        unsigned int G = clusterTopology->nGPUsNode[node];
        clusterTopology->gpusInterconnect[node] = (unsigned int **)calloc(G,sizeof(unsigned int *));
        clusterTopology->gpusBandwidthMatrix[node] = (unsigned int **)calloc(G,sizeof(unsigned int *));
        clusterTopology->gpusLatencies[node] = (unsigned int **)calloc(G,sizeof(unsigned int *));

        for (unsigned int gpu = 0; gpu < G; gpu++) {

            clusterTopology->gpusInterconnect[node][gpu] = (unsigned int *)calloc(G, sizeof(unsigned int));
            clusterTopology->gpusBandwidthMatrix[node][gpu] = (unsigned int *)calloc(G,sizeof(unsigned int));
            clusterTopology->gpusLatencies[node][gpu] = (unsigned int *)calloc(G,sizeof(unsigned int));
        }
    }

    // return cluster topology structure
    return clusterTopology;
}


cJSON *arrayToJSON(unsigned int *arr, unsigned int N)
{
    cJSON *row = cJSON_CreateArray();

    for (unsigned int i = 0; i < N; i++) {
        cJSON_AddItemToArray(row, cJSON_CreateNumber(arr[i]));
    }
    return row;
}

// parse a matrix to a cJSON object
cJSON *matrixToJSON(unsigned int **matrix, unsigned int N)
{
    cJSON *jsonMatrix = cJSON_CreateArray();

    for (unsigned int i = 0; i < N; i++) {

        cJSON *row = cJSON_CreateArray();

        for (unsigned int j = 0; j < N; j++) {
            cJSON_AddItemToArray(
                row,
                cJSON_CreateNumber(matrix[i][j])
            );
        }

        cJSON_AddItemToArray(jsonMatrix, row);
    }

    return jsonMatrix;
}

cJSON *topologyToJSON(topology_t *clusterTopology, char *out){

    // create JSON object
    cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "N", clusterTopology->N);
    cJSON_AddNumberToObject(json, "topo", clusterTopology->topo);
    cJSON_AddNumberToObject(json, "hopLatency", clusterTopology->hopLatency);
    cJSON_AddNumberToObject(json, "linkBand", clusterTopology->linkBand);
    cJSON_AddNumberToObject(json, "k", clusterTopology->k);
    cJSON_AddNumberToObject(json, "l", clusterTopology->l);
    cJSON_AddNumberToObject(json, "nGPUs", clusterTopology->nGPUs);

    cJSON *interconnectJSON = matrixToJSON(clusterTopology->interconnect, clusterTopology->N);
    cJSON *latencyJSON = matrixToJSON(clusterTopology->latencies, clusterTopology->N);
    cJSON *bandwidthJSON = matrixToJSON(clusterTopology->bandwidthMatrix, clusterTopology->N);

    cJSON_AddItemToObject(json, "interconnect", interconnectJSON);
    cJSON_AddItemToObject(json, "latencies", latencyJSON);
    cJSON_AddItemToObject(json, "bandwidths", bandwidthJSON);

    // create array for nodes
    cJSON *nodesJSON = cJSON_CreateArray();
    for(unsigned int node = 0; node < clusterTopology->N; node++){

        cJSON *nodeJSON = cJSON_CreateObject();

        // add general node iformation
        cJSON_AddNumberToObject(nodeJSON,"node",node);
        cJSON_AddNumberToObject(nodeJSON,"nGPUs",clusterTopology->nGPUsNode[node]);
        cJSON_AddNumberToObject(nodeJSON,"intraNodeTopo",clusterTopology->intraNodeTopo[node]);

        // NVLink and PCIe information
        cJSON_AddNumberToObject(nodeJSON,"hopLatencyNVLink",clusterTopology->nodeHopLatencyNVLink[node]);
        cJSON_AddNumberToObject(nodeJSON,"hopLatencyPCIe",clusterTopology->nodeHopLatencyPCIe[node]);
        cJSON_AddNumberToObject(nodeJSON,"linkBandNVLink",clusterTopology->nodeLinkBandNVLink[node]);
        cJSON_AddNumberToObject(nodeJSON,"linkBandPCIe",clusterTopology->nodeLinkBandPCIe[node]);

        // node GPU matrices
        unsigned int G = clusterTopology->nGPUsNode[node];
        cJSON_AddItemToObject(nodeJSON, "CP", arrayToJSON(clusterTopology->CP[node], G));
        cJSON_AddItemToObject(nodeJSON,"gpusInterconnect",matrixToJSON(clusterTopology->gpusInterconnect[node],G));
        cJSON_AddItemToObject(nodeJSON,"gpusLatencies",matrixToJSON(clusterTopology->gpusLatencies[node],G));
        cJSON_AddItemToObject(nodeJSON,"gpusBandwidths",matrixToJSON(clusterTopology->gpusBandwidthMatrix[node],G));

        // add item to array
        cJSON_AddItemToArray(nodesJSON,nodeJSON);
    }

    // add array of nodes to json
    cJSON_AddItemToObject(json, "nodes", nodesJSON);

    // convert the cJSON object to a JSON string
    char *json_str = cJSON_Print(json);

    // write the JSON string to a file
    FILE *fp = fopen(out, "w");
    if (fp == NULL) {
        printf("Error: Unable to open the file.\n");
        return NULL;
    }
    printf("%s\n", json_str);
    fputs(json_str, fp);
    fclose(fp);

    // free the JSON string and cJSON object
    //cJSON_free(json_str);
    //cJSON_Delete(json);

    return json;
}