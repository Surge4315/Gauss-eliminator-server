#ifndef GAUSS_H
#define GAUSS_H


#include <vector>


// GAUSS
void swap_row(std::vector<std::vector<double>> &mat, int i, int j);

int forwardElim(std::vector<std::vector<double>> &mat);

void backSub(const std::vector<std::vector<double>> &mat);

void gaussianElimination(std::vector<std::vector<double>> &mat, int clientSocket);



#endif