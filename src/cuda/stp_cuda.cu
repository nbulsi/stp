// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <cusparse.h>

#include <stp/sim/execute_cuda.hpp>

namespace
{
void check_cuda(const cudaError_t status, const char *operation)
{
  if (status != cudaSuccess)
    throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}

void check_kernel(const char *operation)
{
  check_cuda(cudaGetLastError(), operation);
  check_cuda(cudaDeviceSynchronize(), operation);
}

void release_owned(CUDA_DATA &data)
{
  if (data.d_Vec != nullptr && data.need_release != 0)
  {
    check_cuda(cudaFree(data.d_Vec), "cudaFree");
    data.d_Vec = nullptr;
    data.need_release = 0;
  }
}
} // namespace

uint64_t Total_Thread = 0; // total supported threads

extern "C"
    // get total thread number
    bool Get_Total_Thread_Num(void)
{
  int deviceCount = 0;
  // get the number of CUDA devices
  cudaError_t error = cudaGetDeviceCount(&deviceCount);
  if (error != cudaSuccess)
  {
    std::cerr << "cudaGetDeviceCount failed: " << cudaGetErrorString(error) << std::endl;
    return false;
  }
  if (deviceCount == 0)
  {
    std::cerr << "cudaGetDeviceCount found no CUDA-capable devices" << std::endl;
    return false;
  }

  Total_Thread = 0;
  for (int i = 0; i < deviceCount; ++i)
  {
    cudaDeviceProp deviceProp;
    error = cudaGetDeviceProperties(&deviceProp, i);
    if (error != cudaSuccess)
    {
      std::cerr << "cudaGetDeviceProperties failed for device " << i << ": "
                << cudaGetErrorString(error) << std::endl;
      return false;
    }

    // std::cout << "Device " << i << ": " << deviceProp.name << std::endl;
    // std::cout << "Max threads per block: " << deviceProp.maxThreadsPerBlock << std::endl;
    // std::cout << "Max blocks per grid: " << (int64_t)deviceProp.maxGridSize[0] *
    // deviceProp.maxGridSize[1] * deviceProp.maxGridSize[2] << std::endl; std::cout << "  X : " <<
    // deviceProp.maxGridSize[0] << std::endl; std::cout << "  Y : " << deviceProp.maxGridSize[1] <<
    // std::endl; std::cout << "  Z : " << deviceProp.maxGridSize[2] << std::endl; std::cout <<
    // "Total max threads: " << deviceProp.maxThreadsPerMultiProcessor *
    // deviceProp.multiProcessorCount << std::endl; std::cout << std::endl;
    Total_Thread = deviceProp.maxThreadsPerMultiProcessor * deviceProp.multiProcessorCount;
  }
  return Total_Thread != 0;
}

extern "C" CUDA_DATA Memcpy_To_Device(std::vector<stp_data> &A)
{
  if (A.empty())
    throw std::invalid_argument("cannot copy an empty STP vector to CUDA");

  CUDA_DATA C;
  C._row = A[0];
  C._col = A.size() - 1;
  C.need_release = 1;

  // compute space
  size_t size_A = (A.size() - 1) * sizeof(stp_data);

  // allocate memory
  check_cuda(cudaMalloc((void **)&C.d_Vec, size_A), "cudaMalloc for device vector");

  // Copy parameters
  check_cuda(cudaMemcpy(C.d_Vec, A.data() + 1, size_A, cudaMemcpyHostToDevice),
             "cudaMemcpy host to device");

  return C;
}

extern "C"
    // release GPU memory
    bool Free_Device_Memory(CUDA_DATA &C)
{
  if (C.d_Vec == nullptr)
    return true;

  cudaError_t err = cudaFree(C.d_Vec);
  if (err != cudaSuccess)
  {
    std::cerr << "Error freeing memory for In_KR_Matrix d_C: " << cudaGetErrorString(err)
              << std::endl;
    return false;
  }
  C.d_Vec = nullptr;
  C.need_release = 0;
  return true;
}

