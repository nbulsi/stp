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
  \file stp_multi_thread.hpp
  \brief header file for stp multi thread calculation
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#pragma once

#include <Eigen/Dense>

#include <mutex>
#include <thread>

#include "stp/stp_eigen.hpp"
#include "stp/stp_timer.hpp"

using matrix = Eigen::MatrixXi;            // Defines the type of matrix to use
using matrix_chain = std::vector<matrix>;  // Defined matrix chain

namespace stp
{
struct temp_result
{
  matrix product;
  int start_index;
};

struct tNode
{
  tNode() {}
  tNode( bool is_matrix ) : is_matrix( is_matrix ) {}
  tNode( bool is_matrix, int f1, int f2 )
      : is_matrix( is_matrix ), fanin1( f1 ), fanin2( f2 )
  {
  }

  bool is_matrix = false;
  int level = -1;
  int fanin1 = 0;
  int fanin2 = 0;
  std::pair<int, int> dimensionality;
  uint64_t complexity;
  matrix mtx;
};

struct matrix_tree
{
  tNode& get_node( const int n )
  {
    assert( n < nodes.size() );
    return nodes[ n ];
  }

  int get_m( const int n ) { return get_node( n ).dimensionality.first; }

  int get_n( const int n ) { return get_node( n ).dimensionality.second; }

  uint64_t get_cost( const int n ) { return get_node( n ).complexity; }

  int root;
  std::vector<tNode> nodes;
};

class subchain_multiply_impl
{
 public:
  subchain_multiply_impl( const matrix_chain& mc, int num_threads, bool use_dp,
                          bool verbose )
      : mc( mc ),
        num_threads( num_threads ),
        use_dp( use_dp ),
        verbose( verbose )
  {
    max_num_threads = std::thread::hardware_concurrency();

    modified_num_threads = num_threads;

    if ( num_threads > max_num_threads )
      {
        modified_num_threads = max_num_threads;
        if ( verbose )
          {
            std::cout << "[w] The specified number of threads: " << num_threads
                      << std::endl;
            std::cout
                << "[w] The maximum number of threads suppoted by hardware: "
                << max_num_threads << std::endl;
            std::cout << "[w] The specified number of threads is larger than "
                         "the maximum one.\n";
          }
      }

    if ( num_threads > mc.size() )
      {
        modified_num_threads = mc.size() / 2;
        if ( verbose )
          {
            std::cout << "[w] The specified number of threads: " << num_threads
                      << std::endl;
            std::cout << "[w] The number of elements in the vector: "
                      << mc.size() << std::endl;
            std::cout << "[w] The specified number of threads is larger than "
                         "the vector size.\n";
          }
      }
  }

  matrix_chain get_sub_chain( int start, int end )
  {
    assert( start >= 0 );
    assert( start < mc.size() );
    assert( end > 0 );
    assert( end <= mc.size() );

    std::lock_guard<std::mutex> lock( mtx );
    matrix_chain sub_chain;

    for ( int i = start; i < end; i++ )
      {
        sub_chain.push_back( mc[ i ] );
      }

    return sub_chain;
  }

  matrix sub_chain_product( const matrix_chain& schain )
  {
    std::lock_guard<std::mutex> lock( mtx );
    return matrix_chain_multiply( schain );
  }

  matrix run()
  {
    matrix result;
    if ( !use_dp )
      {
        result = call_with_stopwatch( total_time, [&]() { return run1(); } );
      }
    else
      {
        result = call_with_stopwatch( total_time, [&]() { return run2(); } );
      }

    if ( verbose )
      {
        report();
      }

    return result;
  }

  matrix run1()
  {
    std::vector<std::thread> threads;

    int block_size = mc.size() / modified_num_threads;
    int remaining_elements = mc.size() % modified_num_threads;

    std::vector<temp_result> part( modified_num_threads );

    if ( verbose )
      {
        std::cout << "[i] The matrix chain size: " << mc.size()
                  << " real #threads: " << modified_num_threads << std::endl;
      }

    int start = 0;

    for ( int i = 0; i < modified_num_threads; i++ )
      {
        int num_elements = block_size + ( i < remaining_elements ? 1 : 0 );
        int end = start + num_elements;

        if ( verbose )
          {
            std::cout << "[i] Thread " << i << " starts to process "
                      << num_elements << " elements.\n";
          }

        threads.emplace_back( [this, i, &part, start, end]() {
          part[ i ].product = sub_chain_product( get_sub_chain( start, end ) );
          part[ i ].start_index = start;
        } );

        start = end;
      }

    for ( auto& thread : threads )
      {
        thread.join();
      }

    std::sort( part.begin(), part.end(),
               []( const temp_result& a, const temp_result& b ) {
                 return a.start_index < b.start_index;
               } );

    matrix_chain result;

    for ( const auto& pr : part )
      {
        result.push_back( pr.product );
      }
    return matrix_chain_multiply(
        result, false, stp::mc_multiply_method::dynamic_programming );
  }

  matrix run2()
  {
    std::vector<int> orders = matrix_chain_multiply_impl( mc ).get_order();
    assert( orders[ 0 ] == -1 && orders.back() == -2 );
    make_matrix_tree( orders );

    std::vector<std::vector<int>> levels = compute_levels();

    compute_root( levels );
    return mtree.get_node( mtree.root ).mtx;
  }

