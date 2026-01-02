#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cmath>
#include <vector>
#include <cuda_runtime.h>
#include "gauss_cuda.h"

using namespace std;

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            cerr << "CUDA error in " << __FILE__ << ":" << __LINE__ << ": " \
                 << cudaGetErrorString(err) << endl; \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

// Funkcja pomocnicza do atomicExch dla double (jeśli nie ma natywnej)
__device__ double atomicExchDouble(double* address, double val) {
    unsigned long long int* address_as_ull = (unsigned long long int*)address;
    unsigned long long int old = *address_as_ull, assumed;
    
    do {
        assumed = old;
        old = atomicCAS(address_as_ull, assumed, __double_as_longlong(val));
    } while (assumed != old);
    
    return __longlong_as_double(old);
}

// Funkcja pomocnicza do atomicAdd dla double (dla starszych GPU)
__device__ double atomicAddDouble(double* address, double val) {
    unsigned long long int* address_as_ull = (unsigned long long int*)address;
    unsigned long long int old = *address_as_ull, assumed;
    
    do {
        assumed = old;
        old = atomicCAS(address_as_ull, assumed,
                        __double_as_longlong(val + __longlong_as_double(assumed)));
    } while (assumed != old);
    
    return __longlong_as_double(old);
}

// Kernel do znajdowania maksymalnego elementu w kolumnie (redukcja)
__global__ void findPivotKernel(double* mat, int N, int k, int* i_max, double* v_max) {
    extern __shared__ double shared_data[];
    double* s_vals = shared_data;
    int* s_indices = (int*)&shared_data[blockDim.x];
    
    int tid = threadIdx.x;
    int i = k + 1 + blockIdx.x * blockDim. x + threadIdx.x;
    
    // Inicjalizacja shared memory
    if (i < N) {
        s_vals[tid] = fabs(mat[i * (N + 1) + k]);
        s_indices[tid] = i;
    } else {
        s_vals[tid] = -1.0;
        s_indices[tid] = k;
    }
    __syncthreads();
    
    // Redukcja w bloku
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            if (s_vals[tid + stride] > s_vals[tid]) {
                s_vals[tid] = s_vals[tid + stride];
                s_indices[tid] = s_indices[tid + stride];
            }
        }
        __syncthreads();
    }
    
    // Pierwszy wątek zapisuje wynik bloku (operacja atomowa)
    if (tid == 0 && s_vals[0] > 1e-9) {
        // Porównanie i aktualizacja globalna (wymaga synchronizacji)
        unsigned long long* addr_val = (unsigned long long*)v_max;
        unsigned long long old_val = *addr_val;
        unsigned long long new_val;
        
        do {
            old_val = *addr_val;
            double old_double = __longlong_as_double(old_val);
            if (s_vals[0] > old_double) {
                new_val = __double_as_longlong(s_vals[0]);
            } else {
                break;
            }
        } while (atomicCAS(addr_val, old_val, new_val) != old_val);
        
        // Aktualizacja indeksu
        if (s_vals[0] > __longlong_as_double(old_val)) {
            atomicExch(i_max, s_indices[0]);
        }
    }
}

// Kernel do zamiany wierszy
__global__ void swapRowsKernel(double* mat, int N, int row1, int row2) {
    int j = blockIdx.x * blockDim. x + threadIdx.x;
    
    if (j <= N) {
        double temp = mat[row1 * (N + 1) + j];
        mat[row1 * (N + 1) + j] = mat[row2 * (N + 1) + j];
        mat[row2 * (N + 1) + j] = temp;
    }
}

// Kernel do obliczania współczynników eliminacji
__global__ void computeFactorsKernel(double* mat, double* factors, int N, int k) {
    int i = k + 1 + blockIdx. x * blockDim.x + threadIdx.x;
    
    if (i < N) {
        double pivot = mat[k * (N + 1) + k];
        if (fabs(pivot) > 1e-9) {
            factors[i] = mat[i * (N + 1) + k] / pivot;
        } else {
            factors[i] = 0.0;
        }
    }
}

