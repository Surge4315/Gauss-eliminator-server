#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <ctime>
#include "gauss_process.h"
using namespace std;

static int WORKER_COUNT = 15;

void swap_row_p(vector<vector<double>> &mat, int i, int j) {
    swap(mat[i], mat[j]);
}

struct TreeNode {
    int workerRank;
    int level;
    vector<int> children;
    int parent;
};

TreeNode buildDynamicTreeStructure(int workerRank, int totalWorkers) {
    TreeNode node;
    node.workerRank = workerRank;
    node.parent = -1;
    node.level = 0;
    
    if (workerRank == 0) {
        node.level = 0;
        if (1 < totalWorkers) node.children.push_back(1);
        if (2 < totalWorkers) node.children.push_back(2);
    } else {
        node.parent = (workerRank - 1) / 2;
        
        int leftChild = 2 * workerRank + 1;
        int rightChild = 2 * workerRank + 2;
        
        if (leftChild < totalWorkers) node.children.push_back(leftChild);
        if (rightChild < totalWorkers) node.children.push_back(rightChild);
        
        int temp = workerRank;
        while (temp > 0) {
            temp = (temp - 1) / 2;
            node.level++;
        }
    }
    
    return node;
}

vector<int> findLeafNodes(int totalWorkers) {
    vector<int> leaves;
    for (int i = 0; i < totalWorkers; i++) {
        TreeNode node = buildDynamicTreeStructure(i, totalWorkers);
        if (node.children.empty()) {
            leaves. push_back(i);
        }
    }
    return leaves;
}

void masterBroadcastToTree(MPI_Comm& intercomm, const vector<double>& pivotRow, 
                           int k, int cols) {
    //cout << "[Master] Broadcasting k=" << k << ", cols=" << cols << " to worker 0" << endl;
    MPI_Send(&k, 1, MPI_INT, 0, 0, intercomm);
    MPI_Send(&cols, 1, MPI_INT, 0, 0, intercomm);
    MPI_Send(pivotRow.data(), cols, MPI_DOUBLE, 0, 0, intercomm);
}

void masterDistributeRows(MPI_Comm& intercomm, vector<vector<double>>& mat, 
                          int k, int N, int cols, const vector<int>& leaves) {
    int leafCount = leaves.size();
    
    if (leafCount == 0) {
        cerr << "Error: No leaf workers available!" << endl;
        return;
    }
    
    //cout << "[Master] Distributing " << (N - k - 1) << " rows to " << leafCount << " leaf workers" << endl;
    
    vector<vector<vector<double>>> assigned(WORKER_COUNT);
    
    for (int r = k + 1; r < N; r++) {
        int leafIdx = (r - (k + 1)) % leafCount;
        int workerRank = leaves[leafIdx];
        assigned[workerRank].push_back(mat[r]);
    }
    
    // Wyślij do wszystkich workerów
    for (int w = 0; w < WORKER_COUNT; w++) {
        int count = assigned[w].size();
        //cout << "[Master] Sending " << count << " rows to worker " << w << endl;
        MPI_Send(&count, 1, MPI_INT, w, 0, intercomm);
        
        for (auto& row : assigned[w]) {
            MPI_Send(row.data(), cols, MPI_DOUBLE, w, 0, intercomm);
        }
    }
}

void masterGatherFromTree(MPI_Comm& intercomm, vector<vector<double>>& mat,
                          int k, int N, int cols) {
    int totalCount;
    //cout << "[Master] Waiting for results from worker 0..." << endl;
    MPI_Recv(&totalCount, 1, MPI_INT, 0, 0, intercomm, MPI_STATUS_IGNORE);
    
    //cout << "[Master] Receiving " << totalCount << " processed rows" << endl;
    
    for (int i = 0; i < totalCount && (k + 1 + i) < N; i++) {
        vector<double> row(cols);
        MPI_Recv(row.data(), cols, MPI_DOUBLE, 0, 0, intercomm, MPI_STATUS_IGNORE);
        mat[k + 1 + i] = row;
    }
}

