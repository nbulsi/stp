/* stp: C++ semi-tensor product library for electronic design automation (EDA)
 * Copyright (C) 2023-  Ningbo University, Zhejiang, China
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file stp_eigen.hpp
  \brief header file for some basic STP operations
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>

// include a kronecker product headfile (offically unsupported in eigen)
#include <unsupported/Eigen/KroneckerProduct>

#include <iostream>
#include <numeric>
#include <stp/stp_timer.hpp>

using matrix = Eigen::MatrixXi;            // Defines the type of matrix to use
using matrix_chain = std::vector<matrix>;  // Defined matrix chain

namespace stp
{
inline matrix matrix_random_generation( int row, int col )
{
  matrix result( row, col );
  for ( int i = 0; i < row; i++ )
    {
      for ( int j = 0; j < col; j++ )
        {
          result( i, j ) = rand() % 2;
        }
    }
  return result;
}

inline matrix generate_swap_matrix( const int& m, const int& n )
{
  matrix swap_matrixXi = matrix::Zero( m * n, m * n );
  int p, q;
  for ( int i = 0; i < m * n / 2 + 1; i++ )
    {
      p = i / m;
      q = i % m;
      int j = q * n + p;
      swap_matrixXi( i, j ) = 1;
      swap_matrixXi( m * n - 1 - i, m * n - 1 - j ) = 1;
    }
  return swap_matrixXi;
}

inline bool is_identity_matrix( const matrix& A )
{
  if ( A.rows() != A.cols() )
    {
      return false;
    }

  for ( int i = 0u; i < A.rows(); i++ )
    {
      for ( int j = 0u; j < A.cols(); j++ )
        {
          if ( i == j )
            {
              if ( A( i, j ) != 1 )
                {
                  return false;
                }
            }
          else
            {
              if ( A( i, j ) != 0 )
                {
                  return false;
                }
            }
        }
    }

  return true;
}

inline matrix kronecker_product( const matrix& A, const matrix& B )
{
  /* trivial cases */
  auto a_dimensions = A.rows() * A.cols();
  auto b_dimensions = B.rows() * B.cols();

  if ( a_dimensions == 1u )
    {
      return B;
    }
  if ( b_dimensions == 1u )
    {
      return A;
    }

  Eigen::SparseMatrix<int> sparse_A = A.sparseView();
  Eigen::SparseMatrix<int> sparse_B = B.sparseView();

  Eigen::SparseMatrix<int> KP = Eigen::kroneckerProduct( sparse_A, sparse_B );

  matrix result( KP );

  return result;
}

enum class stp_method : uint8_t
{
  copy_method = 1,
  native_method = 2,
};

class semi_tensor_product_impl
{
 public:
  semi_tensor_product_impl( const matrix& A, const matrix& B, bool verbose,
                            const stp_method method )
      : A( A ), B( B ), verbose( verbose ), method( method )
  {
    m = A.rows();
    n = A.cols();
    p = B.rows();
    q = B.cols();
  }

  matrix run()
  {
    matrix result;

    if ( method == stp_method::native_method )
      {
        result = call_with_stopwatch(
            time, [&]() { return semi_tensor_product_native(); } );
      }
    else
      {
        result = call_with_stopwatch(
            time, [&]() { return semi_tensor_product_copy(); } );
      }

    if ( verbose )
      {
        report( result );
      }

    return result;
  }

 private:
  uint32_t get_lcm( uint32_t m, uint32_t n )
  {
    return ( m * n ) / std::gcd( m, n );
  }

  matrix semi_tensor_product_native()
  {
    unsigned t = get_lcm( n, p );

    matrix Ia = matrix::Identity( t / n, t / n );
    matrix Ib = matrix::Identity( t / p, t / p );

    matrix KPa = kronecker_product( A, Ia );
    matrix KPb = kronecker_product( B, Ib );

    //return KPa * KPb;
    Eigen::SparseMatrix<int> sparse_KPa = KPa.sparseView();
    Eigen::SparseMatrix<int> sparse_KPb = KPb.sparseView();

    Eigen::SparseMatrix<int> KP = sparse_KPa * sparse_KPb;
    matrix result( KP );

    return result;
  }

  // copy method as default
  matrix semi_tensor_product_copy()
  {
    int row, col;
    matrix result_matrix;

    if ( n % p == 0 )
      {
        int times = n / p;
        row = m;
        col = times * q;
        result_matrix = matrix::Zero( row, col );

        for ( int i = 0; i < q; ++i )
          {
            for ( int j = 0; j < p; ++j )
              {
                result_matrix.block( 0, i * times, m, times ) +=
                    B( j, i ) * A.block( 0, j * times, m, times );
              }
          }
      }
    else if ( p % n == 0 )
      {
        int times = p / n;
        row = m * times;
        col = q;
        result_matrix = matrix::Zero( row, col );
        for ( int i = 0; i < m; ++i )
          {
            for ( int j = 0; j < n; ++j )
              {
                result_matrix.block( i * times, 0, times, q ) +=
                    A( i, j ) * B.block( j * times, 0, times, q );
              }
          }
      }
    else
      {
        std::cout << "matrix type error!" << std::endl;
        assert( false );
      }

    return result_matrix;
  }

