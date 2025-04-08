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
  \file stp_normalize.hpp
  \brief header file for various stp expression normalization
  \author Zhufei Chu
*/

#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <regex>

#include "stp/stp_eigen.hpp"
#include "stp/stp_utils.hpp"

using matrix = Eigen::MatrixXi;            // Defines the type of matrix to use
using matrix_chain = std::vector<matrix>;  // Defined matrix chain
using node_ids = std::vector<uint32_t>;
using expressions = std::vector<std::string>;

namespace stp
{
/*
 * Public class
 * */
class circuit_common_impl
{
 public:
  circuit_common_impl( const stp_circuit& circuit ) : circuit( circuit )
  {
    vars_order.clear();
    for ( const id& pi : circuit.get_inputs() )
      {
        num_vars++;
        vars_order[ pi ] = circuit.get_inputs().size() - num_vars + 1;
      }
  }

 public:
  node_ids get_circuit_ids()
  {
    node_ids topo;

    assert( circuit.get_outputs().size() == 1 );
    const id po = circuit.get_outputs()[ 0 ];

    get_chain_id( po, topo );

    return topo;
  }

  matrix_chain get_circuit_chain()
  {
    matrix_chain chain;
    auto nodes = get_circuit_ids();

    for ( const id& n : nodes )
      {
        chain.push_back( get_matrix( n ) );
      }
    return chain;
  }

  std::unordered_map<std::string, matrix> get_str_matrix_map()
  {
    std::unordered_map<std::string, matrix> str2mtx;
    auto nodes = get_circuit_ids();

    for ( const id& n : nodes )
      {
        if ( circuit.is_pi( n ) )
          {
            str2mtx[ circuit.get_node( n ).get_name() ] = get_matrix( n );
          }
        else
          {
            str2mtx[ "m" + std::to_string( n ) ] = get_matrix( n );
          }
      }

    return str2mtx;
  }

  expressions get_circuit_expr()
  {
    expressions expr;
    auto nodes = get_circuit_ids();

    for ( const auto& t : nodes )
      {
        if ( circuit.is_pi( t ) )
          {
            expr.push_back( circuit.get_node( t ).get_name() );
          }
        else
          {
            assert( t < circuit.get_nodes().size() );
            expr.push_back( "m" + std::to_string( t ) );
          }
      }

    return expr;
  }

 private:
  // get the chain expressed using node ids
  void get_chain_id( const id& n, node_ids& nodes )
  {
    nodes.push_back( n );

    if ( !circuit.is_pi( n ) )
      {
        for ( const auto& input : circuit.get_node( n ).get_inputs() )
          {
            get_chain_id( input.index, nodes );
          }
      }
    else
      {
        return;
      }
  }

  matrix get_matrix( const uint32_t n )
  {
    if ( circuit.is_pi( n ) )
      {
        uint32_t order = vars_order[ n ];
        matrix result( 2, 1 );
        result << order, order;
        return result;
      }

    return circuit.get_node( n ).get_mtx();
  }

 private:
  const stp_circuit& circuit;
  bool verbose;
  std::unordered_map<uint32_t, uint32_t> vars_order;
  unsigned num_vars = 0u;
};

node_ids get_circuit_ids( const stp_circuit& circuit )
{
  circuit_common_impl p( circuit );
  return p.get_circuit_ids();
}

matrix_chain get_circuit_chain( const stp_circuit& circuit )
{
  circuit_common_impl p( circuit );
  return p.get_circuit_chain();
}

std::unordered_map<std::string, matrix> get_circuit_str_matrix_map(
    const stp_circuit& circuit )
{
  circuit_common_impl p( circuit );
  return p.get_str_matrix_map();
}

expressions get_circuit_expr( const stp_circuit& circuit )
{
  circuit_common_impl p( circuit );
  return p.get_circuit_expr();
}

/*
 * Given an stp_circuit in DAG, we first extract matrix chain expressed in
 * vector, the class is used to normalize it by iterative sorting.
 *
 * The input matrix format:
 * 1000 1011 1 2
 * 0111 0100 1 2
 *
 * which corresponds to structural matrices and varaibles (with 2*1
 * dimensions, the first value is larger and equal than 1)
 * */

class stp_normalize_matrix_impl
{
 public:
  stp_normalize_matrix_impl( const stp_circuit& circuit, bool& verbose )
      : circuit( circuit ), verbose( verbose )
  {
    // initialization
    chain.clear();

    I2 = matrix::Zero( 2, 2 );
    I2 << 1, 0, 0, 1;

    PR2 = matrix::Zero( 4, 2 );
    PR2 << 1, 0, 0, 0, 0, 0, 0, 1;

    nodes = get_circuit_ids( circuit );
    chain = get_circuit_chain( circuit );
  }

