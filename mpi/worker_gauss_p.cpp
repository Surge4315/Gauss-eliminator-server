#include <mpi.h>
#include <iostream>
#include <vector>
#include "gauss.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Pobierz communicator z parent procesem
    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);
    
    if (parent == MPI_COMM_NULL) {
        std::cerr << "Brak parent communicatora!" << std::endl;
        MPI_Finalize();
        return 1;
    }

    // Odbieranie wymiarów macierzy
    int rows, cols;
    MPI_Recv(&rows, 1, MPI_INT, 0, 0, parent, MPI_STATUS_IGNORE);
    MPI_Recv(&cols, 1, MPI_INT, 0, 0, parent, MPI_STATUS_IGNORE);
    
    std::cout << "Worker procesowy otrzymał macierz " << rows << "x" << cols << std::endl;
    
    // Odbieranie całej macierzy
    std::vector<std::vector<double>> matrix(rows, std::vector<double>(cols));
    for(int i = 0; i < rows; i++) {
        MPI_Recv(matrix[i].data(), cols, MPI_DOUBLE, 0, 0, parent, MPI_STATUS_IGNORE);
    }

    matrix = gaussianElimination(matrix);

    // Wysyłanie wyniku z powrotem
    for(int i = 0; i < rows; i++) {
        MPI_Send(matrix[i].data(), cols, MPI_DOUBLE, 0, 0, parent);
    }

    MPI_Finalize();
    return 0;
}