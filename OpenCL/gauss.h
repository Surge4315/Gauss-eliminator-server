#ifndef GAUSS_H
#define GAUSS_H


#include <vector>


// GAUSS
void swap_row(std::vector<std::vector<double>> &mat, int i, int j);

int forwardElim(std::vector<std::vector<double>> &mat);

void backSub(const std::vector<std::vector<double>> &mat);

std::vector<std::vector<double>> gaussianElimination(std::vector<std::vector<double>> mat);

// OpenCL version
std::vector<std::vector<double>> gaussianElimination_opencl(std::vector<std::vector<double>> mat);


#endif