  void run()
  {
    print_expr();

    auto new_chain = chain_normalize_method1();

    std::cout << "The final results: \n"
              << matrix_chain_multiply( legal_chain( new_chain ) ) << "\n";
  }

  matrix_chain chain_normalize_method1()
  {
    matrix_chain input_chain = chain;

    while ( !is_normal_chain( input_chain ) )
      {
        auto output_chain = reorder( input_chain );
        input_chain = output_chain;
        output_chain.clear();
      }

    return input_chain;
  }

  matrix_chain legal_chain( const matrix_chain& mc )
  {
    matrix_chain order;

    for ( const auto& m : mc )
      {
        if ( !is_variable( m ) )
          {
            order.push_back( m );
          }
      }

    return order;
  }

  bool is_normal_chain( const matrix_chain& mc )
  {
    for ( unsigned i = 0; i < mc.size() - 1u; i++ )
      {
        if ( is_variable( mc[ i ] ) && is_matrix( mc[ i + 1 ] ) )
          {
            return false;
          }
        else if ( is_variable( mc[ i ] ) && is_variable( mc[ i + 1 ] ) )
          {
            auto a = get_variable( mc[ i ] );
            auto b = get_variable( mc[ i + 1 ] );

            if ( a >= b )
              {
                return false;
              }
          }
      }

    return true;
  }

  matrix_chain reorder( const matrix_chain& mc )
  {
    matrix_chain order = mc;

    std::vector<unsigned> insert_idx;

    for ( unsigned i = 0; i < mc.size() - 1u; i++ )
      {
        // swap variable and matrix
        if ( is_variable( mc[ i ] ) && is_matrix( mc[ i + 1 ] ) )
          {
            // swap
            matrix temp = kronecker_product( I2, order[ i + 1 ] );
            order[ i + 1 ] = order[ i ];
            order[ i ] = temp;
          }
        // variables power reducing or swapping
        else if ( is_variable( mc[ i ] ) && is_variable( mc[ i + 1 ] ) )
          {
            auto a = get_variable( mc[ i ] );
            auto b = get_variable( mc[ i + 1 ] );

            if ( a == b )
              {
                order[ i ] = PR2;
              }
            else
              {
                // order the variables
                if ( a > b )
                  {
                    std::swap( order[ i ], order[ i + 1 ] );

                    insert_idx.push_back( i );
                  }
              }
          }
        else
          {
            continue;
          }
      }

    // insert the swap matrix in reverse order
    sort( insert_idx.rbegin(), insert_idx.rend() );
    matrix swap_matrix2 = generate_swap_matrix( 2, 2 );
    for ( const auto& j : insert_idx )
      {
        order.insert( order.begin() + j, swap_matrix2 );
      }

    return order;
  }

  bool is_variable( const matrix& m ) { return m.rows() == 2 && m.cols() == 1; }

  int get_variable( const matrix& m )
  {
    assert( is_variable( m ) );

    return m( 0, 0 );
  }

  // check whether it is a matrix
  bool is_matrix( const matrix& m ) { return m.cols() > 1; }

  void print_chain( const matrix_chain& mc )
  {
    for ( const auto& m : mc )
      {
        std::cout << m << "\n" << std::endl;
      }
  }

  void print_chain_ids()
  {
    assert( nodes.size() != 0u );
    for ( const auto n : nodes )
      {
        std::cout << " " << n;
      }
    std::cout << std::endl;
  }

  bool is_mtx( const uint32_t t )
  {
    if ( circuit.is_pi( t ) )
      {
        return false;
      }
    if ( t >= circuit.get_nodes().size() )
      {
        return false;
      }
    return true;
  }

