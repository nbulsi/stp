#pragma once

#include <iostream>

#include <chrono>
#include <stp/utils/stp_utils.hpp>

typedef struct
{
  stp_data *d_Vec = nullptr;
  stp_data _col = 0;
  stp_data _row = 0;
  int32_t need_release = 0;
} CUDA_DATA;

#ifdef ENABLE_CUDA

extern "C" CUDA_DATA my_cuda_In_KR_Matrix(int32_t dim, CUDA_DATA &A);

extern "C" CUDA_DATA my_cuda_Matrix_KR_In(int32_t dim, CUDA_DATA &A);

extern "C" void Get_Total_Thread_Num(void);

extern "C" CUDA_DATA my_cuda_semi_tensor_product(CUDA_DATA &A, CUDA_DATA &B);

extern "C" CUDA_DATA Memcpy_To_Device(std::vector<stp_data> &A);

extern "C" bool Free_Device_Memory(CUDA_DATA &C);

extern "C" std::vector<stp_data> Memcpy_To_Host(CUDA_DATA &C);

#endif