// Zoptymalizowany kernel eliminacji z wykorzystaniem współczynników
// Wersja bez operacji atomowych - bezpieczna bo każdy wątek pisze do innej komórki
__global__ void forwardEliminationOptKernel(double* mat, double* factors, int N, int k) {
    int i = k + 1 + blockIdx. y * blockDim.y + threadIdx.y;
    int j = k + 1 + blockIdx. x * blockDim.x + threadIdx.x;
    
    if (i < N && j <= N) {
        double f = factors[i];
        // Każdy wątek odpowiada za unikalne (i,j), więc nie potrzebujemy atomics
        mat[i * (N + 1) + j] -= mat[k * (N + 1) + j] * f;
        
        // Wyzeruj element pod przekątną (tylko dla pierwszej kolumny po k)
        if (j == k + 1) {
            mat[i * (N + 1) + k] = 0.0;
        }
    }
}

// Alternatywny kernel z użyciem shared memory (bardziej optymalny)
__global__ void forwardEliminationSharedKernel(double* mat, double* factors, int N, int k) {
    __shared__ double s_row[256]; // Shared memory dla wiersza k
    
    int i = k + 1 + blockIdx.y * blockDim.y + threadIdx. y;
    int j = k + 1 + blockIdx. x * blockDim.x + threadIdx.x;
    int tid_x = threadIdx.x;
    
    // Załaduj wiersz k do shared memory
    if (threadIdx.y == 0 && j <= N) {
        s_row[tid_x] = mat[k * (N + 1) + j];
    }
    __syncthreads();
    
    if (i < N && j <= N) {
        double f = factors[i];
        // Użyj wartości z shared memory
        mat[i * (N + 1) + j] -= s_row[tid_x] * f;
        
        if (j == k + 1) {
            mat[i * (N + 1) + k] = 0.0;
        }
    }
}