extern "C" std::vector<stp_data> Memcpy_To_Host(CUDA_DATA &C)
{
  if (C.d_Vec == nullptr && C._col != 0)
    throw std::invalid_argument("cannot copy a null CUDA vector to the host");

  std::vector<stp_data> A(C._col + 1);
  A[0] = C._row;
  check_cuda(cudaMemcpy(A.data() + 1, C.d_Vec, (C._col) * sizeof(stp_data), cudaMemcpyDeviceToHost),
             "cudaMemcpy device to host");
  release_owned(C);

  return A;
}

// In_KR_Matrix_Kernel
__global__ void In_KR_Matrix_Kernel(int32_t sub_dim, int32_t idx_offset, stp_data *A,
                                    stp_data A_row, stp_data A_val_len, stp_data *C,
                                    stp_data C_val_len)
{
  stp_data ix = blockIdx.x * 1024 + threadIdx.x;
  // index
  stp_data idx = ix + idx_offset;

  stp_data x_code = idx / A_val_len;

  stp_data y_code = idx % A_val_len;

  // boundary check
  if (idx < C_val_len)
  {
    // XP+Y
    C[idx] = x_code * A_row + A[y_code];
  }
}

extern "C" CUDA_DATA my_cuda_In_KR_Matrix(int32_t dim, CUDA_DATA &A)
{
  if (Total_Thread == 0)
    throw std::runtime_error("CUDA thread capacity is not initialized");

  // Get the dimensions of matrix A
  stp_data A_row = A._row;
  stp_data A_col = A._col;

  // Calculate the size of result matrix
  stp_data C_len = dim * A_col;

  CUDA_DATA C;
  C._row = A_row * dim;
  C._col = A_col * dim;

  // Assign the number of rows of result matrix
  stp_data *d_C = nullptr;

  // compute space
  size_t size_C = C_len * sizeof(stp_data);

  // allocate memory
  check_cuda(cudaMalloc((void **)&d_C, size_C), "cudaMalloc for In_KR_Matrix result");

  // Calculate the block size (maximum 1024)
  dim3 threadsPerBlock(1024, 1);

  // Can be done in one go (with each element of C as a thread)
  if ((C_len) <= Total_Thread)
  {
    // Calculate the grid size (for large scale)
    dim3 numBlocks0((C_len + 1024 - 1) / 1024, 1);

    // Launch GPU
    In_KR_Matrix_Kernel<<<numBlocks0, threadsPerBlock>>>(dim, 0, A.d_Vec, A_row, A_col, d_C, C_len);
    check_kernel("In_KR_Matrix kernel");
  }
  // Divide into blocks (by column)
  else
  {
    int32_t remain_num = C_len; // remaining unassigned columns in C
    int32_t idx_offset = 0;     // Thread offset

    while (remain_num)
    {
      // the last
      if (remain_num <= Total_Thread)
      {
        // Calculate the grid size (for large scale)
        dim3 numBlocks((remain_num + 1024 - 1) / 1024, 1);

        In_KR_Matrix_Kernel<<<numBlocks, threadsPerBlock>>>(dim, idx_offset, A.d_Vec, A_row, A_col,
                                                            d_C, C_len);
        check_kernel("In_KR_Matrix kernel");
        // Calculate the thread offset
        idx_offset += remain_num;
        remain_num = 0; // exit the loop
      }
      // Total thread
      else
      {
        // Calculate the grid size (for large scale)
        dim3 numBlocks((Total_Thread + 1024 - 1) / 1024, 1);

        In_KR_Matrix_Kernel<<<numBlocks, threadsPerBlock>>>(dim, idx_offset, A.d_Vec, A_row, A_col,
                                                            d_C, C_len);
        check_kernel("In_KR_Matrix kernel");

        // Calculate thread offset
        idx_offset += Total_Thread;
        remain_num -= Total_Thread; // exit the loop
      }
    }
  }

  // Free resources
  //  if(A_col>20)
  //  {
  //      cudaFree(A.d_Vec);
  //  }
  release_owned(A);
  C.d_Vec = d_C;
  C.need_release = 1;

  return C;
}