  void report( const matrix& result )
  {
    std::cout << "--------------------STP Computation----------------------\n";
    std::cout << "Dimension A: " << m << " x " << n << "\n";
    std::cout << "Dimension B: " << p << " x " << q << "\n";

    if ( method == stp_method::native_method )
      {
        std::cout << "Use the native method to compute STP.\n";
      }
    else
      {
        std::cout << "Use the copy method to compute STP.\n";
      }

    std::cout << "Result Dimensions: " << result.rows() << " x "
              << result.cols() << "\n";
    std::cout << "Total time: " << to_millisecond( time ) << "ms\n";
    std::cout << "---------------------------------------------------------\n";
  }

 private:
  const matrix& A;
  const matrix& B;
  const bool verbose{false};
  const stp_method method;
  uint32_t m{0u}, n{0u}, p{0u}, q{0u};
  stopwatch<>::duration time{0};
};

/*! \brief Compute the semi-tensor product of matrix A and matrix B.
  Example:
    matrix A, B;
    matrix result = semi_tensor_product(A, B);
*/
inline matrix semi_tensor_product(
    const matrix& A, const matrix& B, const bool verbose = false,
    const stp_method method = stp_method::copy_method )
{
  semi_tensor_product_impl stp_impl( A, B, verbose, method );
  return stp_impl.run();
}

enum class mc_multiply_method : uint8_t
{
  sequence = 0,
  dynamic_programming = 1
};

class matrix_chain_multiply_impl
{
 public:
  matrix_chain_multiply_impl( const matrix_chain& mc, bool verbose = false,
                              const mc_multiply_method method =
                                  mc_multiply_method::dynamic_programming )
      : mc( mc ), verbose( verbose ), method( method )
  {
    assert( mc.size() > 0 );
  }

  matrix run()
  {
    switch ( method )
      {
        case mc_multiply_method::sequence:
          matrix_chain_multiply();
          break;

        case mc_multiply_method::dynamic_programming:
          {
            orders = matrix_chain_order();
            result = parse_multiplication_order( orders );
          }
          break;

        default:
          break;
      }

    if ( verbose )
      {
        report();
      }

    return result;
  }

  const std::vector<int> get_order() { return matrix_chain_order(); }

 private:
  void matrix_chain_multiply()
  {
    if ( mc.size() == 1 )
      {
        result = mc[ 0 ];
        return;
      }

    total_cost += complexity_analysis( mc[ 0 ].rows(), mc[ 0 ].cols(),
                                       mc[ 1 ].rows(), mc[ 1 ].cols() )[ 0 ];
    result = call_with_stopwatch(
        time, [&]() { return semi_tensor_product( mc[ 0 ], mc[ 1 ] ); } );

    for ( int i = 2; i < mc.size(); i++ )
      {
        total_cost += complexity_analysis(
            result.rows(), result.cols(), mc[ i ].rows(), mc[ i ].cols() )[ 0 ];
        result = call_with_stopwatch(
            time, [&]() { return semi_tensor_product( result, mc[ i ] ); } );
      }
  }

  std::vector<int> matrix_chain_order()
  {
    struct mc_info
    {
      uint64_t op_num =
          0;  // Records the total complexity of the current operation
      uint64_t flag = 0;  // Record the matrix multiplication sequence
      uint64_t row = 0;
      uint64_t col = 0;
    };

    int length = mc.size();
    std::vector<std::vector<mc_info>> dp( length,
                                          std::vector<mc_info>( length ) );

    for ( int i = 0; i < length; i++ )
      {
        dp[ i ][ i ].row = mc[ i ].rows();
        dp[ i ][ i ].col = mc[ i ].cols();
      }

    for ( int l = 2; l <= length; l++ ) /* The length of the matrix chain */
      {
        for ( int i = 0; i < length - l + 1; i++ )
          {
            int j = i + l - 1;
            dp[ i ][ j ].op_num = UINT64_MAX;
            std::vector<uint64_t> temp;
            for ( int k = i; k < j; k++ )
              {
                temp = complexity_analysis( dp[ i ][ k ].row, dp[ i ][ k ].col,
                                            dp[ k + 1 ][ j ].row,
                                            dp[ k + 1 ][ j ].col );
                uint64_t q =
                    dp[ i ][ k ].op_num + dp[ k + 1 ][ j ].op_num + temp[ 0 ];

                if ( q < dp[ i ][ j ].op_num )
                  {
                    dp[ i ][ j ].op_num = q;
                    dp[ i ][ j ].flag = k;
                    dp[ i ][ j ].row = temp[ 1 ];
                    dp[ i ][ j ].col = temp[ 2 ];
                  }
              }
          }
      }

    // Parse the Dynamic programming table
    std::vector<int> rule;
    std::vector<std::vector<int>> flags( length, std::vector<int>( length ) );
    for ( int i = 0; i < length; i++ )
      {
        for ( int j = 0; j < length; j++ )
          {
            flags[ i ][ j ] = dp[ i ][ j ].flag;
          }
      }
    optimal_parens( flags, 0, length - 1, rule );
    return rule;
  }

