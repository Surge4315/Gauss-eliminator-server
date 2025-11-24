#include <cstring>
#include <iostream>
#include <unistd.h>
#include <string>
#include <termios.h>
#include "macierz.h"
#include "macierz_struct.h"

#include "connection.h"

using namespace std;

char getch()
{
    // evil function to capture single key press
    char buf = 0;
    termios old = {0};
    if (tcgetattr(STDIN_FILENO, &old) < 0) // saves old terminal config, if lower than 0 you get error
        perror("tcgetattr()");
    termios newt = old;
    newt.c_lflag &= ~(ICANON | ECHO);                // turn off canonical mode & echo by inverting boolean bits using tilde
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0) // creates new terminal config
        perror("tcsetattr()");
    if (read(STDIN_FILENO, &buf, 1) < 0) // read one byte
        perror("read()");
    tcsetattr(STDIN_FILENO, TCSANOW, &old); // restore settings
    return buf;
}

matrix convert(const std::vector<std::vector<double>> &v)
{
    matrix m;
    m.matrix_len = v.size();
    m.matrix_val = (row *)malloc(sizeof(row) * m.matrix_len);

    for (size_t i = 0; i < v.size(); i++)
    {
        m.matrix_val[i].row_len = v[i].size();
        m.matrix_val[i].row_val = (double *)malloc(sizeof(double) * v[i].size());
        for (size_t j = 0; j < v[i].size(); j++)
        {
            m.matrix_val[i].row_val[j] = v[i][j];
        }
    }
    return m;
}

int main()
{
    CLIENT *clnt;
    char **result;
    char **resultM;
    matrix *resultMatrix;
    matrix *resultMatrixP;
    matrix m;

    clnt = clnt_create("localhost", CONNECTION_PROG, CONNECTION_VERS, "tcp");
    if (clnt == NULL)
    {
        clnt_pcreateerror("localhost");
        return 1;
    }

    char option;
    // --- wysyłanie chara ---
    cout << "1. 10\n2. 100\n3. 1000\n4. 10 000\n5. test obliczen\nq. quit\n";

    while (1)
    {
        option = getch();
        vector<vector<double>> augmented;
        int size = 0;

        std::vector<std::vector<double>> testMatrix = {
                {1, -2, 3, -7},
                {3, 1, 4, 5},
                {2, 5, 1, 18}};

        switch (option)
        {
        case '1':
            size = 10;
            break;
        case '2':
            size = 100;
            break;
        case '3':
            size = 1000;
            break;
        case '4':
            size = 10000;
            cout << "poczekasz troche..." << endl;
            break;
        case '5':
                
            m = convert(testMatrix);
            cout << "Wyniki dla testowej macierzy sekwencyjnie:" << endl;
            resultMatrix = gauss_matrix_print_1(&m, clnt);
            print_gauss_solutions(resultMatrix);
            free(resultMatrix->matrix_val);

            cout<< "Wyniki dla testowej macierzy równolegle:" << endl;
            resultMatrixP = gauss_matrix_print_process_1(&m, clnt);
            print_gauss_solutions(resultMatrixP);
            free(resultMatrixP->matrix_val);
            break;
        case 'q':
        case 'Q':
            return 0;
        default:
            continue;
        }

        if (size != 0)
        {
            cout << size << endl;
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(size), generateVectorB(size));
            m = convert(augmented);
            resultM = gauss_matrix_1(&m, clnt);
            cout << *resultM << endl;
            if (resultM == nullptr)
            {
                clnt_perror(clnt, "call failed");
                return 1;
            }
            size = 0;
        }
    }
    return 0;
}
