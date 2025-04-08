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
  \brief header file for stp based logic expression
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#pragma once

#include "stp_circuit.hpp"
#include "stp_utils.hpp"

using namespace stp;

class bench_reader
{
 public:
  bool parse( std::istream& is, stp_circuit& circuit )
  {
    const std::string flag_input = "INPUT";
    const std::string flag_output = "OUTPUT";
    const std::string flag_lut = "LUT";
    const std::string flag_gnd = "gnd";
    for ( std::string line; std::getline( is, line, '\n' ); )
      {
        if ( line.empty() )
          {
            continue;
          }
        if ( line.find( flag_input ) != std::string::npos )
          {
            match_input( circuit, line );
            continue;
          }
        if ( line.find( flag_output ) != std::string::npos )
          {
            match_output( circuit, line );
            continue;
          }
        if ( line.find( flag_lut ) != std::string::npos )
          {
            match_gate( circuit, line );
            continue;
          }
      }
    return true;
  }

 private:
  bool match_input( stp_circuit& circuit, const std::string& line )
  {
    std::string input_name = str_split( line, "( )" )[ 1 ];
    uint32_t index = circuit.create_pi( input_name );
    return true;
  }

  bool match_output( stp_circuit& circuit, const std::string& line )
  {
    std::string output_name = str_split( line, "( )" )[ 1 ];
    uint32_t index = circuit.create_po( output_name );
    return true;
  }

  bool match_gate( stp_circuit& circuit, const std::string& line )
  {
    std::vector<std::string> gate = str_split( line, ",=( )" );
    std::string output = gate[ 0 ];
    std::string tt = gate[ 2 ];
    gate.erase( gate.begin(), gate.begin() + 3 );
    std::vector<std::string> inputs( gate );
    circuit.create_node( tt, inputs, output );
    return true;
  }
};
