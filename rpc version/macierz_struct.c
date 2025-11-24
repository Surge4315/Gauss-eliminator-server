
#include <stdio.h>
#include "connection.h"


void print_matrix(matrix m) {
    for (u_int i = 0; i < m.matrix_len; i++) {
        row r = m.matrix_val[i];
        for (u_int j = 0; j < r.row_len; j++) {
            printf("%lf ", r.row_val[j]);
        }
        printf("\n");
    }
}

void print_gauss_solutions(matrix *mat) {
    int N = mat->matrix_len;
    double *x = (double*) malloc(N * sizeof(double));

    // Odwrotne podstawianie
    for (int i = N - 1; i >= 0; i--) {
        x[i] = mat->matrix_val[i].row_val[N]; // ostatnia kolumna = wyniki
        for (int j = i + 1; j < N; j++) {
            x[i] -= mat->matrix_val[i].row_val[j] * x[j];
        }
        x[i] /= mat->matrix_val[i].row_val[i];
    }

    // Wypisanie wyników
    for (int i = 0; i < N; i++) {
        printf("x%d = %lf\n", i + 1, x[i]);
    }

    free(x);
}
