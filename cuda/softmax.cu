#include <iostream>
#include <cuda_runtime.h>
#include <cmath>
#include <cstdlib>

#define CUDA_CHECK(call) \
do { \
    cudaError_t err = (call); \
    if (err != cudaSuccess) { \
        std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl; \
        std::exit(EXIT_FAILURE); \
    } \
} while (0)

__global__ void softmax(const float* input, float* output, int N)
{
    int tid = threadIdx.x;
    int row = blockIdx.x;

    int elements_per_thread = N / blockDim.x;

    __shared__ float max_values[256];
    __shared__ float sum_values[256];

    const float* row_input = input + row * N;
    float* row_output = output + row * N;

    float local_max = -INFINITY;

    for (int i = 0; i < elements_per_thread; i++) {
        int idx = tid * elements_per_thread + i;
        local_max = fmaxf(local_max, row_input[idx]);
    }

    max_values[tid] = local_max;

    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            max_values[tid] = fmaxf(
                max_values[tid],
                max_values[tid + stride]
            );
        }
        __syncthreads();
    }

    float global_max = max_values[0];

    float local_sum = 0.0f;

    for (int i = 0; i < elements_per_thread; i++) {
        int idx = tid * elements_per_thread + i;
        local_sum += expf(row_input[idx] - global_max);
    }

    sum_values[tid] = local_sum;

    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            sum_values[tid] += sum_values[tid + stride];
        }
        __syncthreads();
    }

    float global_sum = sum_values[0];

    for (int i = 0; i < elements_per_thread; i++) {
        int idx = tid * elements_per_thread + i;
        row_output[idx] =
            expf(row_input[idx] - global_max) / global_sum;
    }
}

int main()
{
    int N = 4096;
    int threads = 256;
    int blocks = N;
    int iterations = 20;

    size_t bytes = (size_t)N * N * sizeof(float);

    float* h_input = new float[(size_t)N * N];
    float* h_output = new float[(size_t)N * N];

    for (size_t i = 0; i < (size_t)N * N; i++) {
        h_input[i] = static_cast<float>(i) / N + 5.0f * 10.5f;
    }

    float* d_input;
    float* d_output;

    CUDA_CHECK(cudaMalloc(&d_input, bytes));
    CUDA_CHECK(cudaMalloc(&d_output, bytes));

    CUDA_CHECK(cudaMemcpy(
        d_input,
        h_input,
        bytes,
        cudaMemcpyHostToDevice
    ));

    softmax<<<blocks, threads>>>(d_input, d_output, N);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    cudaEvent_t start, stop;

    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));

    for (int i = 0; i < iterations; i++) {
        softmax<<<blocks, threads>>>(d_input, d_output, N);
    }

    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));

    float total_ms = 0.0f;

    CUDA_CHECK(cudaEventElapsedTime(&total_ms, start, stop));

    float avg_ms = total_ms / iterations;

    CUDA_CHECK(cudaMemcpy(
        h_output,
        d_output,
        bytes,
        cudaMemcpyDeviceToHost
    ));

    double row_sum = 0.0;

    for (int i = 0; i < N; i++) {
        row_sum += h_output[i];
    }

    double flops = 6.0 * N * N;
    double gflops = flops / (avg_ms * 1e6);

    std::cout << "N       : " << N << "\n";
    std::cout << "Threads : " << threads << "\n";
    std::cout << "Blocks  : " << blocks << "\n";
    std::cout << "\n";

    std::cout << "===== BENCHMARK =====\n";
    std::cout << "Iterations       : " << iterations << "\n";
    std::cout << "Total GPU time   : " << total_ms << " ms\n";
    std::cout << "Average GPU time : " << avg_ms << " ms\n";
    std::cout << "Approx FLOPs     : " << flops << "\n";
    std::cout << "Approx GFLOP/s   : " << gflops << "\n";

    std::cout << "\n===== CORRECTNESS =====\n";
    std::cout << "output[0]     : " << h_output[0] << "\n";
    std::cout << "output[N-1]   : " << h_output[N - 1] << "\n";
    std::cout << "row 0 sum     : " << row_sum << "\n";

    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(stop));
    CUDA_CHECK(cudaFree(d_input));
    CUDA_CHECK(cudaFree(d_output));

    delete[] h_input;
    delete[] h_output;

    return 0;
}
