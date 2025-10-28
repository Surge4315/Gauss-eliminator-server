#include <iostream>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <cmath>
#include <sstream>
#include <unistd.h>   // for fork()
#include <sys/wait.h> // for wait()
#include <sys/mman.h> // for mmap (shared memory)
#include <sys/socket.h>
#include "gauss_process.h"
using namespace std;

#define NUM_PROCS 4

double **create_shared_matrix_from_vector(const vector<vector<double>> &mat)
{
    int N = mat.size();
    // Allocate array of row pointers
    double **shared = (double **)mmap(nullptr, N * sizeof(double *),
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED)
    {
        perror("mmap row pointers failed");
        exit(1);
    }

    // Allocate each row and copy data
    for (int i = 0; i < N; i++)
    {
        shared[i] = (double *)mmap(nullptr, (N + 1) * sizeof(double),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (shared[i] == MAP_FAILED)
        {
            perror("mmap row failed");
            exit(1);
        }
        for (int j = 0; j <= N; j++)
            shared[i][j] = mat[i][j];
    }
    return shared;
}

void destroy_shared_matrix(double **mat, int N)
{
    for (int i = 0; i < N; i++)
        munmap(mat[i], (N + 1) * sizeof(double));
    munmap(mat, N * sizeof(double *));
}

void swap_row_p(vector<vector<double>> &mat, int i, int j)
{
    swap(mat[i], mat[j]);
}

int forwardElim_p(double **mat, int N)
{
    for (int k = 0; k < N; k++)
    {
        int i_max = k;
        double v_max = fabs(mat[i_max][k]);
        for (int i = k + 1; i < N; i++)
            if (fabs(mat[i][k]) > v_max)
                v_max = fabs(mat[i][k]), i_max = i;

        if (fabs(mat[i_max][k]) < 1e-9)
            return k;

        if (i_max != k)
            for (int j = 0; j <= N; j++)
                swap(mat[k][j], mat[i_max][j]);

        int rows_per_proc = (N - (k + 1) + NUM_PROCS - 1) / NUM_PROCS; // podział wierszy między procesy

        for (int p = 0; p < NUM_PROCS; p++)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                // Zakres wierszy dla tego procesu
                int start = k + 1 + p * rows_per_proc;
                int end = std::min(N, start + rows_per_proc);

                for (int i = start; i < end; i++)
                {
                    double f = mat[i][k] / mat[k][k];
                    for (int j = k + 1; j <= N; j++)
                        mat[i][j] -= mat[k][j] * f;
                    mat[i][k] = 0;
                }

                _exit(0); // zakończ proces potomny
            }
        }

        // Czekamy na zakończenie wszystkich 4 procesów
        while (wait(nullptr) > 0)
            ;
    }
    return -1;
}

void backSub_p(double **mat, int N)
{
    vector<double> x(N);
    for (int i = N - 1; i >= 0; i--)
    {
        x[i] = mat[i][N];
        for (int j = i + 1; j < N; j++)
            x[i] -= mat[i][j] * x[j];
        x[i] = x[i] / mat[i][i];
    }

    // cout << "\nRozwiązanie układu:\n";
    // for (int i = 0; i < N; i++)
    //     cout << "x" << i + 1 << " = " << x[i] << endl;
}

void gaussianElimination_p(vector<vector<double>> &input, int clientSocket)
{
    int N = input.size();
    double **mat = create_shared_matrix_from_vector(input);

    auto start_wall = chrono::high_resolution_clock::now();
    clock_t start_cpu = clock();

    int singular_flag = forwardElim_p(mat, N);
    stringstream ss;
    if (singular_flag != -1)
    {
        cout << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
        ss << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
        string msg = ss.str();
        send(clientSocket, msg.c_str(), msg.size(), 0);
        return;
    }
    backSub_p(mat, N);

    auto end_wall = chrono::high_resolution_clock::now();
    clock_t end_cpu = clock();

    chrono::duration<double> elapsed_wall = end_wall - start_wall;
    double elapsed_cpu = double(end_cpu - start_cpu) / CLOCKS_PER_SEC;

    cout << "z procesami:\n";
    cout << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    cout << "Czas CPU: " << elapsed_cpu << " s\n";

    ss << "z procesami:\n";
    ss << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    ss << "Czas CPU: " << elapsed_cpu << " s\n";
    string msg = ss.str();
    send(clientSocket, msg.c_str(), msg.size(), 0);
}