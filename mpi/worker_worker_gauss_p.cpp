#include <mpi.h>
#include <iostream>
#include <vector>

void eliminateRows(int k,
                   const std::vector<double>& pivotRow,
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

    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);

    if (parent == MPI_COMM_NULL) {
        std::cerr << "No parent communicator!" << std::endl;
        MPI_Finalize();
        return 1;
    }

    while (true) {
        int k;
        int n;
        int rowCount;

        // 1. receive pivot index k (-1 means terminate)
        // Use MPI_ANY_SOURCE (or 0) as the source rank in parent group
        MPI_Recv(&k, 1, MPI_INT, 0, 0, parent, MPI_STATUS_IGNORE);
        
        if (k == -1) {
            break;
        }

        // 2. receive row length n
        MPI_Recv(&n, 1, MPI_INT, 0, 0, parent, MPI_STATUS_IGNORE);

        // 3. receive pivot row
        std::vector<double> pivotRow(n);
        MPI_Recv(pivotRow.data(), n, MPI_DOUBLE, 0, 0, parent, MPI_STATUS_IGNORE);

        // 4. receive number of rows
        MPI_Recv(&rowCount, 1, MPI_INT, 0, 0, parent, MPI_STATUS_IGNORE);

        // 5. receive rows
        std::vector<std::vector<double>> rows(rowCount, std::vector<double>(n));

        for (int i = 0; i < rowCount; i++) {
            MPI_Recv(rows[i].data(), n, MPI_DOUBLE, 0, 0, parent, MPI_STATUS_IGNORE);
        }

        // 6. elimination
        eliminateRows(k, pivotRow, rows);

        // 7. send processed rows back
        for (int i = 0; i < rowCount; i++) {
            MPI_Send(rows[i].data(), n, MPI_DOUBLE, 0, 0, parent);
        }
    }

    MPI_Comm_disconnect(&parent);
    MPI_Finalize();
    return 0;
}