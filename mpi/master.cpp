#include <mpi.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "macierz.h"
using namespace std;

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

    cout << "\nRozwiązanie układu:\n";
    for (int i = 0; i < N; i++)
        cout << "x" << i + 1 << " = " << x[i] << endl;
    cout << endl;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        int num_workers = 1; // this is just to send data to worker that does all the process work so 1 is more than enough
        MPI_Info info;
        MPI_Info_create(&info);

        cout << "1. 10\n2. 100\n3. 1000\n4. 10000\n5. test\nq. quit\n";
        char choice;
        vector<vector<double>> testMatrix;
        int test=0;
        while(true)
        {
            cin >> choice;
            switch (choice)
            {
            case '1':
                testMatrix = createAugmentedMatrix(generateDiagonallyDominantMatrix(10), generateVectorB(10));
                break;
            case '2':
                testMatrix = createAugmentedMatrix(generateDiagonallyDominantMatrix(100), generateVectorB(100));
                break;
            case '3':
                testMatrix = createAugmentedMatrix(generateDiagonallyDominantMatrix(1000), generateVectorB(1000));
                break;
            case '4':
                testMatrix = createAugmentedMatrix(generateDiagonallyDominantMatrix(10000), generateVectorB(10000));
                break;
            case '5':
                testMatrix = {
                    {1, -2, 3, -7},
                    {3, 1, 4, 5},
                    {2, 5, 1, 18}};
                test=1;
                break;
            case 'q':
                MPI_Finalize();
                return 0;
            default:
                cout << "Invalid choice. Please try again.\n";
                continue;
            }
        
        MPI_Comm workers_comm;
        MPI_Comm_spawn("./worker_gauss", MPI_ARGV_NULL, num_workers,
                       info, 0, MPI_COMM_WORLD, &workers_comm, MPI_ERRCODES_IGNORE);

        // Wysyłanie całej macierzy do workera
        int rows = testMatrix.size();
        int cols = testMatrix[0].size();

        // Wysłanie wymiarów macierzy
        MPI_Send(&rows, 1, MPI_INT, 0, 0, workers_comm);
        MPI_Send(&cols, 1, MPI_INT, 0, 0, workers_comm);

        // Wysłanie każdego wiersza
        for (int i = 0; i < rows; i++)
        {
            MPI_Send(testMatrix[i].data(), cols, MPI_DOUBLE, 0, 0, workers_comm);
        }
        // Odbieranie wyniku od workera
        std::vector<std::vector<double>> result(rows);
        for (int i = 0; i < rows; i++)
        {
            result[i].resize(cols);
            MPI_Recv(result[i].data(), cols, MPI_DOUBLE, 0, 0, workers_comm, MPI_STATUS_IGNORE);
        }
        usleep(1000); // for synchronization
        // Wyświetlanie wyniku
        if(test) backSub(result);

        usleep(1000); // for synchronization

        MPI_Comm workers_comm2;
        MPI_Comm_spawn("./worker_gauss_p", MPI_ARGV_NULL, num_workers,
                       info, 0, MPI_COMM_WORLD, &workers_comm2, MPI_ERRCODES_IGNORE);

        // Wysłanie wymiarów macierzy
        MPI_Send(&rows, 1, MPI_INT, 0, 0, workers_comm2);
        MPI_Send(&cols, 1, MPI_INT, 0, 0, workers_comm2);

        // Wysłanie każdego wiersza
        for (int i = 0; i < rows; i++)
        {
            MPI_Send(testMatrix[i].data(), cols, MPI_DOUBLE, 0, 0, workers_comm2);
        }

        // Odbieranie wyniku od drugiego workera (opcjonalnie)
        std::vector<std::vector<double>> result2(rows);
        for (int i = 0; i < rows; i++)
        {
            result2[i].resize(cols);
            MPI_Recv(result2[i].data(), cols, MPI_DOUBLE, 0, 0, workers_comm2, MPI_STATUS_IGNORE);
        }
        usleep(1000); // for synchronization
        // Wyświetlanie wyniku drugiego workera
        if(test){
            backSub(result2);
            test=0;
        }
    }

        MPI_Info_free(&info);
    
}

MPI_Finalize();
return 0;
}