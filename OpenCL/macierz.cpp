#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <unistd.h>     // for fork()
#include <sys/wait.h>   // for wait()
#include <sys/mman.h>   // for mmap (shared memory)
#include "macierz.h"

using namespace std;



// Funkcja generująca macierz diagonalnie dominującą
vector<vector<double>> generateDiagonallyDominantMatrix(int n) {
    vector<vector<double>> A(n, vector<double>(n));

    srand(time(0));

    for (int i = 0; i < n; i++) {
        double sum = 0;
        for (int j = 0; j < n; j++) {
            if (i != j) {
                A[i][j] = rand() % 10; // losowe liczby od 0 do 9
                sum += abs(A[i][j]);
            }
        }
        // ustawienie elementu diagonalnego większego niż suma reszty w wierszu
        A[i][i] = sum + rand() % 10 + 1;
    }

    return A;
}

// Funkcja generująca wektor wyrazów wolnych
vector<double> generateVectorB(int n) {
    vector<double> b(n);
    for (int i = 0; i < n; i++) {
        b[i] = rand() % 20; // losowe liczby od 0 do 19
    }
    return b;
}

// stworzenie macierzy rozszerzonej (A|b)
vector<vector<double>> createAugmentedMatrix(const vector<vector<double>>& A, const vector<double>& b) {
    int N = A.size();
    vector<vector<double>> mat(N, vector<double>(N + 1));
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            mat[i][j] = A[i][j];
    for (int i = 0; i < N; i++)
        mat[i][N] = b[i];
    return mat;
}

// Funkcja do wyświetlania macierzy rozszerzonej (A|b)
void printSystemAug(const vector<vector<double>>& Ab) {
    int n = Ab.size();        // liczba wierszy
    int m = Ab[0].size();     // liczba kolumn (n + 1)

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m - 1; j++) {
            cout << Ab[i][j] << "\t";
        }
        cout << "| " << Ab[i][m - 1] << endl;   // ostatnia kolumna to b
    }
}

// Funkcja do wyświetlania macierzy i wektora
void printSystem(const vector<vector<double>>& A, const vector<double>& b) {
    int n = A.size();
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            cout << A[i][j] << "\t";
        }
        cout << "| " << b[i] << endl;
    }
}
