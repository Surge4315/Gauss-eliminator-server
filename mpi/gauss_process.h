#ifndef GAUSS_PROCESS_H
#define GAUSS_PROCESS_H

#include <vector>
#include <mpi.h>

std::vector<std::vector<double>> gaussianElimination_p(std::vector<std::vector<double>> mat);
void initializeWorkers();
void terminateWorkers();

#endif