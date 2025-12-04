#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include "gauss_process.h"
using namespace std;


// ===========================
// wymiana wierszy
// ===========================
void swap_row(vector<vector<double>> &mat, int i, int j) {
    swap(mat[i], mat[j]);
}


// ===========================
// równoległa eliminacja Gaussa (MASTER + spawned workers)
// ===========================
int forwardElimMPI(vector<vector<double>>& mat, MPI_Comm& intercomm) 
{
    int N = mat.size();
    int workerCount = 4;
    int cols = N + 1;

    for (int k = 0; k < N; k++) 
    {
        // 1️ Wyznaczenie pivotu (master)
        int i_max = k;
        double v_max = fabs(mat[i_max][k]);
        for (int i = k + 1; i < N; i++)
            if (fabs(mat[i][k]) > v_max)
                v_max = fabs(mat[i][k]), i_max = i;

        if (fabs(mat[i_max][k]) < 1e-9) {
            return k;
        }

        if (i_max != k)
            swap_row(mat, k, i_max);

        // 2️ MASTER → WORKERS : wyślij pivot
        vector<double> pivotRow = mat[k];

        for (int w = 0; w < workerCount; w++) {
            MPI_Send(&k, 1, MPI_INT, w, 0, intercomm);
            MPI_Send(&cols, 1, MPI_INT, w, 0, intercomm);
            MPI_Send(pivotRow.data(), cols, MPI_DOUBLE, w, 0, intercomm);
        }

        // 3️ MASTER dzieli wiersze na workerów
        vector<vector<vector<double>>> assigned(workerCount);

        for (int r = k + 1; r < N; r++) {
            int w = (r - (k + 1)) % workerCount;
            assigned[w].push_back(mat[r]);
        }

        // 4️ MASTER wysyła do workerów ich wiersze
        for (int w = 0; w < workerCount; w++) 
        {
            int count = assigned[w].size();
            MPI_Send(&count, 1, MPI_INT, w, 0, intercomm);

            for (auto& row : assigned[w]) {
                MPI_Send(row.data(), cols, MPI_DOUBLE, w, 0, intercomm);
            }
        }

        // 5️ MASTER odbiera przetworzone wiersze
        for (int w = 0; w < workerCount; w++) 
        {
            int count = assigned[w].size();
            for (int i = 0; i < count; i++) {
                vector<double> row(cols);
                MPI_Recv(row.data(), cols, MPI_DOUBLE, w, 0,
                         intercomm, MPI_STATUS_IGNORE);

                int globalIdx = k + 1 + (i * workerCount) + w;
                if (globalIdx < N)
                    mat[globalIdx] = row;
            }
        }
    }
    
    return -1;
}


// ===========================
// BACK SUBSTITUTION
// ===========================
void backSub(const vector<vector<double>>& mat) 
{
    int N = mat.size();
    vector<double> x(N);

    for (int i = N - 1; i >= 0; i--) {
        x[i] = mat[i][N];
        for (int j = i + 1; j < N; j++)
            x[i] -= mat[i][j] * x[j];
        x[i] /= mat[i][i];
    }
}

// Globalne: jeden zestaw workerów dla całego procesu
static MPI_Comm global_workers_comm = MPI_COMM_NULL;
static bool workers_spawned = false;

void initializeWorkers() 
{
    if (!workers_spawned) {
        int workerCount = 4;
        MPI_Comm_spawn("./worker_worker_gauss_p", MPI_ARGV_NULL, workerCount,
                       MPI_INFO_NULL, 0, MPI_COMM_SELF, &global_workers_comm,
                       MPI_ERRCODES_IGNORE);
        workers_spawned = true;
    }
}

void terminateWorkers() 
{
    if (workers_spawned) {
        int terminate = -1;
        int workerCount = 4;
        for (int w = 0; w < workerCount; w++) {
            MPI_Send(&terminate, 1, MPI_INT, w, 0, global_workers_comm);
        }
        MPI_Comm_disconnect(&global_workers_comm);
        workers_spawned = false;
        global_workers_comm = MPI_COMM_NULL;
    }
}

// ===========================
// główna funkcja eliminacji
// ===========================
vector<vector<double>> gaussianElimination_p(vector<vector<double>> mat)
{
    // Zainicjuj workerów tylko raz
    initializeWorkers();

    auto start = chrono::high_resolution_clock::now();

    int singular = forwardElimMPI(mat, global_workers_comm);

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;

    cout << "MPI równolegle:" << endl;
    cout << "Czas (wall): " << elapsed.count() << " s\n";

    return mat;
}