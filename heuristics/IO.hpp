#pragma once

class JobQueue;

void writeSimulationResults(const char *allocationFilename, const char *summaryFilename, JobQueue *fQueue, unsigned int makespan);