// Funkcja do eliminacji Gaussa na GPU
int forwardElimGPU(double* d_mat, int N, cudaStream_t* streams, int num_streams) {
    int* d_i_max;
    double* d_v_max;
    double* d_factors;
    
    CUDA_CHECK(cudaMalloc(&d_i_max, sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_v_max, sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_factors, N * sizeof(double)));
    
    int h_i_max;
    double h_v_max;
    
    for (int k = 0; k < N; k++) {
        // Inicjalizacja wartości maksymalnej
        h_i_max = k;
        double pivot_val;
        CUDA_CHECK(cudaMemcpy(&pivot_val, &d_mat[k * (N + 1) + k], sizeof(double), cudaMemcpyDeviceToHost));
        h_v_max = fabs(pivot_val);
        
        CUDA_CHECK(cudaMemcpy(d_i_max, &h_i_max, sizeof(int), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(d_v_max, &h_v_max, sizeof(double), cudaMemcpyHostToDevice));
        
        // Znajdź pivot (redukcja) - wykorzystanie strumienia 0
        int remaining = N - k - 1;
        if (remaining > 0) {
            int threadsPerBlock = 256;
            int numBlocks = (remaining + threadsPerBlock - 1) / threadsPerBlock;
            size_t shared_size = threadsPerBlock * (sizeof(double) + sizeof(int));
            
            findPivotKernel<<<numBlocks, threadsPerBlock, shared_size, streams[0]>>>(
                d_mat, N, k, d_i_max, d_v_max
            );
            CUDA_CHECK(cudaStreamSynchronize(streams[0]));
        }
        
        // Sprawdź czy macierz nie jest osobliwa
        CUDA_CHECK(cudaMemcpy(&h_v_max, d_v_max, sizeof(double), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&h_i_max, d_i_max, sizeof(int), cudaMemcpyDeviceToHost));
        
        if (h_v_max < 1e-9) {
            CUDA_CHECK(cudaFree(d_i_max));
            CUDA_CHECK(cudaFree(d_v_max));
            CUDA_CHECK(cudaFree(d_factors));
            return k;
        }
        
        // Zamień wiersze jeśli potrzeba - strumień 1
        if (h_i_max != k) {
            int threadsPerBlock = 256;
            int numBlocks = (N + 1 + threadsPerBlock - 1) / threadsPerBlock;
            swapRowsKernel<<<numBlocks, threadsPerBlock, 0, streams[1 % num_streams]>>>(
                d_mat, N, k, h_i_max
            );
            CUDA_CHECK(cudaStreamSynchronize(streams[1 % num_streams]));
        }
        
        // Oblicz współczynniki - strumień 2
        int threadsPerBlock1D = 256;
        int numBlocks1D = (N - k - 1 + threadsPerBlock1D - 1) / threadsPerBlock1D;
        if (numBlocks1D > 0) {
            computeFactorsKernel<<<numBlocks1D, threadsPerBlock1D, 0, streams[2 % num_streams]>>>(
                d_mat, d_factors, N, k
            );
            CUDA_CHECK(cudaStreamSynchronize(streams[2 % num_streams]));
        }
        
        // Wykonaj eliminację - strumienie równoległe dla różnych części macierzy
        dim3 threadsPerBlock2D(16, 16);
        dim3 numBlocks2D(
            (N - k + threadsPerBlock2D.x - 1) / threadsPerBlock2D.x,
            (N - k + threadsPerBlock2D.y - 1) / threadsPerBlock2D. y
        );
        
        if (numBlocks2D.x > 0 && numBlocks2D.y > 0) {
            // Użyj wersji z shared memory jeśli możliwe
            if (N - k <= 256) {
                forwardEliminationSharedKernel<<<numBlocks2D, threadsPerBlock2D, 0, streams[3 % num_streams]>>>(
                    d_mat, d_factors, N, k
                );
            } else {
                forwardEliminationOptKernel<<<numBlocks2D, threadsPerBlock2D, 0, streams[3 % num_streams]>>>(
                    d_mat, d_factors, N, k
                );
            }
            CUDA_CHECK(cudaStreamSynchronize(streams[3 % num_streams]));
        }
    }
    
    CUDA_CHECK(cudaFree(d_i_max));
    CUDA_CHECK(cudaFree(d_v_max));
    CUDA_CHECK(cudaFree(d_factors));
    
    return -1;
}

vector<vector<double>> gaussianElimination_c(vector<vector<double>> mat) {
    int N = mat. size();
    
    // Alokacja pamięci na GPU
    double* d_mat;
    size_t mat_size = N * (N + 1) * sizeof(double);
    CUDA_CHECK(cudaMalloc(&d_mat, mat_size));
    
    // Konwersja do formatu liniowego
    vector<double> linear_mat(N * (N + 1));
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= N; j++) {
            linear_mat[i * (N + 1) + j] = mat[i][j];
        }
    }
    
    // Utworzenie strumieni CUDA
    const int num_streams = 4;
    cudaStream_t streams[num_streams];
    for (int i = 0; i < num_streams; i++) {
        CUDA_CHECK(cudaStreamCreate(&streams[i]));
    }
    
    // Kopiowanie danych na GPU
    CUDA_CHECK(cudaMemcpy(d_mat, linear_mat.data(), mat_size, cudaMemcpyHostToDevice));
    
    auto start_wall = chrono::high_resolution_clock:: now();
    clock_t start_cpu = clock();
    
    // Wykonanie eliminacji Gaussa na GPU
    int singular_flag = forwardElimGPU(d_mat, N, streams, num_streams);
    
    // Synchronizacja wszystkich strumieni
    for (int i = 0; i < num_streams; i++) {
        CUDA_CHECK(cudaStreamSynchronize(streams[i]));
    }
    
    auto end_wall = chrono::high_resolution_clock:: now();
    clock_t end_cpu = clock();
    
    chrono::duration<double> elapsed_wall = end_wall - start_wall;
    double elapsed_cpu = double(end_cpu - start_cpu) / CLOCKS_PER_SEC;
    
    cout << "CUDA (równolegle z atomics, redukcją i strumieniami):" << endl;
    cout << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    cout << "Czas CPU:  " << elapsed_cpu << " s\n";
    
    // Kopiowanie wyników z powrotem
    CUDA_CHECK(cudaMemcpy(linear_mat.data(), d_mat, mat_size, cudaMemcpyDeviceToHost));
    
    // Konwersja z powrotem do formatu 2D
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= N; j++) {
            mat[i][j] = linear_mat[i * (N + 1) + j];
        }
    }
    
    if (singular_flag != -1) {
        cout << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
    }
    
    // Zwolnienie zasobów
    for (int i = 0; i < num_streams; i++) {
        CUDA_CHECK(cudaStreamDestroy(streams[i]));
    }
    CUDA_CHECK(cudaFree(d_mat));
    
    return mat;
}
