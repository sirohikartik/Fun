#include <iostream>
#include <chrono>
#include <cmath>
#include <cuda_runtime.h>

#define TILE_SIZE 16
#define THREAD_TILE 2

__global__ void matrix_multiplication_kernel(
    const float* A,
    const float* B,
    float* C,
    int M,
    int K,
    int N
) {
    __shared__ float tileA[TILE_SIZE][TILE_SIZE];
    __shared__ float tileB[TILE_SIZE][TILE_SIZE];

    int tx = threadIdx.x;
    int ty = threadIdx.y;
    int row =
        blockIdx.y * TILE_SIZE +
        ty * THREAD_TILE;

    int col =
        blockIdx.x * TILE_SIZE +
        tx * THREAD_TILE;



    float c00 = 0.0f;
    float c01 = 0.0f;
    float c10 = 0.0f;
    float c11 = 0.0f;


    // --------------------------------------------------------
    // Flatten the 8x8 thread block
    //
    // 8 x 8 = 64 threads
    //
    // Each thread loads one float4 = 4 floats
    //
    // 64 x 4 = 256 floats
    //
    // 16 x 16 = 256 floats
    //
    // Therefore every element of the tile is loaded exactly once.
    // --------------------------------------------------------

    int tid =
        ty * blockDim.x +
        tx;

    int loadRow =
        tid / 4;

    int loadCol =
        (tid % 4) * 4;


    int numTiles =
        (K + TILE_SIZE - 1) / TILE_SIZE;


    for (int t = 0; t < numTiles; t++) {


    
        int globalARow =
            blockIdx.y * TILE_SIZE +
            loadRow;

        int globalACol =
            t * TILE_SIZE +
            loadCol;


        if (
            globalARow < M &&
            globalACol + 3 < K
        ) {

            // ------------------------------------------------
            // float4 = 4 floats = 16 bytes
            //
            // loadCol is always:
            //
            // 0, 4, 8, 12
            //
            // so the pointer is 16-byte aligned for our
            // 4096-wide matrices.
            // ------------------------------------------------

            const float4* ptr =
                reinterpret_cast<const float4*>(
                    &A[
                        globalARow * K +
                        globalACol
                    ]
                );

            float4 value = ptr[0];


            tileA[loadRow][loadCol + 0] = value.x;
            tileA[loadRow][loadCol + 1] = value.y;
            tileA[loadRow][loadCol + 2] = value.z;
            tileA[loadRow][loadCol + 3] = value.w;

        } else {


            for (int i = 0; i < 4; i++) {

                int c =
                    globalACol + i;

                int localCol =
                    loadCol + i;

                if (
                    globalARow < M &&
                    c < K
                ) {
                    tileA[loadRow][localCol] =
                        A[
                            globalARow * K +
                            c
                        ];
                } else {
                    tileA[loadRow][localCol] =
                        0.0f;
                }
            }
        }


         int globalBRow =
            t * TILE_SIZE +
            loadRow;

        int globalBCol =
            blockIdx.x * TILE_SIZE +
            loadCol;


        if (
            globalBRow < K &&
            globalBCol + 3 < N
        ) {

            const float4* ptr =
                reinterpret_cast<const float4*>(
                    &B[
                        globalBRow * N +
                        globalBCol
                    ]
                );

            float4 value = ptr[0];


            tileB[loadRow][loadCol + 0] = value.x;
            tileB[loadRow][loadCol + 1] = value.y;
            tileB[loadRow][loadCol + 2] = value.z;
            tileB[loadRow][loadCol + 3] = value.w;

        } else {
            for (int i = 0; i < 4; i++) {

                int c =
                    globalBCol + i;

                int localCol =
                    loadCol + i;

                if (
                    globalBRow < K &&
                    c < N
                ) {
                    tileB[loadRow][localCol] =
                        B[
                            globalBRow * N +
                            c
                        ];
                } else {
                    tileB[loadRow][localCol] =
                        0.0f;
                }
            }
        }


        // ====================================================
        // Wait for entire tile to be loaded
        // ====================================================

        __syncthreads();


        // ====================================================
        // Compute
        //
        // Each thread computes a 2x2 C tile.
        //
        // Everything below is reused from shared memory.
        // ====================================================

        #pragma unroll
        for (int j = 0; j < TILE_SIZE; j++) {

            float a0 =
                tileA[
                    ty * 2 + 0
                ][j];

            float a1 =
                tileA[
                    ty * 2 + 1
                ][j];


            float b0 =
                tileB[j][
                    tx * 2 + 0
                ];

            float b1 =
                tileB[j][
                    tx * 2 + 1
                ];


            c00 += a0 * b0;
            c01 += a0 * b1;
            c10 += a1 * b0;
            c11 += a1 * b1;
        }


        // ====================================================
        // Make sure everyone has finished reading the tile
        // before it gets overwritten in the next iteration.
        // ====================================================

        __syncthreads();
    }


    // ========================================================
    // Store the 2x2 result
    // ========================================================

    if (row < M && col < N) {

        C[
            row * N + col
        ] = c00;
    }


    if (row < M && col + 1 < N) {

        C[
            row * N + col + 1
        ] = c01;
    }


    if (row + 1 < M && col < N) {

        C[
            (row + 1) * N + col
        ] = c10;
    }


    if (
        row + 1 < M &&
        col + 1 < N
    ) {

        C[
            (row + 1) * N + col + 1
        ] = c11;
    }
}


// ============================================================
// GPU solve
// ============================================================

