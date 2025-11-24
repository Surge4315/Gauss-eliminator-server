#ifndef MACIERZ_H
#define MACIERZ_H


#include <vector>


std::vector<std::vector<double>> generateDiagonallyDominantMatrix(int n);

std::vector<double> generateVectorB(int n);

std::vector<std::vector<double>> createAugmentedMatrix(const std::vector<std::vector<double>>& A, const std::vector<double>& b);

void printSystem(const std::vector<std::vector<double>>& A, const std::vector<double>& b);




#endif