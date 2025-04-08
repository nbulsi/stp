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
  \file stp_circuit.hpp
  \brief header file for some basic STP operations, the code is inspired from
  mockturtle
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#include <cassert>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "stp_eigen.hpp"

#pragma once

using id = uint32_t;

namespace stp
{
class node
{
 public:
  struct edge
  {
    edge()
    {
      is_complemented = 0u;
      index = 0u;
    }

    edge( uint32_t t )
    {
      is_complemented = t >> 63;
      index = t & ( 1 << 63 );
    }

    edge( uint32_t t1, uint32_t t2 )
    {
      is_complemented = t1;
      index = t2;
    }

    unsigned is_complemented : 1;
    unsigned index : 31;
  };

 public:
  node() {}

  const std::string &get_name() const { return m_name; }
  std::string &name() { return m_name; }

  const std::string &get_tt() const { return m_tt; }
  std::string &tt() { return m_tt; }

  const matrix &get_mtx() const { return m_mtx; }
  matrix &mtx() { return m_mtx; }

  const std::vector<edge> &get_inputs() const { return m_inputs; }
  std::vector<edge> &inputs() { return m_inputs; }

  const std::vector<edge> &get_outputs() const { return m_outputs; }
  std::vector<edge> &outputs() { return m_outputs; }

 public:
  bool is_pi{false};
  bool is_po{false};
  id m_id;

 private:
  std::string m_name;
  std::string m_tt;
  matrix m_mtx;
  std::vector<edge> m_inputs;
  std::vector<edge> m_outputs;
};

class stp_circuit
{
 public:
  stp_circuit()
  {
    m_nodes.reserve( 200 );
    m_inputs.reserve( 20 );
    m_outputs.reserve( 20 );
  }

#pragma region create Primary I / O and node
  uint32_t create_pi( const std::string &name )
  {
    uint32_t index = ensure_node( name );
    node &n = m_nodes[ index ];
    n.is_pi = true;
    m_inputs.push_back( index );
    return index;
  }

  uint32_t create_po( const std::string &name )
  {
    uint32_t index = ensure_node( name );
    node &n = m_nodes[ index ];
    n.is_po = true;
    m_outputs.push_back( index );
    return index;
  }

  uint32_t create_node( const std::string &tt,
                        const std::vector<std::string> &input_names,
                        const std::string &output_name )
  {
    std::vector<uint32_t> inputs;
    for ( const std::string &name : input_names )
      {
        inputs.push_back( ensure_node( name ) );
      }

    uint32_t output = ensure_node( output_name );
    node &n = m_nodes[ output ];
    n.tt() = tt;
    n.mtx() = get_stp_matrix( tt, inputs.size() );

    for ( int i = inputs.size() - 1; i >= 0; i-- )
      {
        auto input = inputs[ i ];
        n.inputs().emplace_back( 0u, input );
        m_nodes[ input ].outputs().emplace_back( 0u, output );
      }

    return output;
  }
#pragma endregion

#pragma region visit
  void init_visit()
  {
    m_visit = 0u;
    m_visits.resize( m_nodes.size(), 0u );
  }

  void clear_visited() { m_visits.resize( m_nodes.size(), 0u ); }

  void set_visited( const id n ) { m_visits[ n ] = m_visit; }

  void incr_trav_id() { m_visit++; }

  bool had_visited( const id n ) { return m_visits[ n ] == m_visit; }
#pragma endregion

#pragma region get circuit information
  bool is_pi( const id n ) const
  {
    if ( n >= m_nodes.size() )
      {
        return false;
      }
    return m_nodes[ n ].is_pi;
  }

  bool is_po( const id n ) const
  {
    if ( n >= m_nodes.size() )
      {
        return false;
      }
    return m_nodes[ n ].is_po;
  }

  const node &get_node( const id n ) const { return m_nodes[ n ]; }

  const std::vector<node> &nodes() const { return m_nodes; }

  std::vector<node> &nodes() { return m_nodes; }

  const std::vector<node> &get_nodes() const { return m_nodes; }

  std::vector<id> &inputs() { return m_inputs; }

  const std::vector<id> &get_inputs() const { return m_inputs; }

  std::vector<id> &outputs() { return m_outputs; }

