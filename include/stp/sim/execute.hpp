// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stp/sim/execute_cuda.hpp>
#include <vector>

using stp_data = uint32_t;
using id = stp_data;
using stp_expr = std::vector<id>;

inline std::vector<stp_data> generate_swap_vec(const stp_data &m, const stp_data &n)
{
  stp_data dim = m * n;
  std::vector<stp_data> swap_matrix(dim + 1);
  swap_matrix[0] = dim;
  stp_data I, J;

  for (size_t i = 0; i < dim; ++i)
  {
    I = i / n;
    J = i % n;

    swap_matrix[i + 1] = J * m + I;
  }

  return swap_matrix;
}

inline std::vector<stp_data> generate_Mr_vec(const stp_data &k)
{
  stp_data dim = k;
  std::vector<stp_data> Mr_matrix(dim + 1);
  Mr_matrix[0] = dim * dim;

  for (size_t i = 1; i < dim + 1; ++i)
  {
    Mr_matrix[i] = (i - 1) * (k + 1);
  }

  return Mr_matrix;
}

// // In_KR_Matrix
// inline std::vector<stp_data> In_KR_Vec(stp_data dim, const std::vector<stp_data> &A)
// {
//     // Get the dimensions of matrix A
//     stp_data A_row = A[0];
//     stp_data A_col = A.size() - 1;

//     // Calculate the size of result matrix
//     stp_data C_len = dim * A_col + 1;

//     // Define the result matrix
//     std::vector<stp_data> C(C_len);

//     // Assign the number of rows of result matrix
//     C[0] = A_row * dim;

//     for (stp_data i = 0; i < dim; i++)
//     {
//         stp_data temp = i * A_row;
//         stp_data idx = i * A_col + 1;
//         for (stp_data j = 0; j < A_col; j++)
//         {
//             C[idx + j] = temp + A[j + 1];
//         }
//     }
//     return C;
// }

inline std::vector<stp_data> In_KR_Vec(stp_data dim, const std::vector<stp_data> &A)
{
  // Get the dimensions of matrix A
  stp_data A_row = A[0];
  stp_data A_col = A.size() - 1;

  // Calculate the size of result matrix
  stp_data C_len = dim * A_col + 1;

  // Define the result matrix
  std::vector<stp_data> C(C_len);

  // Assign the number of rows of result matrix
  C[0] = A_row * dim;

  const stp_data *A_data = A.data() + 1;

  for (stp_data i = 0; i < dim; i++)
  {
    stp_data temp = i * A_row;
    stp_data idx = i * A_col + 1;

    std::transform(A_data, A_data + A_col, C.data() + idx,
                   [temp](stp_data a_val) { return temp + a_val; });
  }
  return C;
}

std::vector<stp_data> Vec_KR_In(stp_data dim, const std::vector<stp_data> &A)
{
  // get dimensions of matrix A
  stp_data A_row = A[0];
  stp_data A_col = A.size() - 1;

  // calculate size of result matrix
  stp_data C_len = A_col * dim + 1;

  std::vector<stp_data> C(C_len);

  // assign number of rows of result matrix
  C[0] = A_row * dim;

  const stp_data *A_data = A.data() + 1;

  for (stp_data i = 0; i < A_col; i++)
  {
    stp_data temp = A_data[i] * dim;
    stp_data idx = i * dim + 1;

    std::iota(C.begin() + idx, C.begin() + idx + dim, temp);
  }
  return C;
}

// // Matrix_KR_In
// std::vector<stp_data> Vec_KR_In(stp_data dim, const std::vector<stp_data> &A)
// {
//     // get dimensions of matrix A
//     stp_data A_row = A[0];
//     stp_data A_col = A.size() - 1;

//     // calculate size of result matrix
//     // int32_t C_len = A_row * dim + 1;
//     stp_data C_len = A_col * dim + 1;

//     std::vector<stp_data> C(C_len);

//     // assign number of rows of result matrix
//     C[0] = A_row * dim;

//     for (stp_data i = 0; i < A_col; i++)
//     {
//         stp_data temp = A[i + 1] * dim;
//         stp_data idx = i * dim + 1;
//         for (stp_data j = 0; j < dim; j++)
//         {
//             C[idx + j] = temp + j;
//         }
//     }
//     return C;
// }

std::vector<stp_data> Vec_semi_tensor_product(const std::vector<stp_data> &A,
                                              const std::vector<stp_data> &B)
{
  // get dimensions of matrix A and B
  stp_data A_row = A[0];
  stp_data A_col = A.size() - 1;
  stp_data B_row = B[0];
  stp_data B_col = B.size() - 1;

  if (A_col % B_row == 0)
  {
    // calculate size of result matrix
    stp_data C_len = (int64_t)A_col * B_col / B_row + 1;

    std::vector<stp_data> C(C_len);

    C[0] = A_row;
    stp_data times = A_col / B_row;

    for (stp_data i = 0; i < B_col; i++)
    {
      // t = n/p
      for (stp_data j = 0; j < times; j++)
      {
        C[times * i + j + 1] = A[1 + B[i + 1] * times + j];
      }
    }

    return C;
  }
  else if (B_row % A_col == 0)
  {
    std::vector<stp_data> temp = Vec_KR_In(B_row / A_col, A);
    std::vector<stp_data> C = Vec_semi_tensor_product(temp, B);

    return C;
  }
  else
  {
    // error
    std::cout << "Error" << std::endl;
    std::vector<stp_data> C;
    // set size to improve computation speed
    C.resize(1);
    C[0] = -1;

    return C;
  }
}

std::vector<stp_data> Vec_chain_multiply(std::vector<std::vector<stp_data>> &mc, bool verbose)
{
  std::vector<stp_data> result = mc[0];

  if (mc.size() < 2)
  {
    return result;
  }

  for (size_t i = 1; i < mc.size(); i++)
  {
    result = Vec_semi_tensor_product(result, mc[i]);
  }
  return result;
}