  void make_matrix_tree( const std::vector<int>& order )
  {
    std::vector<int> map( 2 * mc.size() - 1 );

    // make leaves node
    for ( int i = 0; i < mc.size(); i++ )
      {
        mtree.nodes.emplace_back( true );
        tNode& n = mtree.nodes.back();
        n.level = 0;
        n.dimensionality = std::make_pair( mc[ i ].rows(), mc[ i ].cols() );
        n.complexity = 0u;
        n.mtx = mc[ i ];
        map[ i ] = i;
      }

    // make nodes
    int matrix_num = mc.size();
    std::vector<int> v;

    for ( int i = 0; i < order.size(); ++i )
      {
        if ( order[ i ] == -1 )
          {
          }
        else if ( order[ i ] == -2 )
          {
            if ( v.size() == 1 ) continue;
            assert( v.size() >= 2 );
            int f1 = v[ v.size() - 2 ];
            int f2 = v[ v.size() - 1 ];
            std::vector<uint64_t> temp =
                complexity_analysis( mtree.get_m( f1 ), mtree.get_n( f1 ),
                                     mtree.get_m( f2 ), mtree.get_n( f2 ) );
            mtree.nodes.emplace_back( false, f1, f2 );
            tNode& n = mtree.nodes.back();
            n.dimensionality = std::make_pair( temp[ 1 ], temp[ 2 ] );
            n.complexity = temp[ 0 ];
            v.pop_back();
            v.back() = matrix_num;
            matrix_num++;
          }
        else
          {
            v.push_back( order[ i ] );
          }
      }
    assert( v.size() == 1 );
    // make root
    mtree.root = mtree.nodes.size() - 1;
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

  matrix compute_root( const std::vector<std::vector<int>>& levels )
  {
    assert( levels[ 0 ].size() == mc.size() );
    assert( levels.back().size() == 1 );

    // In order of complexity from smallest to largest.
    for ( int i = 1; i < levels.size(); i++ )
      {
        std::vector<int> nodes = levels[ i ];
        std::sort( nodes.begin(), nodes.end(), [&]( int n1, int n2 ) {
          int l1 = mtree.get_node( n1 ).complexity;
          int l2 = mtree.get_node( n2 ).complexity;
          return l1 < l2;
        } );

        std::vector<std::thread> threads;
        int block_size = nodes.size() > modified_num_threads
                             ? nodes.size()
                             : modified_num_threads;
        if ( nodes.size() > modified_num_threads )
          {
            uint64_t sum = 0u;
            for ( const int n : nodes ) sum += mtree.get_node( n ).complexity;
            uint64_t ave_complexity = sum / uint64_t( block_size );
            std::vector<std::vector<int>> nds;
            sum = 0u;
            std::vector<int> ns;
            for ( const int n : nodes )
              {
                sum += mtree.get_node( n ).complexity;
                ns.push_back( n );
                if ( sum >= ave_complexity )
                  {
                    nds.push_back( ns );
                    ns.clear();
                    sum = 0u;
                  }
              }
            if ( ns.size() != 0 )
              {
                nds.push_back( ns );
              }
            for ( const std::vector<int> ns : nds )
              {
                threads.emplace_back( [this, ns]() { compute_mtxs( ns ); } );
              }
          }
        else
          {
            for ( const int n : nodes )
              threads.emplace_back( [this, n]() { compute_mtx( n ); } );
          }

        for ( auto& thread : threads )
          {
            thread.join();
          }
      }

    return mtree.get_node( mtree.root ).mtx;
  }

  void compute_mtxs( const std::vector<int> ns )
  {
    for ( const int n : ns )
      {
        compute_mtx( n );
      }
  }

  void compute_mtx( const int n )
  {
    assert( n < mtree.nodes.size() );
    tNode& tn = mtree.nodes[ n ];
    int f1 = tn.fanin1;
    int f2 = tn.fanin2;
    matrix& m1 = mtree.get_node( f1 ).mtx;
    matrix& m2 = mtree.get_node( f2 ).mtx;

    assert( mtree.get_node( f1 ).is_matrix );
    assert( mtree.get_node( f2 ).is_matrix );

    tn.mtx = stp::semi_tensor_product( m1, m2 );
    tn.is_matrix = true;

    // clean no use matrix
    m1 = matrix();
    m2 = matrix();
    mtree.get_node( f1 ).is_matrix = false;
    mtree.get_node( f2 ).is_matrix = false;
  }

  std::vector<std::vector<int>> compute_levels()
  {
    compute_level( mtree.root );
    std::vector<std::vector<int>> nodes( mtree.get_node( mtree.root ).level +
                                         1 );
    // Nodes with the same level can multiply at the same time
    for ( int i = 0; i < mtree.nodes.size(); i++ )
      {
        nodes[ mtree.get_node( i ).level ].push_back( i );
      }
    return nodes;
  }

  int compute_level( const int n )
  {
    tNode& tn = mtree.nodes[ n ];
    if ( tn.level != -1 ) return tn.level;
    int level1 = compute_level( tn.fanin1 );
    int level2 = compute_level( tn.fanin2 );

    tn.level = std::max( level1, level2 ) + 1;
    return tn.level;
  }

  void report()
  {
    if ( use_dp )
      {
        std::cout << "[i] Threads partition based on dynamic programming\n";
      }
    else
      {
        std::cout << "[i] Simple threads partition.\n";
      }

    std::cout << "[i] Total time: " << to_millisecond( total_time ) << " ms.\n";
  }

 private:
  matrix_chain mc;
  std::mutex mtx;
  matrix_tree mtree;
  int num_threads;
  int max_num_threads;
  int modified_num_threads;
  bool use_dp;
  bool verbose;
  stopwatch<>::duration total_time;
};

matrix matrix_chain_multiply_by_multi_thread( const matrix_chain& mc,
                                              int num_threads,
                                              bool use_dp = false,
                                              bool verbose = false )
{
  subchain_multiply_impl p( mc, num_threads, use_dp, verbose );
  return p.run();
}
}  // namespace stp