  const std::vector<id> &get_outputs() const { return m_outputs; }

#pragma endregion

#pragma region compute level
  int level( const id n ) { return m_levels[ n ]; }

  void update_levels()
  {
    for ( const node &n : m_nodes ) m_levels[ n.m_id ] = -1;
    for ( const id po : m_outputs ) compute_level( po );
  }

  int compute_level( const id n )
  {
    if ( m_levels[ n ] != -1 ) return m_levels[ n ];

    int level = -1;

    const node &nd = m_nodes[ n ];

    for ( const node::edge &e : nd.get_inputs() )
      {
        int lev = compute_level( e.index );
        if ( level < lev )
          {
            level = lev;
          }
      }

    m_levels[ n ] = level + 1;
    return m_levels[ n ];
  }
#pragma endregion

 private:
  uint32_t ensure_node( const std::string &name )
  {
    auto it = from_name_to_index.find( name );
    if ( it != from_name_to_index.end() )
      {
        return it->second;
      }
    uint32_t index = m_nodes.size();
    m_nodes.emplace_back();
    node &n = m_nodes.back();
    n.m_id = m_nodes.size() - 1;
    n.name() = name;
    from_name_to_index[ name ] = index;
    return index;
  }

  matrix get_stp_matrix( const std::string &tt, const int &inputs_num )
  {
    assert( inputs_num > 0 );
    int tt_length = 1 << inputs_num;
    matrix result( 2, tt_length );
    uint32_t idx = 0;
    std::string str = "";
    // 0x???
    for ( int i = 2; i < tt.size(); i++ )
      {
        str += hex2bin( tt[ i ] );
      }
    for ( int i = str.size() - tt_length; i < str.size(); i++ )
      {
        char t = str[ i ];
        if ( t == '0' )
          {
            result( 0, idx ) = 0;
            result( 1, idx ) = 1;
          }
        else
          {
            result( 0, idx ) = 1;
            result( 1, idx ) = 0;
          }
        idx++;
      }
    assert( idx == tt_length );
    return result;
  }

  std::string hex2bin( const char c )
  {
    if ( c == '0' ) return "0000";
    if ( c == '1' ) return "0001";
    if ( c == '2' ) return "0010";
    if ( c == '3' ) return "0011";
    if ( c == '4' ) return "0100";
    if ( c == '5' ) return "0101";
    if ( c == '6' ) return "0110";
    if ( c == '7' ) return "0111";
    if ( c == '8' ) return "1000";
    if ( c == '9' ) return "1001";
    if ( c == 'a' || c == 'A' ) return "1010";
    if ( c == 'b' || c == 'B' ) return "1011";
    if ( c == 'c' || c == 'C' ) return "1100";
    if ( c == 'd' || c == 'D' ) return "1101";
    if ( c == 'e' || c == 'E' ) return "1110";
    if ( c == 'f' || c == 'F' ) return "1111";
    assert( false );
    return "";
  }

  void set_id()
  {
    int i = 0;
    for ( node &n : m_nodes )
      {
        n.m_id = i;
        i++;
      }
  }

 public:
  void print_circuit()
  {
    for ( const uint32_t input : m_inputs )
      {
        std::cout << "INPUT(" << m_nodes[ input ].get_name() << ")\n";
      }

    for ( const uint32_t output : m_outputs )
      {
        std::cout << "OUTPUT(" << m_nodes[ output ].get_name() << ")\n";
      }

    uint32_t size = m_nodes.size();
    std::string str = " ";
    for ( const node &n : m_nodes )
      {
        if ( n.is_pi ) continue;
        std::cout << n.get_name() << str << "= LUT " << n.get_tt() << "(";
        std::string s = "";
        for ( int i = n.get_inputs().size() - 1; i >= 0; i-- )
          {
            auto input = n.get_inputs()[ i ];
            s += ( m_nodes[ input.index ].get_name() + ", " );
          }
        s.pop_back();
        s.pop_back();
        s += ")";
        std::cout << s << "\n";
      }
  }

 private:
  std::vector<node> m_nodes;
  std::vector<id> m_inputs;
  std::vector<id> m_outputs;

  uint32_t m_depth{0u};

  uint32_t m_visit{0u};
  std::vector<uint32_t> m_visits;

  std::unordered_map<std::string, id> from_name_to_index;

  std::unordered_map<id, int> m_levels;
};
}  // namespace stp