// Matrix_KR_In_Kernel
__global__ void Matrix_KR_In_Kernel(int32_t dim, int32_t idx_offset, stp_data *A,
                                    stp_data A_val_len, stp_data *C, stp_data C_val_len)
{
  stp_data ix = blockIdx.x * 1024 + threadIdx.x;
  // index
  stp_data idx = ix + idx_offset;

  // calculate x_code
  stp_data x_code = idx / dim;

  // calculate y_code
  stp_data y_code = idx % dim;

  // boundary check
  if (idx < C_val_len)
  {
    // xp+y
    C[idx] = A[x_code] * dim + y_code;
  }
}

extern "C"
    // Matrix_KR_In
    CUDA_DATA my_cuda_Matrix_KR_In(int32_t dim, CUDA_DATA &A)
{
  if (Total_Thread == 0)
    throw std::runtime_error("CUDA thread capacity is not initialized");

  // get dimensions of matrix A
  stp_data A_row = A._row;
  stp_data A_col = A._col;

  // calculate size of result matrix
  stp_data C_len = A_col * dim;

  CUDA_DATA C;
  C._row = A_row * dim;
  C._col = A_col * dim;

  stp_data *d_C = nullptr;
  // compute space
  size_t size_C = C_len * sizeof(stp_data);

  // allocate memory
  check_cuda(cudaMalloc((void **)&d_C, size_C), "cudaMalloc for Matrix_KR_In result");

  // calculate block size (maximum 1024)
  dim3 threadsPerBlock(1024, 1);

  // can be done in one go (with each element of C as a thread)
  if ((C_len - 1) <= Total_Thread)
  {
    // calculate grid size (for large scale)
    dim3 numBlocks((C_len + 1024 - 1) / 1024, 1);

    Matrix_KR_In_Kernel<<<numBlocks, threadsPerBlock>>>(dim, 0, A.d_Vec, A_col, d_C, C_len);
    check_kernel("Matrix_KR_In kernel");
  }
  // divide into blocks (by column)
  else
  {
    int32_t remain_num = C_len; // C remaining unassigned columns
    int32_t idx_offset = 0;     // thread offset

    while (remain_num)
    {
      // last time  quantity remain_num
      if (remain_num <= Total_Thread)
      {
        // calculate grid size (for large scale)
        dim3 numBlocks((remain_num + 1024 - 1) / 1024, 1);

        Matrix_KR_In_Kernel<<<numBlocks, threadsPerBlock>>>(dim, idx_offset, A.d_Vec, A_col, d_C,
                                                            C_len);
        check_kernel("Matrix_KR_In kernel");
        // calculate thread offset
        idx_offset += remain_num;
        remain_num = 0; // exit the loop
      }
      // total thread
      else
      {
        // calculate grid size (for large scale)
        dim3 numBlocks((Total_Thread + 1024 - 1) / 1024, 1);

        Matrix_KR_In_Kernel<<<numBlocks, threadsPerBlock>>>(dim, idx_offset, A.d_Vec, A_col, d_C,
                                                            C_len);
        check_kernel("Matrix_KR_In kernel");

        // calculate thread offset
        idx_offset += Total_Thread;
        remain_num -= Total_Thread; // exit the loop
      }
    }
  }

  // free resources
  //  if(A_col>20)
  //  {
  //      cudaFree(A.d_Vec);
  //  }
  release_owned(A);

  C.d_Vec = d_C;
  C.need_release = 1;

  return C;
}

