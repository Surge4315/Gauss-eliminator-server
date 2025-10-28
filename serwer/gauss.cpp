#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cmath>
#include <sstream>
#include <sys/socket.h>
#include "gauss.h"

using namespace std;

void swap_row(vector<vector<double>> &mat, int i, int j) { swap(mat[i], mat[j]); }

int forwardElim(vector<vector<double>> &mat)
{
    int N = mat.size();
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
            swap_row(mat, k, i_max);

        for (int i = k + 1; i < N; i++)
        {
            double f = mat[i][k] / mat[k][k];
            for (int j = k + 1; j <= N; j++)
                mat[i][j] -= mat[k][j] * f;
            mat[i][k] = 0;
        }
    }
    return -1;
}

void backSub(const vector<vector<double>> &mat)
{
    int N = mat.size();
    vector<double> x(N);
    for (int i = N - 1; i >= 0; i--)
    {
        x[i] = mat[i][N];
        for (int j = i + 1; j < N; j++)
            x[i] -= mat[i][j] * x[j];
        x[i] = x[i] / mat[i][i];
    }

    //cout << "\nRozwiązanie układu:\n";
    //for (int i = 0; i < N; i++)
    //    cout << "x" << i + 1 << " = " << x[i] << endl;
}

void gaussianElimination(vector<vector<double>> &mat, int clientSocket)
{

    auto start_wall = chrono::high_resolution_clock::now();
    clock_t start_cpu = clock(); 

    int singular_flag = forwardElim(mat);
    stringstream ss;
    if (singular_flag != -1)
    {
        cout << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
        ss << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
        string msg = ss.str();
        send(clientSocket, msg.c_str(), msg.size(), 0);
        return;
    }
    backSub(mat);

    auto end_wall = chrono::high_resolution_clock::now();
    clock_t end_cpu = clock();
    
    chrono::duration<double> elapsed_wall = end_wall - start_wall;
    double elapsed_cpu = double(end_cpu - start_cpu) / CLOCKS_PER_SEC;

    cout<<"szeregowo:"<<endl;
    cout << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    cout << "Czas CPU: " << elapsed_cpu << " s\n";

    ss <<"szeregowo:\n";
    ss << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    ss << "Czas CPU: " << elapsed_cpu << " s"; //no need for newline since stringbuffer has newline by default
    string msg = ss.str();
    send(clientSocket, msg.c_str(), msg.size(), 0);
}