#define CL_TARGET_OPENCL_VERSION 300
#include <iostream>
#include <vector>
#include <chrono>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>
#include <CL/cl.h>
#include "gauss.h"

using namespace std;

// Funkcja do wczytywania kodu kernela z pliku
string loadKernelSource(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Nie można otworzyć pliku kernela: " << filename << endl;
        exit(1);
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void checkError(cl_int err, const char* operation) {
    if (err != CL_SUCCESS) {
        cerr << "Error during operation '" << operation << "': " << err << endl;
        exit(1);
    }
}

void swap_row_opencl(vector<vector<double>> &mat, int i, int j) { 
    swap(mat[i], mat[j]); 
}

int forwardElim_opencl(vector<vector<double>> &mat, cl_context context, 
                       cl_command_queue queue, cl_program program)
{
    int N = mat.size();
    int cols = N + 1;
    
    // Konwersja macierzy 2D do 1D dla OpenCL (double -> float)
    vector<float> mat_flat(N * cols);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < cols; j++) {
            mat_flat[i * cols + j] = (float)mat[i][j];
        }
    }
    
    // Utworzenie bufora OpenCL
    cl_int err;
    cl_mem d_mat = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                  sizeof(float) * N * cols, mat_flat.data(), &err);
    checkError(err, "clCreateBuffer");
    
    // Utworzenie kernela
    cl_kernel kernel = clCreateKernel(program, "gaussElimination", &err);
    checkError(err, "clCreateKernel");
    
    // Główna pętla eliminacji
    for (int k = 0; k < N; k++) {
        // Pobranie danych z GPU dla pivotingu
        err = clEnqueueReadBuffer(queue, d_mat, CL_TRUE, 0, 
                                 sizeof(float) * N * cols, mat_flat.data(), 0, NULL, NULL);
        checkError(err, "clEnqueueReadBuffer");
        
        // Pivoting na CPU (częściowe pivotowanie)
        int i_max = k;
        double v_max = fabs(mat_flat[i_max * cols + k]);
        for (int i = k + 1; i < N; i++) {
            if (fabs(mat_flat[i * cols + k]) > v_max) {
                v_max = fabs(mat_flat[i * cols + k]);
                i_max = i;
            }
        }
        
        if (fabs(mat_flat[i_max * cols + k]) < 1e-9) {
            clReleaseMemObject(d_mat);
            clReleaseKernel(kernel);
            return k;
        }
        
        // Zamiana wierszy jeśli potrzeba
        if (i_max != k) {
            for (int j = 0; j < cols; j++) {
                swap(mat_flat[k * cols + j], mat_flat[i_max * cols + j]);
            }
            // Aktualizacja bufora po zamianie
            err = clEnqueueWriteBuffer(queue, d_mat, CL_TRUE, 0,
                                      sizeof(float) * N * cols, mat_flat.data(), 0, NULL, NULL);
            checkError(err, "clEnqueueWriteBuffer after swap");
        }
        
        // Ustawienie argumentów kernela
        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_mat);
        err |= clSetKernelArg(kernel, 1, sizeof(int), &N);
        err |= clSetKernelArg(kernel, 2, sizeof(int), &k);
        checkError(err, "clSetKernelArg");
        
        // Uruchomienie kernela
        size_t globalSize = N - k - 1;
        if (globalSize > 0) {
            err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, NULL, 0, NULL, NULL);
            checkError(err, "clEnqueueNDRangeKernel");
        }
    }
    
    // Oczekiwanie na zakończenie
    clFinish(queue);
    
    // Pobranie wyniku
    err = clEnqueueReadBuffer(queue, d_mat, CL_TRUE, 0, 
                             sizeof(float) * N * cols, mat_flat.data(), 0, NULL, NULL);
    checkError(err, "clEnqueueReadBuffer final");
    
    // Konwersja z powrotem do 2D
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < cols; j++) {
            mat[i][j] = mat_flat[i * cols + j];
        }
    }
    
    // Zwolnienie zasobów
    clReleaseMemObject(d_mat);
    clReleaseKernel(kernel);
    
    return -1;
}

vector<vector<double>> gaussianElimination_opencl(vector<vector<double>> mat)
{
    auto start_wall = chrono::high_resolution_clock::now();
    clock_t start_cpu = clock();
    
    // Inicjalizacja OpenCL
    cl_int err;
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    
    // Wybór platformy
    err = clGetPlatformIDs(1, &platform, NULL);
    checkError(err, "clGetPlatformIDs");
    
    // Wybór urządzenia GPU (lub CPU jako fallback)
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
        cout << "GPU nie znalezione, używam CPU" << endl;
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
        checkError(err, "clGetDeviceIDs CPU");
    }
    
    // Utworzenie kontekstu
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    checkError(err, "clCreateContext");
    
    // Utworzenie kolejki poleceń
    queue = clCreateCommandQueueWithProperties(context, device, NULL, &err);
    checkError(err, "clCreateCommandQueueWithProperties");
    
    // Wczytanie i kompilacja programu
    string kernelSourceStr = loadKernelSource("gauss_kernel.cl");
    const char* kernelSource = kernelSourceStr.c_str();
    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    checkError(err, "clCreateProgramWithSource");
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = new char[log_size];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        cerr << "Build error:\n" << log << endl;
        delete[] log;
        exit(1);
    }
    
    // Wykonanie eliminacji
    int singular_flag = forwardElim_opencl(mat, context, queue, program);
    
    if (singular_flag != -1) {
        cout << "Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n";
    }
    
    // Zwolnienie zasobów OpenCL
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    auto end_wall = chrono::high_resolution_clock::now();
    clock_t end_cpu = clock();
    
    chrono::duration<double> elapsed_wall = end_wall - start_wall;
    double elapsed_cpu = double(end_cpu - start_cpu) / CLOCKS_PER_SEC;
    
    cout << "\nOpenCL:" << endl;
    cout << "Czas zegarowy (wall): " << elapsed_wall.count() << " s\n";
    cout << "Czas CPU: " << elapsed_cpu << " s\n";
    
    return mat;
}
