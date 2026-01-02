
#include <iostream>
#include <vector>
#include <unistd.h>
#include "macierz.h"
#include "gauss.h"
#include "gauss_cuda.h"
using namespace std;

void backSub_master(const vector<vector<double>> &mat)
{
    int N = mat. size();
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

    
    char choice;
    vector<vector<double>> testMatrix;
    int test = 0;
    cout << "1. 10\n2. 100\n3. 1000\n4. 10000\n5. test\nq. quit\n";
    while(true)
    {
       
        cin >> choice;
        
        if (choice == 'q') {
            cout << "exiting..." << endl;
            break;
        }
        
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
            test = 1;
            break;
        default:
            cout << "Invalid choice. Please try again.\n";
            continue;
        }
    
        int rows = testMatrix.size();
        int cols = testMatrix[0].size();

        std::vector<std::vector<double>> result(rows);
        result = gaussianElimination(testMatrix);

        if(test) backSub_master(result);

        std::vector<std::vector<double>> result2(rows);
        result2 = gaussianElimination_c(testMatrix);
        
        if(test) backSub_master(result2);
    
        test = 0;
    }
    
    cout << "Program terminated successfully." << endl;
    return 0;
}