int forwardElimMPI(vector<vector<double>>& mat, MPI_Comm& intercomm) 
{
    int N = mat.size();
    int cols = N + 1;
    
    vector<int> leaves = findLeafNodes(WORKER_COUNT);
    
    //cout << "[Master] Starting forward elimination, N=" << N << ", leaf workers:  " << leaves.size() << endl;

    for (int k = 0; k < N; k++) 
    {
        //cout << "\n[Master] === Iteration k=" << k << " ===" << endl;
        
        int i_max = k;
        double v_max = fabs(mat[i_max][k]);
        for (int i = k + 1; i < N; i++)
            if (fabs(mat[i][k]) > v_max)
                v_max = fabs(mat[i][k]), i_max = i;

        if (fabs(mat[i_max][k]) < 1e-9) {
            //cout << "[Master] Singular matrix detected, sending termination" << endl;
            int terminate = -1;
            MPI_Send(&terminate, 1, MPI_INT, 0, 0, intercomm);
            return k;
        }

        if (i_max != k)
            swap_row_p(mat, k, i_max);

        vector<double> pivotRow = mat[k];
        masterBroadcastToTree(intercomm, pivotRow, k, cols);
        masterDistributeRows(intercomm, mat, k, N, cols, leaves);
        masterGatherFromTree(intercomm, mat, k, N, cols);
        
        //cout << "[Master] Completed iteration k=" << k << endl;
    }
    
    //cout << "[Master] Forward elimination completed" << endl;
    return -1;
}

void backSub_p(const vector<vector<double>>& mat) 
{
    int N = mat.size();
    vector<double> x(N);

    for (int i = N - 1; i >= 0; i--) {
        x[i] = mat[i][N];
        for (int j = i + 1; j < N; j++)
            x[i] -= mat[i][j] * x[j];
        x[i] /= mat[i][i];
    }
    
    //cout << "[Master] Back substitution completed" << endl;
}

static MPI_Comm global_workers_comm = MPI_COMM_NULL;
static bool workers_spawned = false;

void initializeWorkers(int workerCount) 
{
    if (! workers_spawned) {
        if (workerCount == -1) {
            workerCount = 7; // Bezpieczna domyślna wartość
        }
        
        WORKER_COUNT = workerCount;
        
        //cout << "[Master] Spawning " << WORKER_COUNT << " workers..." << endl;
        
        int result = MPI_Comm_spawn("./worker_tree", MPI_ARGV_NULL, WORKER_COUNT,
                       MPI_INFO_NULL, 0, MPI_COMM_SELF, &global_workers_comm,
                       MPI_ERRCODES_IGNORE);
        
        if (result != MPI_SUCCESS) {
            cerr << "[Master] Failed to spawn workers!" << endl;
            MPI_Abort(MPI_COMM_SELF, 1);
        }
        
        workers_spawned = true;
        
       // cout << "[Master] Sending configuration to workers..." << endl;
        
        // Wyślij konfigurację do każdego workera
        for (int w = 0; w < WORKER_COUNT; w++) {
            MPI_Send(&WORKER_COUNT, 1, MPI_INT, w, 99, global_workers_comm);
        }
        
        //cout << "[Master] Workers initialized successfully" << endl;
    }
}

void terminateWorkers() 
{
    if (workers_spawned) {
        //cout << "[Master] Terminating workers..." << endl;
        int terminate = -1;
        MPI_Send(&terminate, 1, MPI_INT, 0, 0, global_workers_comm);
        
        // Poczekaj chwilę aby workery mogły zakończyć
        MPI_Barrier(MPI_COMM_SELF);
        
        MPI_Comm_disconnect(&global_workers_comm);
        workers_spawned = false;
        global_workers_comm = MPI_COMM_NULL;
        
        //cout << "[Master] Workers terminated" << endl;
    }
}

vector<vector<double>> gaussianElimination_p(vector<vector<double>> mat)
{
    initializeWorkers(15); // Inicjalizacja 15 workerów + 1 master = n=16

    clock_t cpu_start = clock();
    auto start = chrono::high_resolution_clock:: now();

    int singular = forwardElimMPI(mat, global_workers_comm);

    auto end = chrono:: high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    clock_t cpu_end = clock();
    double cpu_time = double(cpu_end - cpu_start) / CLOCKS_PER_SEC;

    cout << "MPI ComputeTree " << endl;
    cout << "Czas (wall): " << elapsed.count() << " s\n";
    cout << "Czas CPU: " << cpu_time << " s\n";

    return mat;
}