  std::vector<uint64_t> complexity_analysis( int m, int n, int p, int q )
  {
    /*
      temp[0] operational complexity
      temp[1] rows
      temp[2] cols
    */
    std::vector<uint64_t> temp( 3, 0u );

    if ( n % p == 0 )
      {
        uint64_t times = n / p;
        uint64_t row = m;
        uint64_t col = times * q;

        // temp[0] = n * q / p;
        temp[ 0 ] = ( 2 + 1 ) * ( m * times ) * p * q;
        temp[ 1 ] = m;
        temp[ 2 ] = n * q / p;
        return temp;
      }
    else if ( p % n == 0 )
      {
        uint64_t times = p / n;
        uint64_t row = m * times;
        uint64_t col = q;
        // temp[0] = q;
        temp[ 0 ] = ( 2 + 1 ) * ( times * q ) * m * n;
        temp[ 1 ] = m * p / n;
        temp[ 2 ] = q;
        return temp;
      }
    else
      {
        std::cout << "matrix type error!" << std::endl;
      }

    return temp;
  }

  void optimal_parens( const std::vector<std::vector<int>>& s, int i, int j,
                       std::vector<int>& rule )
  {
    if ( i == j )
      {
        rule.push_back( i );
      }
    else
      {
        rule.push_back( -1 );
        optimal_parens( s, i, s[ i ][ j ], rule );
        optimal_parens( s, s[ i ][ j ] + 1, j, rule );
        rule.push_back( -2 );
      }
  }

  matrix parse_multiplication_order( std::vector<int> order )
  {
    std::vector<matrix> stacks;
    std::vector<int> idx;

    for ( int i = 0; i < order.size(); ++i )
      {
        if ( order[ i ] == -1 )
          {
            idx.push_back( i );
          }
        else if ( order[ i ] == -2 )
          {
            if ( stacks.size() >= 2 )
              {
                matrix& A = stacks[ stacks.size() - 2 ];
                matrix& B = stacks[ stacks.size() - 1 ];
                total_cost += complexity_analysis( A.rows(), A.cols(), B.rows(),
                                                   B.cols() )[ 0 ];
                A = call_with_stopwatch(
                    time, [&]() { return semi_tensor_product( A, B ); } );
                stacks.pop_back();
              }

            order.erase( order.begin() + idx.back(), order.begin() + i );
            i = idx.back();
            idx.pop_back();
          }
        else
          {
            stacks.push_back( mc[ order[ i ] ] );
          }
      }

    assert( stacks.size() == 1 );
    return stacks[ 0 ];
  }

  void report()
  {
    std::cout
        << "------------------Matrix Chain STP Computation-----------------\n";
    if ( method == mc_multiply_method::dynamic_programming )
      {
        std::cout
            << "Use dynamic programming method for matrix chain multiply.\n";
        std::cout << "The parenthesis are added as shown in the following.\n";
        for ( int t : orders )
          {
            if ( t == -1 )
              std::cout << "(";
            else if ( t == -2 )
              std::cout << ")";
            else
              std::cout << "M" << t;
          }
        std::cout << "\n";
        std::cout << "Total time: " << to_millisecond( time ) << "ms\n";
      }
    else
      {
        std::cout << "Use sequence method for matrix chain multiply.\n";
        std::cout << "Total time: " << to_millisecond( time ) << "ms\n";
      }
    std::cout
        << "-------------------------------------------------------------\n";
  }

 private:
  const bool verbose{false};
  const mc_multiply_method method{0u};
  const matrix_chain& mc;
  matrix result;
  std::vector<int> orders;
  stopwatch<>::duration time{0};
  uint64_t total_cost{0u};
};

inline matrix matrix_chain_multiply(
    const matrix_chain& mc, const bool verbose = false,
    const mc_multiply_method method = mc_multiply_method::dynamic_programming )
{
  matrix_chain_multiply_impl mcm_impl( mc, verbose, method );
  return mcm_impl.run();
}

}  // namespace stp
