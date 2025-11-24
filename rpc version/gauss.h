#ifndef GAUSS_H
#define GAUSS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "connection.h"

void swap_row(matrix *mat, int i, int j);

int forwardElim(matrix *mat);

void backSub(matrix *mat);

void gaussianElimination(matrix *mat, char* buf, size_t bufsize);

#endif