  void print_expr()
  {
    std::cout << "[stp_expr]: ";
    for ( const uint32_t t : nodes )
      {
        if ( circuit.is_pi( t ) )
          {
            std::cout << circuit.get_node( t ).get_name() << " ";
          }
        else if ( is_mtx( t ) )
          {
            std::cout << "m" << t << " ";
          }
        else
          {
            std::cout << "UNKNOWN ";
          }
      }
    std::cout << "\n";
  }

 private:
  matrix_chain chain;
  bool verbose;
  const stp_circuit& circuit;
  node_ids nodes;
  matrix I2;
  matrix PR2;
};

/*
 * The above class deal with the matrix directly which may cause large memory,
 * we change the way to deal with the strings first.
 * */
class stp_normalize_string_impl
{
 public:
  stp_normalize_string_impl( const stp_circuit& circuit, bool& verbose )
      : circuit( circuit ), verbose( verbose )
  {
    str2mtx = get_circuit_str_matrix_map( circuit );

    if ( verbose )
      {
        for ( const auto& e : str2mtx )
          {
            std::cout << "map " << e.first << " \n" << e.second << std::endl;
          }
      }

    PR2 = matrix::Zero( 4, 2 );
    PR2 << 1, 0, 0, 0, 0, 0, 0, 1;

    W2 = generate_swap_matrix( 2, 2 );
  }

  void run()
  {
    auto expr = get_circuit_expr( circuit );
    unsigned start_index = 0;
    print_expr( expr );

    while ( !is_normal_expressions( expr, start_index ) )
      {
        reorder( expr, start_index );
        if ( verbose )
          {
            std::cout << "vec size: " << expr.size()
                      << " start index: " << start_index << std::endl;
          }
      }
    print_expr( expr );

    auto chain = expr_to_matrix_chain( expr );

    std::cout << "The final results: \n"
              << matrix_chain_multiply( chain ) << "\n";
  }

  bool is_matrix_string( const std::string& str )
  {
    if ( str[ 0 ] == 'm' || str[ 0 ] == 'I' || str[ 0 ] == 'P' ||
         str[ 0 ] == 'W' )
      {
        return true;
      }
    else
      {
        return false;
      }
  }

  unsigned get_variable_order( const std::string& str )
  {
    return str2mtx[ str ]( 0, 0 );
  }

  bool is_variable_string( const std::string& str )
  {
    return is_matrix_string( str ) == false;
  }

  bool is_normal_expressions( const expressions& expr, unsigned& start_index )
  {
    for ( unsigned i = 0; i < expr.size() - 1u; i++ )
      {
        if ( is_variable_string( expr[ i ] ) &&
             is_matrix_string( expr[ i + 1 ] ) )
          {
            start_index = i;
            return false;
          }
        else if ( is_variable_string( expr[ i ] ) &&
                  is_variable_string( expr[ i + 1 ] ) )
          {
            auto a = get_variable_order( expr[ i ] );
            auto b = get_variable_order( expr[ i + 1 ] );

            if ( a >= b )
              {
                start_index = i;
                return false;
              }
          }
        else
          {
            continue;
          }
      }

    start_index = 0;
    return true;
  }

  // Given a string like "I9m8", extract the digit after "I" and the remaining
  // string
  struct identity_strings
  {
    unsigned dimensions;
    std::string suffix;
  };

  identity_strings extract_num_and_suffix( const std::string& str )
  {
    std::regex pattern( R"(I(\d+)([A-Za-z0-9]+))" );
    std::smatch matches;

    if ( std::regex_search( str, matches, pattern ) )
      {
        identity_strings result;
        result.dimensions = std::stoi( matches[ 1 ].str() );
        result.suffix = matches[ 2 ].str();

        return result;
      }

    assert( false && "no matching found\n" );
  }

  void identity_string_processing( std::string& str )
  {
    if ( str[ 0 ] != 'I' )
      {
        str = "I1" + str;  // I1 means the identity matrix with dimensions 2^1
      }
    else
      {
        std::string number_string;
        std::string remaining_string;
        bool extract_complete = false;

        for ( unsigned i = 1u; i < str.size(); i++ )
          {
            if ( std::isdigit( str[ i ] ) && !extract_complete )
              {
                number_string += str[ i ];
              }
            else
              {
                extract_complete = true;
                remaining_string += str[ i ];
              }
          }

        assert( !number_string.empty() );
        auto number = std::stoi( number_string );
        auto current_number = ++number;

        str = "I" + std::to_string( current_number ) + remaining_string;
      }
  }

