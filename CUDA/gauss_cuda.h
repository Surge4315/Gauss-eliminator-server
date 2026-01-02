#ifndef GAUSS_CUDA_H
#define GAUSS_CUDA_H

#include <vector>
using namespace std;

// Funkcja główna eliminacji Gaussa (wersja CUDA)
// Użyj extern "C" dla lepszej kompatybilności lub po prostu deklaruj normalnie
vector<vector<double>> gaussianElimination_c(vector<vector<double>> mat);

// Funkcje pomocnicze (jeśli potrzebne w innych plikach)
void swap_row_c(vector<vector<double>> &mat, int i, int j);
int forwardElim_c(vector<vector<double>> &mat);
void backSub_c(const vector<vector<double>> &mat);

#endif // GAUSS_CUDA_H
