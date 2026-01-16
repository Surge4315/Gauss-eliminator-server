__kernel void gaussElimination(
    __global float* mat,
    const int N,
    const int k)
{
    int i = get_global_id(0) + k + 1;
    
    if (i < N) {
        int cols = N + 1;
        float f = mat[i * cols + k] / mat[k * cols + k];
        
        for (int j = k + 1; j <= N; j++) {
            mat[i * cols + j] -= mat[k * cols + j] * f;
        }
        mat[i * cols + k] = 0.0f;
    }
}

__kernel void findPivot(
    __global float* mat,
    __global int* max_idx,
    __global float* max_val,
    const int N,
    const int k)
{
    int i = get_global_id(0) + k;
    
    if (i < N) {
        int cols = N + 1;
        float val = fabs(mat[i * cols + k]);
        
        // Simple atomic-like approach: each thread writes if it has larger value
        if (val > *max_val) {
            *max_val = val;
            *max_idx = i;
        }
    }
}
