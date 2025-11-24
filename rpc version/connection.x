/* connection.x */

typedef double row<>;       /* tablica double o zmiennej długości */
typedef row matrix<>;       /* tablica wierszy o zmiennej długości */



program CONNECTION_PROG {
    version CONNECTION_VERS {
        string echo_char(char) = 1;
        string echo_string(string) = 2;
        string gauss_matrix(matrix) = 3;
        matrix gauss_matrix_print(matrix) = 4;
        matrix gauss_matrix_print_process(matrix) = 5;

    } = 1;
} = 0x20000002;
