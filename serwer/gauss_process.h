#ifndef GAUSS_PROCESS_H
#define GAUSS_PROCESS_H


#include <vector>

// GAUSS PROCESSES
double** create_shared_matrix_from_vector(const std::vector<std::vector<double>>& mat);

void destroy_shared_matrix(double **mat, int N);

void swap_row_p(std::vector<std::vector<double>> &mat, int i, int j);

int forwardElim_p(double **mat, int N);

void backSub_p(double **mat, int N);

void gaussianElimination_p(std::vector<std::vector<double>> &input, int clientSocket);



#endif