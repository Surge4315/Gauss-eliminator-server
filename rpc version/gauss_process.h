#ifndef GAUSS_PROCESS_H
#define GAUSS_PROCESS_H

#include <stdint.h>
#include <stdlib.h>
#include "connection.h"

matrix matrix_deep_copy(const matrix *src);

void matrix_free(matrix *m);

double **create_shared_matrix(const matrix *m);

void destroy_shared_matrix(double **mat, int N);

int forwardElim_p(double **mat, int N);

void backSub_p(double **mat, int N);

void gaussianElimination_p(matrix *m, char* buf, size_t bufsize);

#endif