void solve(
    const float* A,
    const float* B,
    float* C,
    int M,
    int N,
    int K
) {
    dim3 threadsPerBlock(
        8,
        8
    );


    dim3 blocksPerGrid(
        (N + TILE_SIZE - 1) / TILE_SIZE,
        (M + TILE_SIZE - 1) / TILE_SIZE
    );


    matrix_multiplication_kernel<<<
        blocksPerGrid,
        threadsPerBlock
    >>>(
        A,
        B,
        C,
        M,
        K,
        N
    );
}


void cpu_gemm(
    const float* A,
    const float* B,
    float* C,
    int M,
    int K,
    int N
) {
    for (int i = 0; i < M; i++) {

        for (int j = 0; j < N; j++) {

            float sum = 0.0f;

            for (int k = 0; k < K; k++) {

                sum +=
                    A[i * K + k] *
                    B[k * N + j];
            }

            C[i * N + j] = sum;
        }
    }
}


int main() {

     int M = 4096;
    int K = 4096;
    int N = 4096;


    std::cout
        << "Matrix dimensions:\n";

    std::cout
        << "A: "
        << M
        << " x "
        << K
        << "\n";

    std::cout
        << "B: "
        << K
        << " x "
        << N
        << "\n";

    std::cout
        << "C: "
        << M
        << " x "
        << N
        << "\n\n";


       size_t size_A =
        static_cast<size_t>(M) * K;

    size_t size_B =
        static_cast<size_t>(K) * N;

    size_t size_C =
        static_cast<size_t>(M) * N;

    float* h_A =
        new float[size_A];

    float* h_B =
        new float[size_B];

    // GPU result copied back here
    float* h_C =
        new float[size_C];

    // CPU reference result
    float* h_C_cpu =
        new float[size_C];


  
    for (size_t i = 0; i < size_A; i++) {

        h_A[i] =
            static_cast<float>(
                (i % 100) / 100.0
            );
    }


    for (size_t i = 0; i < size_B; i++) {

        h_B[i] =
            static_cast<float>(
                (i % 100) / 100.0
            );
    }


    float* d_A;
    float* d_B;
    float* d_C;


    cudaMalloc(
        &d_A,
        size_A * sizeof(float)
    );


    cudaMalloc(
        &d_B,
        size_B * sizeof(float)
    );


    cudaMalloc(
        &d_C,
        size_C * sizeof(float)
    );

    cudaMemcpy(
        d_A,
        h_A,
        size_A * sizeof(float),
        cudaMemcpyHostToDevice
    );


    cudaMemcpy(
        d_B,
        h_B,
        size_B * sizeof(float),
        cudaMemcpyHostToDevice
    );


       for (int i = 0; i < 5; i++) {

        solve(
            d_A,
            d_B,
            d_C,
            M,
            N,
            K
        );
    }


    cudaDeviceSynchronize();

    const int iterations = 20;


    auto start =
        std::chrono::high_resolution_clock::now();


    for (int i = 0; i < iterations; i++) {

        solve(
            d_A,
            d_B,
            d_C,
            M,
            N,
            K
        );
    }


    // IMPORTANT:
    // CUDA launches are asynchronous.
    // We must wait before stopping the CPU timer.

    cudaDeviceSynchronize();


    auto end =
        std::chrono::high_resolution_clock::now();


    std::chrono::duration<double, std::milli>
        elapsed =
            end - start;


    double total_ms =
        elapsed.count();


    double average_ms =
        total_ms / iterations;


 
    double flops =
        2.0 *
        static_cast<double>(M) *
        static_cast<double>(K) *
        static_cast<double>(N);


    double seconds =
        average_ms / 1000.0;


    double gflops =
        flops /
        seconds /
        1e9;


  
    std::cout
        << "============================\n";

    std::cout
        << "Vectorized Register-Tiled MatMul\n";

    std::cout
        << "============================\n";

    std::cout
        << "Iterations: "
        << iterations
        << "\n";

    std::cout
        << "Average kernel time: "
        << average_ms
        << " ms\n";

    std::cout
        << "Performance: "
        << gflops
        << " GFLOP/s\n";



    cudaMemcpy(
        h_C,
        d_C,
        size_C * sizeof(float),
        cudaMemcpyDeviceToHost
    );


    std::cout
        << "\nRunning CPU reference GEMM...\n";


    auto cpu_start =
        std::chrono::high_resolution_clock::now();


    cpu_gemm(
        h_A,
        h_B,
        h_C_cpu,
        M,
        K,
        N
    );


    auto cpu_end =
        std::chrono::high_resolution_clock::now();


    std::chrono::duration<double, std::milli>
        cpu_elapsed =
            cpu_end - cpu_start;


    std::cout
        << "CPU GEMM time: "
        << cpu_elapsed.count()
        << " ms\n";


    float max_diff =
        0.0f;

    double total_error =
        0.0;


    for (size_t i = 0; i < size_C; i++) {

        float diff =
            std::abs(
                h_C[i] -
                h_C_cpu[i]
            );


        if (diff > max_diff)
            max_diff = diff;


        total_error += diff;
    }


    double mean_error =
        total_error /
        static_cast<double>(size_C);


    std::cout
        << "\n============================\n";

    std::cout
        << "Correctness Check\n";

    std::cout
        << "============================\n";


    std::cout
        << "Max absolute difference: "
        << max_diff
        << "\n";


    std::cout
        << "Mean absolute difference: "
        << mean_error
        << "\n";


    if (max_diff < 1e-3f) {

        std::cout
            << "RESULT: PASS\n";

    } else {

        std::cout
            << "RESULT: FAIL\n";
    }

    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);


    delete[] h_A;
    delete[] h_B;
    delete[] h_C;
    delete[] h_C_cpu;


    return 0;
}
