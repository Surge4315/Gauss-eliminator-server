#include <mpi.h>
#include <iostream>
#include <vector>

struct TreeNode {
    int workerRank;
    int level;
    std::vector<int> children;
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

void eliminateRows(int k, const std:: vector<double>& pivotRow,
                   std::vector<std::vector<double>>& rows)
{
    for (auto& row : rows) {
        double factor = row[k] / pivotRow[k];
        for (size_t j = k; j < row.size(); ++j) {
            row[j] -= factor * pivotRow[j];
        }
    }
}

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    MPI_Comm parent_comm;
    MPI_Comm_get_parent(&parent_comm);

    if (parent_comm == MPI_COMM_NULL) {
        std::cerr << "No parent communicator!" << std::endl;
        MPI_Finalize();
        return 1;
    }

    // Uzyskaj rank w grupie workerów
    int workerRank, totalWorkers;
    MPI_Comm_rank(MPI_COMM_WORLD, &workerRank);
    MPI_Comm_size(MPI_COMM_WORLD, &totalWorkers);

    //std::cout << "[Worker " << workerRank << "] Started, total workers: " << totalWorkers << std::endl;

    // Odbierz konfigurację od mastera
    int configWorkers;
    MPI_Recv(&configWorkers, 1, MPI_INT, 0, 99, parent_comm, MPI_STATUS_IGNORE);
    
    //std::cout << "[Worker " << workerRank << "] Received config: " << configWorkers << " workers" << std::endl;

    TreeNode node = buildDynamicTreeStructure(workerRank, totalWorkers);

    //std::cout << "[Worker " << workerRank << "] Tree node - Level: " << node.level 
              //<< ", Parent: " << node.parent << ", Children: " << node.children. size() << std::endl;

    int iteration = 0;
    while (true) {
        int k, cols;
        std::vector<double> pivotRow;

        // ROOT WORKER odbiera od mastera
        if (workerRank == 0) {
            MPI_Recv(&k, 1, MPI_INT, 0, 0, parent_comm, MPI_STATUS_IGNORE);
            
            //std::cout << "[Worker 0] Iteration " << iteration << ", received k=" << k << std::endl;
            
            if (k == -1) {
                //std::cout << "[Worker 0] Termination signal received" << std::endl;
                // Broadcast terminate do innych workerów
                for (int w = 1; w < totalWorkers; w++) {
                    MPI_Send(&k, 1, MPI_INT, w, 0, MPI_COMM_WORLD);
                }
                break;
            }
            
            MPI_Recv(&cols, 1, MPI_INT, 0, 0, parent_comm, MPI_STATUS_IGNORE);
            pivotRow.resize(cols);
            MPI_Recv(pivotRow.data(), cols, MPI_DOUBLE, 0, 0, parent_comm, MPI_STATUS_IGNORE);
            
            //std::cout << "[Worker 0] Broadcasting k=" << k << ", cols=" << cols << std::endl;
            
            // Broadcast do wszystkich innych workerów
            for (int w = 1; w < totalWorkers; w++) {
                MPI_Send(&k, 1, MPI_INT, w, 0, MPI_COMM_WORLD);
                MPI_Send(&cols, 1, MPI_INT, w, 0, MPI_COMM_WORLD);
                MPI_Send(pivotRow.data(), cols, MPI_DOUBLE, w, 0, MPI_COMM_WORLD);
            }
        } else {
            // NON-ROOT WORKERS odbierają od worker 0
            MPI_Recv(&k, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (k == -1) {
                //std::cout << "[Worker " << workerRank << "] Termination signal received" << std::endl;
                break;
            }
            
            MPI_Recv(&cols, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            pivotRow.resize(cols);
            MPI_Recv(pivotRow.data(), cols, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            //std::cout << "[Worker " << workerRank << "] Received k=" << k << ", cols=" << cols << std::endl;
        }

        // Każdy worker odbiera swoje wiersze od mastera
        std::vector<std::vector<double>> localRows;
        
        int rowCount;
        MPI_Recv(&rowCount, 1, MPI_INT, 0, 0, parent_comm, MPI_STATUS_IGNORE);
        
        //std::cout << "[Worker " << workerRank << "] Received " << rowCount << " rows" << std::endl;
        
        if (rowCount > 0) {
            for (int i = 0; i < rowCount; i++) {
                std:: vector<double> row(cols);
                MPI_Recv(row.data(), cols, MPI_DOUBLE, 0, 0, parent_comm, MPI_STATUS_IGNORE);
                localRows. push_back(row);
            }
            
            // Eliminacja
            eliminateRows(k, pivotRow, localRows);
            //std::cout << "[Worker " << workerRank << "] Eliminated " << rowCount << " rows" << std:: endl;
        }

        // Gather wyników przez drzewo
        std::vector<std::vector<double>> gatheredRows = localRows;
        
        // Zbierz od dzieci
        for (int child :  node.children) {
            int childCount;
            MPI_Recv(&childCount, 1, MPI_INT, child, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            //std::cout << "[Worker " << workerRank << "] Receiving " << childCount 
                      //<< " rows from child " << child << std:: endl;
            
            for (int i = 0; i < childCount; i++) {
                std::vector<double> row(cols);
                MPI_Recv(row.data(), cols, MPI_DOUBLE, child, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                gatheredRows.push_back(row);
            }
        }
        
        // Wyślij do rodzica (w drzewie workerów) lub do mastera (jeśli root)
        if (workerRank == 0) {
            // Root worker wysyła do mastera
            int totalCount = gatheredRows.size();
            //std::cout << "[Worker 0] Sending " << totalCount << " rows back to master" << std::endl;
            
            MPI_Send(&totalCount, 1, MPI_INT, 0, 0, parent_comm);
            
            for (auto& row : gatheredRows) {
                MPI_Send(row.data(), cols, MPI_DOUBLE, 0, 0, parent_comm);
            }
        } else if (node.parent != -1) {
            // Non-root worker wysyła do rodzica w drzewie
            int totalCount = gatheredRows.size();
            //std::cout << "[Worker " << workerRank << "] Sending " << totalCount 
                      //<< " rows to parent " << node.parent << std::endl;
            
            MPI_Send(&totalCount, 1, MPI_INT, node.parent, 0, MPI_COMM_WORLD);
            
            for (auto& row : gatheredRows) {
                MPI_Send(row.data(), cols, MPI_DOUBLE, node.parent, 0, MPI_COMM_WORLD);
            }
        }
        
        iteration++;
        //std::cout << "[Worker " << workerRank << "] Completed iteration " << iteration << std::endl;
    }

    //std::cout << "[Worker " << workerRank << "] Exiting after " << iteration << " iterations" << std::endl;

    MPI_Comm_disconnect(&parent_comm);
    MPI_Finalize();
    return 0;
}