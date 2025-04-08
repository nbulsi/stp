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
  \file stp_logic_expr.hpp
  \brief header file for dag to normalize
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#pragma once

#include <Eigen/Dense>
#include <cmath>
#include <unordered_map>

#include "stp/stp_circuit.hpp"
#include "stp/stp_logic_expr.hpp"
#include "stp/stp_utils.hpp"

namespace stp
{
class stp_simulator
{
 public:
  using patterns = std::vector<uint8_t>;

 public:
  stp_simulator( stp_circuit& circuit, bool only_po = false )
      : circuit( circuit ),
        num_vars( circuit.get_inputs().size() ),
        only_po( only_po )
  {
    update_nodes();
  }

  // full sim, obtaining a truth table represented by a string
  std::string run()
  {
    init();
    get_need_node();
    sim();
    return get_tt();
  }

  void reset()
  {
    patterns_num = 0u;
    sim_info.clear();
    for ( uint32_t i = 0; i < circuit.get_nodes().size(); i++ )
      {
        const node& n = circuit.get_nodes()[ i ];
        assert( i == n.m_id );
        sim_info.emplace( n.m_id, patterns() );
        flag[ n.m_id ] = false;
      }
  }

  void add_pattern( const patterns& pattern )
  {
    assert( pattern.size() == num_vars );
    for ( uint32_t i = 0; i < num_vars; i++ )
      {
        sim_info[ i ].push_back( pattern[ i ] );
      }
    patterns_num++;
  }

 private:
  void update_nodes()
  {
    nodes.reserve( circuit.get_nodes().size() );

    for ( uint32_t i = 0; i < circuit.get_nodes().size(); i++ )
      {
        const node& n = circuit.get_nodes()[ i ];
        assert( i == n.m_id );
        nodes.push_back( n.m_id );
      }

    std::sort( nodes.begin(), nodes.end(), [&]( const id n1, const id n2 ) {
      return circuit.level( n1 ) < circuit.level( n2 );
    } );
  }

  void init()
  {
    reset();
    full_sim();
  }

  void full_sim()
  {
    patterns ps( num_vars, 0u );

    // the number of variables should be fewer than 32
    uint32_t num = 1 << num_vars;
    for ( uint32_t i = ( num - 1 ); i != 0u; i-- )
      {
        for ( uint32_t j = 0; j < num_vars; j++ )
          {
            ps[ num_vars - j - 1 ] = uint8_t( i >> ( num_vars - 1 - j ) & 1u );
          }
        add_pattern( ps );
      }
    add_pattern( patterns( num_vars, 0u ) );
  }

  bool had_sim( const id n ) { return sim_info[ n ].size() == patterns_num; }

  void get_need_node()
  {
    if ( !only_po )
      {
        need_sim_nodes = nodes;
        for ( const id n : nodes ) flag[ n ] = true;
        return;
      }

    need_sim_nodes.clear();

    for ( const id n : nodes )
      {
        const node& nd = circuit.get_node( n );
        if ( nd.is_po || nd.get_outputs().size() > 1 )
          {
            flag[ n ] = true;
            need_sim_nodes.push_back( n );
          }
        if ( nd.is_pi )
          {
            flag[ n ] = true;
          }
      }

    for ( const id n : need_sim_nodes )
      {
        cut_tree( n );
      }

    need_sim_nodes.clear();

    for ( const id n : nodes )
      {
        if ( flag[ n ] = true && !circuit.get_node( n ).is_pi )
          {
            need_sim_nodes.push_back( n );
          }
      }
  }

  void cut_tree( const id n )
  {
    std::deque<id> nds;
    nds.push_back( n );
    while ( !nds.empty() )
      {
        std::deque<id> temp_nodes;
        temp_nodes.push_back( nds.front() );
        nds.pop_front();
        int count = 0;
        while ( 1 )
          {
            for ( const node::edge& fin : circuit.get_node( n ).get_inputs() )
              {
                count++;
                if ( flag[ fin.index ] == false )
                  {
                    temp_nodes.push_back( fin.index );
                  }
              }
            temp_nodes.pop_front();
            count--;
            // two cases: is a big tree, part it into smaller one || already a
            // small tree
            if ( count > limit || temp_nodes.empty() )
              {
                break;
              }
          }
        for ( const id nd : temp_nodes )
          {
            flag[ nd ] = true;
            nds.push_back( nd );
          }
      }
  }

