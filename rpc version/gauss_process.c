#define _GNU_SOURCE // bez tego map anonymous nie ma
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "connection.h"

#include "gauss_process.h"

#define NUM_PROCS 4


matrix matrix_deep_copy(const matrix *src) {
    matrix dest;
    dest.matrix_len = src->matrix_len;

    // alokujemy tablicę row
    dest.matrix_val = malloc(dest.matrix_len * sizeof(row));

    for (u_int i = 0; i < dest.matrix_len; i++) {
        dest.matrix_val[i].row_len = src->matrix_val[i].row_len;

        // alokujemy tablicę double
        dest.matrix_val[i].row_val =
            malloc(dest.matrix_val[i].row_len * sizeof(double));

        // kopiujemy wartości
        for (u_int j = 0; j < dest.matrix_val[i].row_len; j++) {
            dest.matrix_val[i].row_val[j] = src->matrix_val[i].row_val[j];
        }
    }

    return dest;
}

void matrix_free(matrix *m) {
    for (u_int i = 0; i < m->matrix_len; i++) {
        free(m->matrix_val[i].row_val);
    }
    free(m->matrix_val);
}


double **create_shared_matrix(const matrix *m)
{
    int N = m->matrix_len;

    double **shared = mmap(NULL, N * sizeof(double *),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (shared == MAP_FAILED)
    {
        perror("mmap row pointers failed");
        exit(1);
    }

    for (int i = 0; i < N; i++)
    {
        shared[i] = mmap(NULL, m->matrix_val[i].row_len * sizeof(double),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        if (shared[i] == MAP_FAILED)
        {
            perror("mmap row failed");
            exit(1);
        }

        for (int j = 0; j < m->matrix_val[i].row_len; j++)
        {
            shared[i][j] = m->matrix_val[i].row_val[j];
        }
    }

    return shared;
}

void copy_shared_to_matrix(double **shared, matrix *m)
{
    int N = m->matrix_len;
    
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < m->matrix_val[i].row_len; j++)
        {
            m->matrix_val[i].row_val[j] = shared[i][j];
        }
    }
}

void destroy_shared_matrix(double **mat, int N)
{
    for (int i = 0; i < N; i++)
        munmap(mat[i], (N + 1) * sizeof(double));

    munmap(mat, N * sizeof(double *));
}

int forwardElim_p(double **mat, int N)
{
    for (int k = 0; k < N; k++)
    {

        int i_max = k;
        double v_max = fabs(mat[i_max][k]);

        for (int i = k + 1; i < N; i++)
        {
            if (fabs(mat[i][k]) > v_max)
            {
                v_max = fabs(mat[i][k]);
                i_max = i;
            }
        }

        if (fabs(mat[i_max][k]) < 1e-9)
            return k;

        if (i_max != k)
        {
            for (int j = 0; j <= N; j++)
            {
                double tmp = mat[k][j];
                mat[k][j] = mat[i_max][j];
                mat[i_max][j] = tmp;
            }
        }

        int rows_per_proc = (N - (k + 1) + NUM_PROCS - 1) / NUM_PROCS;

        for (int p = 0; p < NUM_PROCS; p++)
        {

            pid_t pid = fork();
            if (pid == 0)
            {
                int start = k + 1 + p * rows_per_proc;
                int end = start + rows_per_proc;
                if (end > N)
                    end = N;

                for (int i = start; i < end; i++)
                {
                    if (i >= N)
                        break;

                    double f = mat[i][k] / mat[k][k];

                    for (int j = k + 1; j <= N; j++)
                        mat[i][j] -= mat[k][j] * f;

                    mat[i][k] = 0.0;
                }
                _exit(0);
            }
        }

        while (wait(NULL) > 0)
            ;
    }

    return -1;
}

void backSub_p(double **mat, int N)
{
    double *x = malloc(N * sizeof(double));

    for (int i = N - 1; i >= 0; i--)
    {
        x[i] = mat[i][N];

        for (int j = i + 1; j < N; j++)
            x[i] -= mat[i][j] * x[j];

        x[i] /= mat[i][i];
    }

    
    //for (int i = 0; i < N; i++)
    //    printf("x%d = %lf\n", i + 1, x[i]);
    

    free(x);
}

void gaussianElimination_p(matrix *m, char *buf, size_t bufsize)
{
    int N = m->matrix_len;
    double **mat = create_shared_matrix(m);

    struct timeval start_wall, end_wall;
    clock_t start_cpu, end_cpu;

    gettimeofday(&start_wall, NULL);
    start_cpu = clock();

    int singular_flag = forwardElim_p(mat, N);

    if (singular_flag != -1)
    {
        printf("Macierz osobliwa, układ może nie mieć jednoznacznego rozwiązania.\n");
        snprintf(buf, bufsize, "Macierz osobliwa, brak jednoznacznego rozwiązania.\n");
        destroy_shared_matrix(mat, N);
        return;
    }

    backSub_p(mat, N);

    gettimeofday(&end_wall, NULL);
    end_cpu = clock();

    double wall_time =
        (end_wall.tv_sec - start_wall.tv_sec) +
        (end_wall.tv_usec - start_wall.tv_usec) / 1e6;

    double cpu_time = (double)(end_cpu - start_cpu) / CLOCKS_PER_SEC;

    printf("z procesami:\n");
    printf("Czas zegarowy (wall): %lf s\n", wall_time);
    printf("Czas CPU: %lf s\n", cpu_time);

    snprintf(buf, bufsize,
             "z procesami:\nCzas zegarowy (wall): %lf s\nCzas CPU: %lf s\n",
             wall_time, cpu_time);

    // Kopiujemy wyniki z pamięci współdzielonej do oryginalnej macierzy
    copy_shared_to_matrix(mat, m);

    destroy_shared_matrix(mat, N);
}