  void reorder( expressions& expr, const unsigned& start_index )
  {
    // recode the index of all operations
    std::vector<std::pair<unsigned, std::string>> to_add;
    std::vector<unsigned> to_delete;
    std::vector<unsigned> to_swap;

    for ( unsigned i = start_index; i < expr.size() - 1u; i++ )
      {
        if ( is_variable_string( expr[ i ] ) &&
             is_matrix_string( expr[ i + 1 ] ) )
          {
            identity_string_processing( expr[ i + 1 ] );
            to_swap.push_back( i );
            i++;
          }
        else if ( is_variable_string( expr[ i ] ) &&
                  is_variable_string( expr[ i + 1 ] ) )
          {
            if ( expr[ i ] == expr[ i + 1 ] )
              {
                to_delete.push_back( i );
                i++;
              }
            else
              {
                auto a = get_variable_order( expr[ i ] );
                auto b = get_variable_order( expr[ i + 1 ] );
                if ( a > b )
                  {
                    to_swap.push_back( i );
                    to_add.push_back( std::make_pair( i, "W2" ) );
                  }
              }
          }
        else
          {
            continue;
          }
      }

    // first swap
    for ( const auto& j : to_swap )
      {
        std::swap( expr[ j ], expr[ j + 1 ] );
      }
    // then insert PR2
    for ( const auto& j : to_delete )
      {
        expr[ j ] = "PR2";
      }
    // finally add
    std::sort( to_add.begin(), to_add.end(),
               []( const std::pair<unsigned, std::string>& a,
                   const std::pair<unsigned, std::string>& b ) {
                 return a.first > b.first;
               } );

    for ( const auto& j : to_add )
      {
        expr.insert( expr.begin() + j.first, j.second );
      }
  }

  unsigned fast_two_pow( unsigned base, unsigned exponent )
  {
    unsigned result = 1;
    while ( exponent > 0 )
      {
        if ( exponent % 2 == 1 )
          {
            result *= base;
          }

        base *= base;
        exponent /= 2;
      }

    return result;
  }

  // translate the expressions into matrix
  matrix_chain expr_to_matrix_chain( expressions& expr )
  {
    matrix_chain chain;

    for ( const auto& e : expr )
      {
        if ( e == "W2" )
          {
            chain.push_back( W2 );
          }
        else if ( e == "PR2" )
          {
            chain.push_back( PR2 );
          }
        else if ( e[ 0 ] == 'm' )
          {
            chain.push_back( str2mtx[ e ] );
          }
        else if ( e[ 0 ] == 'I' )
          {
            // IXmX style
            auto result = extract_num_and_suffix( e );
            auto dim = fast_two_pow( 2, result.dimensions );
            matrix identity_matrix;
            identity_matrix.setIdentity( dim, dim );

            matrix suffix_matrix;

            if ( result.suffix == "W2" )
              {
                suffix_matrix = W2;
              }
            else if ( result.suffix == "PR2" )
              {
                suffix_matrix = PR2;
              }
            else
              {
                assert( result.suffix[ 0 ] == 'm' );
                suffix_matrix = str2mtx[ result.suffix ];
              }

            chain.push_back(
                kronecker_product( identity_matrix, suffix_matrix ) );
          }
        else
          {
            if ( verbose )
              {
                std::cout << e << " is a primary input variable\n";
              }
          }
      }

    return chain;
  }

  void print_expr( const expressions& expr )
  {
    std::cout << "[i] expr: ";
    for ( const auto& e : expr )
      {
        std::cout << e << " ";
      }
    std::cout << std::endl;
  }

 private:
  const stp_circuit& circuit;
  bool verbose;
  node_ids nodes;
  std::unordered_map<std::string, matrix> str2mtx;
  matrix PR2;
  matrix W2;
};

void stp_normalize_string( const stp_circuit& circuit, bool verbose = false )
{
  stp_normalize_string_impl p( circuit, verbose );
  p.run();
}

void stp_normalize( const stp_circuit& circuit, bool verbose = false )
{
  stp_normalize_matrix_impl p( circuit, verbose );
  p.run();
}

}  // namespace stp