  void sim()
  {
    for ( const id n : need_sim_nodes )
      {
        assert( flag[ n ] );
        if ( had_sim( n ) ) continue;
        compute( n );
      }
  }

  void compute( const id n )
  {
    // get the matrix
    std::unordered_map<id, int> var_map;
    const matrix mtx = get_matrix( var_map, n );
    // parse the variables order
    std::vector<id> vars( var_map.size() );
    int var_num = vars.size();
    for ( const auto& t : var_map )
      {
        vars[ t.second - 1 ] = t.first;
      }

    // sim
    int bits = 1 << var_num;
    sim_info[ n ].reserve( patterns_num );
    for ( int count = 0; count < patterns_num; count++ )
      {
        uint32_t idx = 0;
        for ( const id var : vars )
          {
            idx = ( idx << 1 ) + sim_info[ var ][ count ];
          }
        sim_info[ n ].push_back( mtx( 0, bits - idx - 1 ) );
      }
  }

  matrix get_matrix( std::unordered_map<id, int>& var_map, const id n )
  {
    assert( flag[ n ] );
    std::deque<id> nds( 1, n );
    matrix_chain mc;
    mc.push_back( circuit.get_node( n ).get_mtx() );
    int temp;
    while ( !nds.empty() )
      {
        const id n = nds.front();
        nds.pop_front();
        for ( const auto& input : circuit.get_node( n ).get_inputs() )
          {
            const id fi = input.index;
            // var
            if ( flag[ fi ] = true )
              {
                // vars have not been collected
                if ( var_map.find( fi ) == var_map.end() )
                  {
                    temp = var_map.size() + 1;
                    var_map.emplace( fi, temp );
                  }
                else
                  {
                    temp = var_map[ fi ];
                  }
                matrix m( 2, 1 );
                m << temp, temp;
                mc.emplace_back( m );
              }
            else
              {
                mc.emplace_back( circuit.get_node( fi ).get_mtx() );
                nds.push_back( fi );
              }
          }
      }

    // canonical
    matrix m = stp::normalize_matrix( mc );
    return m;
  }

  std::string get_tt()
  {
    const id po = circuit.get_outputs()[ 0 ];
    std::string str = "";
    for ( const uint8_t val : sim_info[ po ] )
      {
        if ( val )
          str += "1";
        else
          str += "0";
      }
    return str;
  }

 public:
  void print_info()
  {
    for ( const auto& input_id : circuit.get_inputs() )
      {
        std::cout << circuit.get_node( input_id ).get_name() << " ";
      }
    std::cout << ": ";
    for ( const auto& output_id : circuit.get_outputs() )
      {
        std::cout << circuit.get_node( output_id ).get_name() << " ";
      }
    std::cout << std::endl;
    for ( int i = 0; i < patterns_num; i++ )
      {
        for ( const auto& input_id : circuit.get_inputs() )
          {
            std::cout << int( sim_info[ input_id ][ i ] ) << "  ";
          }
        std::cout << ":  ";
        for ( const auto& output_id : circuit.get_outputs() )
          {
            std::cout << int( sim_info[ output_id ][ i ] ) << " ";
          }
        std::cout << std::endl;
      }
  }

 private:
  // circuit info
  stp_circuit& circuit;
  uint32_t num_vars = 0u;

  // simulator info
  bool only_po;
  uint32_t limit = 6u;
  uint32_t patterns_num = 0u;
  std::unordered_map<id, patterns> sim_info;

  // supplementary information
  std::vector<id> nodes;           // ordered by levels in descending
  std::vector<id> need_sim_nodes;  // record the necessary simulation nodes
  std::unordered_map<id, bool>
      flag;  // the flag to indicate the node need be simulated or it is a PI
};

}  // namespace stp
