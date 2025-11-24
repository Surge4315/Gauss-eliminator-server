#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "connection.h"

void swap_row(matrix *mat, int i, int j)
{
    row temp = mat->matrix_val[i];
    mat->matrix_val[i] = mat->matrix_val[j];
    mat->matrix_val[j] = temp;
}

int forwardElim(matrix *mat)
{
    int N = mat->matrix_len;

    for (int k = 0; k < N; k++)
    {

        /* szukanie wiersza o największym elemencie w kolumnie k */
        int i_max = k;
        double v_max = fabs(mat->matrix_val[i_max].row_val[k]);

        for (int i = k + 1; i < N; i++)
        {
            double val = fabs(mat->matrix_val[i].row_val[k]);
            if (val > v_max)
            {
                v_max = val;
                i_max = i;
            }
        }

        /* osobliwa macierz */
        if (fabs(mat->matrix_val[i_max].row_val[k]) < 1e-9)
            return k;

        /* zamiana wierszy */
        if (i_max != k)
            swap_row(mat, k, i_max);

        /* eliminacja */
        for (int i = k + 1; i < N; i++)
        {
            double f = mat->matrix_val[i].row_val[k] / mat->matrix_val[k].row_val[k];

            for (int j = k + 1; j <= N; j++)
            {
                mat->matrix_val[i].row_val[j] -=
                    mat->matrix_val[k].row_val[j] * f;
            }
            mat->matrix_val[i].row_val[k] = 0.0;
        }
    }
    return -1;
}

void backSub(matrix *mat)
{
    int N = mat->matrix_len;
    double *x = malloc(N * sizeof(double));

    for (int i = N - 1; i >= 0; i--)
    {
        x[i] = mat->matrix_val[i].row_val[N];
        for (int j = i + 1; j < N; j++)
        {
            x[i] -= mat->matrix_val[i].row_val[j] * x[j];
        }
        x[i] /= mat->matrix_val[i].row_val[i];
    }

    
    //for (int i = 0; i < N; i++)
    //    printf("x%d = %lf\n", i+1, x[i]);
    

    free(x);
}

void gaussianElimination(matrix *mat, char *buf, size_t bufsize)
{
    struct timeval start_wall, end_wall;
    clock_t start_cpu, end_cpu;

    gettimeofday(&start_wall, NULL);
    start_cpu = clock();

    int singular_flag = forwardElim(mat);
    if (singular_flag != -1)
    {
        printf("Macierz osobliwa, brak jednoznacznego rozwiązania.\n");
        snprintf(buf, bufsize, "Macierz osobliwa, brak jednoznacznego rozwiązania.\n");
        return;
    }

    backSub(mat);

    gettimeofday(&end_wall, NULL);
    end_cpu = clock();

    double wall_time =
        (end_wall.tv_sec - start_wall.tv_sec) +
        (end_wall.tv_usec - start_wall.tv_usec) / 1e6;

    double cpu_time = (double)(end_cpu - start_cpu) / CLOCKS_PER_SEC;

    printf("szeregowo:\n");
    printf("Czas zegarowy (wall): %lf s\n", wall_time);
    printf("Czas CPU: %lf s\n", cpu_time);

    snprintf(buf, bufsize,
             "szeregowo:\nCzas zegarowy (wall): %lf s\nCzas CPU: %lf s\n",
             wall_time, cpu_time);
}