// Matrix_Multipiy_Kernel
__global__ void Matrix_Multipiy_Kernel(int32_t idx_offset, stp_data *A, int32_t A_val_len,
                                       stp_data *B, int32_t B_val_len, stp_data *C,
                                       int32_t C_val_len, int32_t t)
{
  int32_t ix = blockIdx.x * 1024 + threadIdx.x;
  // index
  int32_t idx = ix + idx_offset;

  // calculate x_code
  int32_t x_code = idx % t;

  // calculate y_code
  int32_t y_code = idx / t;

  // boundary check
  if (idx < C_val_len)
  {
    // result
    C[idx] = A[B[y_code] * t + x_code];
  }
}

extern "C"
    // my_semi_tensor_product
    CUDA_DATA my_cuda_semi_tensor_product(CUDA_DATA &A, CUDA_DATA &B)
{
  if (Total_Thread == 0)
    throw std::runtime_error("CUDA thread capacity is not initialized");

  // get dimensions of matrix A and B
  int32_t A_row = A._row;
  int32_t A_col = A._col;
  int32_t B_row = B._row;
  int32_t B_col = B._col;

  if (A_col % B_row == 0)
  {
    CUDA_DATA C;
    C._row = A_row;

    // calculate size of result matrix
    int32_t C_len = (int64_t)A_col * B_col / B_row;

    C._col = C_len;

    // caculate size of result matrix

    size_t size_C = C_len * sizeof(stp_data);

    stp_data *d_C = nullptr;

    // allocate memory
    check_cuda(cudaMalloc((void **)&d_C, size_C), "cudaMalloc for semi-tensor product result");

    // calculate block size (maximum 1024)
    dim3 threadsPerBlock(1024, 1);

    if (C_len <= Total_Thread)
    {
      // calculate grid size (for large scale)
      dim3 numBlocks((C_len + 1024 - 1) / 1024, 1);
      Matrix_Multipiy_Kernel<<<numBlocks, threadsPerBlock>>>(0, A.d_Vec, A._col, B.d_Vec, B._col,
                                                             d_C, C_len, A_col / B_row);

      // wait for all threads to complete
      check_kernel("semi-tensor product kernel");
    }
    else
    {
      int32_t remain_num = C_len; // remaining unassigned columns in C
      int32_t idx_offset = 0;     // thread offset

      while (remain_num)
      {
        // the last
        if (remain_num <= Total_Thread)
        {
          // calculate grid size (for large scale)
          dim3 numBlocks((remain_num + 1024 - 1) / 1024, 1);

          Matrix_Multipiy_Kernel<<<numBlocks, threadsPerBlock>>>(
              idx_offset, A.d_Vec, A._col, B.d_Vec, B._col, d_C, C_len, A_col / B_row);
          check_kernel("semi-tensor product kernel");
          // calculate thread offset
          idx_offset += remain_num;
          remain_num = 0; // exit the loop
        }
        // total thread
        else
        {
          // calculate grid size (for large scale)
          dim3 numBlocks((Total_Thread + 1024 - 1) / 1024, 1);

          Matrix_Multipiy_Kernel<<<numBlocks, threadsPerBlock>>>(
              idx_offset, A.d_Vec, A_col, B.d_Vec, B_col, d_C, C_len, A_col / B_row);
          check_kernel("semi-tensor product kernel");
          // calculate thread offset
          idx_offset += Total_Thread;
          remain_num -= Total_Thread; // exit the loop
        }
      }
    }

    // free resources
    //  if(A_col>20)
    //  {
    //      cudaFree(A.d_Vec);
    //  }
    //  if(B_col>20)
    //  {
    //      cudaFree(B.d_Vec);
    //  }

    release_owned(A);
    release_owned(B);
    C.d_Vec = d_C;
    C.need_release = 1;

    return C;
  }
  else if (B_row % A_col == 0)
  {
    CUDA_DATA temp = my_cuda_Matrix_KR_In(B_row / A_col, A);
    CUDA_DATA C = my_cuda_semi_tensor_product(temp, B);

    return C;
  }
  else
  {
    throw std::invalid_argument(
        "semi-tensor product requires one inner dimension to divide the other");
  }
}
