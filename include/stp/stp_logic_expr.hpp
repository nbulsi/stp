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

#include <Eigen/Dense>
#include <cmath>

#include "stp/stp_eigen.hpp"
#include "stp/stp_utils.hpp"

using matrix = Eigen::MatrixXi;            // Defines the type of matrix to use
using matrix_chain = std::vector<matrix>;  // Defined matrix chain

namespace stp
{
class expr_normalize_impl
{
 public:
  expr_normalize_impl( const std::string& expr,
                       const std::vector<std::string>& input_names,
                       bool& verbose )
      : expr( expr ), input_names( input_names ), verbose( verbose )
  {
  }

  matrix_chain normalizing()
  {
    if ( is_valid_string() )
      {
        initialization();
        auto expr_ops = move_vars_to_rightside( all_tokens );
        if ( verbose )
          {
            std::cout << "[i] The strings after moving vars to the rightside: ";
            print_strings( expr_ops );
          }

        auto normal = sort_vars();
        if ( verbose )
          {
            std::cout << "[i] The strings after sorting vars:";
            print_strings( normal );
          }

        normal.insert( normal.begin(), expr_ops.begin(),
                       expr_ops.end() - num_vars_in_expr );

        if ( verbose )
          {
            std::cout << "[i] The normalized strings: ";
            print_strings( normal );
          }

        auto mc = expr_to_chain( normal );

        return mc;
      }
    else
      {
        std::cerr << "[e] Input is not a valid string.\n";
      }
  }

  matrix run()
  {
    auto mc = normalizing();
    return matrix_chain_multiply( mc );
  }

 private:
  bool is_operation( const std::string& word )
  {
    return word.substr( 0, 2 ) == "m_";
  }

  bool is_variable( const std::string& word )
  {
    return word.substr( 0, 2 ) == "x_";
  }

  bool is_valid_string()
  {
    std::istringstream iss( expr );
    std::string word;

    while ( iss >> word )
      {
        if ( ( !is_operation( word ) && !is_variable( word ) ) ||
             ( word.length() <= 2 ) ||
             ( is_operation( word ) && word.length() > 3 ) )
          {
            return false;
          }
      }

    return true;
  }

  void initialization()
  {
    Mr = matrix::Zero( 4, 2 );
    Mr << 1, 0, 0, 0, 0, 0, 0, 1;

    Mc = matrix::Zero( 2, 4 );
    Mc << 1, 0, 0, 0, 0, 1, 1, 1;

    Md = matrix::Zero( 2, 4 );
    Md << 1, 1, 1, 0, 0, 0, 0, 1;

    Mi = matrix::Zero( 2, 4 );
    Mi << 1, 0, 1, 1, 0, 1, 0, 0;

    Me = matrix::Zero( 2, 4 );
    Me << 1, 0, 0, 1, 0, 1, 1, 0;

    Mp = matrix::Zero( 2, 4 );
    Mp << 0, 1, 1, 0, 1, 0, 0, 1;

    Mt = matrix::Zero( 2, 4 );
    Mt << 0, 1, 1, 1, 1, 0, 0, 0;

    Mb = matrix::Zero( 2, 4 );
    Mb << 0, 0, 0, 1, 1, 1, 1, 0;

    Mn = matrix::Zero( 2, 2 );
    Mn << 0, 1, 1, 0;

    all_tokens = parse_tokens( expr, "" );
    auto x_tokens = parse_tokens( expr, "x_" );
    num_vars_in_expr = x_tokens.size();
  }

  matrix get_variable_matrix( const std::string& var )
  {
    for ( int i = 0; i < input_names.size(); i++ )
      {
        if ( input_names[ i ] == var )
          {
            matrix mv( 2, 1 );
            mv << i + 1, i + 1;
            return mv;
          }
      }

    assert( false );
  }

  int get_identity_dim( const std::string& token )
  {
    int number;
    size_t digit_start = token.find_first_of( "0123456789" );

    if ( digit_start != std::string::npos )
      {
        std::string number_part = token.substr( digit_start );
        number = std::stoi( number_part );
      }
    else
      {
        assert( false && "No number found." );
      }

    return number;
  }

  matrix_chain expr_to_chain( const std::vector<std::string>& normal )
  {
    // first, get the inital chain
    for ( const auto& token : normal )
      {
        if ( token == "m_c" )
          {
            chain.push_back( Mc );
          }
        else if ( token == "m_d" )
          {
            chain.push_back( Md );
          }
        else if ( token == "m_n" )
          {
            chain.push_back( Mn );
          }
        else if ( token == "m_i" )
          {
            chain.push_back( Mi );
          }
        else if ( token == "m_e" )
          {
            chain.push_back( Me );
          }
        else if ( token == "m_p" )
          {
            chain.push_back( Mp );
          }
        else if ( token == "m_t" )
          {
            chain.push_back( Mt );
          }
        else if ( token == "m_b" )
          {
            chain.push_back( Mb );
          }
        else if ( token.substr( 0, 1 ) == "I" )  // identity
          {
            auto dim = get_identity_dim( token );

            matrix identity_matrix;
            identity_matrix.setIdentity( dim, dim );

            chain.push_back( identity_matrix );
          }
        else if ( token == "W2" )
          {
            chain.push_back( generate_swap_matrix( 2, 2 ) );
          }
        else if ( token == "PR2" )
          {
            chain.push_back( Mr );
          }
        else  // variable
          {
            assert( is_variable( token ) );
          }
      }

    // for the identity matrix, we should first calculate the kronecker
    // product
    matrix_chain new_chain;
    for ( int i = 0; i < normal.size() - input_names.size(); i++ )
      {
        if ( normal[ i ].substr( 0, 1 ) == "I" )
          {
            new_chain.push_back(
                kronecker_product( chain[ i ], chain[ i + 1 ] ) );
            i++;
          }
        else
          {
            new_chain.push_back( chain[ i ] );
          }
      }

    return new_chain;
  }

  void print_chain()
  {
    assert( chain.size() != 0 );

    auto mat_to_var = [&]( const matrix& mat ) {
      return input_names[ mat( 0, 0 ) - 1 ];
    };

    for ( const auto& m : chain )
      {
        if ( m.rows() == 2 && m.cols() == 1 )
          {
            std::cout << mat_to_var( m ) << std::endl;
          }
        else
          {
            std::cout << m << std::endl << std::endl;
          }
      }
  }

  int get_the_number_vars_before_operation(
      const std::vector<std::string>& inputs, int op_idx )
  {
    int count = 0;
    for ( int i = 0; i < op_idx; i++ )
      {
        if ( is_variable( inputs[ i ] ) )
          {
            count++;
          }
      }
    return count;
  }

  std::vector<std::string> vars_power_reducing(
      const std::vector<std::string>& inputs )
  {
    std::vector<std::string> new_expr;
    std::vector<std::string> temp_vars;

    for ( int i = 0; i < inputs.size(); i++ )
      {
        if ( !is_variable( inputs[ i ] ) )
          {
            new_expr.push_back( inputs[ i ] );
          }
        else
          {
            if ( temp_vars.size() > 0 &&
                 inputs[ i ] == temp_vars.back() )  // power reducing
              {
                temp_vars.insert( temp_vars.end() - 1, "PR2" );
              }
            else
              {
                temp_vars.push_back( inputs[ i ] );
              }
          }
      }

    new_expr.insert( new_expr.end(), temp_vars.begin(), temp_vars.end() );
    return new_expr;
  }

  std::vector<std::string> move_vars_to_rightside(
      const std::vector<std::string>& inputs )
  {
    std::vector<std::string> new_expr;
    std::vector<std::string> temp_vars;

    for ( int i = 0; i < inputs.size(); i++ )
      {
        if ( !is_variable( inputs[ i ] ) )
          {
            auto count = get_the_number_vars_before_operation( inputs, i );

            if ( count == 0 )
              {
                new_expr.push_back( inputs[ i ] );
              }
            else
              {
                int dim = std::pow( 2, count );
                new_expr.push_back( "I" + std::to_string( dim ) );
                new_expr.push_back( inputs[ i ] );
              }
          }
        else
          {
            temp_vars.push_back( inputs[ i ] );
          }
      }

    new_expr.insert( new_expr.end(), temp_vars.begin(), temp_vars.end() );
    return new_expr;
  }

  bool variable_compare( const std::string& s1, const std::string& s2 )
  {
    auto it1 = std::find( input_names.begin(), input_names.end(), s1 );
    auto it2 = std::find( input_names.begin(), input_names.end(), s2 );

    if ( it1 != input_names.end() && it2 != input_names.end() )
      {
        return std::distance( input_names.begin(), it1 ) <
               std::distance( input_names.begin(), it2 );
      }

    assert( false );
    return s1 < s2;  // lexicographical order
  }

  std::vector<std::string> swap_vars( const std::vector<std::string>& inputs,
                                      int idx )
  {
    auto result = inputs;
    bool find_var = false;
    int idx_var;

    for ( auto i = 0; i < inputs.size(); i++ )
      {
        if ( is_variable( inputs[ i ] ) )
          {
            idx_var = i;
            find_var = true;
            break;
          }
      }

    assert( find_var );
    std::swap( result[ idx_var + idx ], result[ idx_var + idx + 1 ] );
    result.insert( result.begin() + idx_var + idx, "W2" );
    return result;
  }

  bool validate( const std::vector<std::string>& inputs )
  {
    for ( int i = 0; i < inputs.size(); i++ )
      {
        if ( !is_variable( inputs[ i ] ) )
          {
            if ( get_the_number_vars_before_operation( inputs, i ) != 0 )
              {
                return false;
              }
          }
      }

    return true;
  }

  std::vector<std::string> sort_vars()
  {
    auto x_tokens = parse_tokens( expr, "x_" );
    auto result = x_tokens;

    for ( int i = 1; i < x_tokens.size(); ++i )
      {
        std::string key = x_tokens[ i ];
        int j = i - 1;

        while ( j >= 0 && variable_compare( key, x_tokens[ j ] ) )
          {
            auto current_result = result;
            auto vars_right_result = move_vars_to_rightside( current_result );

            result = swap_vars( vars_right_result, j );

            if ( verbose )
              {
                std::cout << "[step] Swap: " << x_tokens[ j + 1 ] << " and "
                          << x_tokens[ j ] << std::endl;
                std::cout << "[step] Current expr: ";
                print_strings( result );
              }

            std::swap( x_tokens[ j ], x_tokens[ j + 1 ] );
            --j;
          }
      }


    private:
      std::string expr;
      std::vector<std::string> all_tokens;
      std::vector<std::string> input_names; //the name of variables
      bool verbose;
      int num_vars_in_expr;
      matrix_chain chain;
      matrix Mr; //power reducing 
      matrix Mc; //conjunctive, AND
      matrix Md; //disjunctive, OR
      matrix Mi; //implication, IMPLY
      matrix Me; //equivalence, XNOR
      matrix Mp; //XOR
      matrix Mt; //NAND
      matrix Mb; //NOR
      matrix Mn; //not
  };

  /****************************************************************************************************************
   * The following code is implemented by Ruibing Zhang, another method for
   * expression normalization, the input is an assignment clause used in verilog.
   * Example: (a & b) | (a & ~c) | (~b & ~c)
   * ************************************************************************************************************/

  inline bool is_variable( const matrix& mat )
  {
    return mat.rows() == 2 && mat.cols() == 1;
  }

  inline int get_variable( const matrix& mat )
  {
    if( is_variable( mat ) )
    {
      return mat( 0, 0 );
    }
  else
    {
      return 0;
    }
}


  inline std::vector<std::string> matrix_split( const std::string& input, const std::string& pred )
  {
    std::vector<std::string> result;
    std::string temp{ "" };

  unsigned count1 = input.size();
  unsigned count2 = pred.size();
  unsigned j;

  for ( size_t i = 0; i < count1; i++ )
    {
      for ( j = 0; j < count2; j++ )
        {
          if ( input[ i ] == pred[ j ] )
            {
              break;
            }
        }

      if ( j == count2 )
        {
          temp += input[ i ];
        }
      else
        {
          if ( !temp.empty() )
            {
              result.push_back( temp );
              temp.clear();
            }
        }
    }

  if ( !temp.empty() )
    {
      result.push_back( temp );
      temp.clear();
    }

  return result;
}


  inline matrix normalize_matrix( matrix_chain mc )
  {
    matrix Mr( 4, 2 ); // Reduced power matrix
    Mr << 1, 0, 0, 0, 0, 0, 0, 1;

  matrix I2( 2, 2 );  // Identity matrix
  I2 << 1, 0, 0, 1;

  matrix normal_matrix;
  int p_variable;
  int p;

  int max = 0;  // the max is the number of variable

  for ( int i = 0; i < mc.size(); i++ )
    {
      if ( mc[ i ]( 0, 0 ) > max )
        {
          max = mc[ i ]( 0, 0 );
        }
    }

  std::vector<int> idx( max + 1 );  // id[0] is the max of idx
  p_variable = mc.size() - 1;

  while ( p_variable >= 0 )
    {
      bool find_variable = false;
      matrix& matrix = mc[ p_variable ];
      int var = get_variable( matrix );

      if ( var != 0 )  // 1:find a variable
        {
          if ( idx[ var ] ==
               0 )  // the variable appears for the first time ：end : not_end
            {
              idx[ var ] = idx[ 0 ] + 1;
              idx[ 0 ]++;

              if ( p_variable ==
                   mc.size() - 1 )  // the variable shows in the end
                {
                  mc.pop_back();
                  p_variable--;
                  continue;
                }
            }
          else  // the variable appears for the not first time
            {
              if ( idx[ var ] == idx[ 0 ] )
                {
                  find_variable = true;
                }
              else
                {
                  find_variable = true;
                  mc.push_back( generate_swap_matrix(
                      2, 1 << ( idx[ 0 ] - idx[ var ] ) ) );

                  for ( int i = 1; i <= max; i++ )
                    {
                      if ( idx[ i ] != 0 && idx[ i ] > idx[ var ] ) idx[ i ]--;
                    }

                  idx[ var ] = idx[ 0 ];
                }
            }

          matrix_chain mc_temp;
          mc_temp.clear();

          for ( p = p_variable + 1; p < mc.size(); p++ )
            {
              mc_temp.push_back( mc[ p ] );
            }

          while ( p > p_variable + 1 )
            {
              mc.pop_back();
              p--;
            }

          if ( mc_temp.size() > 0 )
            {
              mc.push_back( matrix_chain_multiply( mc_temp ) );
            }

          if ( p_variable != mc.size() - 1 )
            {
              mc[ p_variable ] = kronecker_product( I2, mc[ p_variable + 1 ] );
              mc.pop_back();
            }

          if ( find_variable )
            {
              mc.push_back( Mr );
            }
          continue;
        }
      else
        {
          p_variable--;
        }
    }

  for ( int i = max; i > 0; i-- )
    {
      mc.push_back( generate_swap_matrix( 2, pow( 2, idx[ 0 ] - idx[ i ] ) ) );

      for ( int j = 1; j <= max; j++ )
        {
          if ( ( idx[ j ] != 0 ) && ( idx[ j ] > idx[ i ] ) )
            {
              idx[ j ]--;
            }
        }

      idx[ i ] = max;
    }

  normal_matrix = matrix_chain_multiply( mc );
  return normal_matrix;
}

  // example:             (a & b) | (a & ~c) | (~b & ~c)                                 
  //                      a b c
  inline matrix from_exp_to_nmx( const std::string& expression, 
                          const std::vector<std::string>& input_names, 
                          bool print = false )
  {
    std::vector<std::string> equation;
    equation.push_back( "(" );
    std::string temp = "";

  for ( int i = 0; i < expression.size(); i++ )
    {
      if ( expression[ i ] == ' ' )
        {
          continue;
        }
      else if ( expression[ i ] == '(' || expression[ i ] == ')' ||
                expression[ i ] == '|' || expression[ i ] == '&' ||
                expression[ i ] == '~' )
        {
          if ( !temp.empty() )
            {
              equation.push_back( temp );
            }

          std::string p1 = "";
          p1 = p1 + expression[ i ];
          equation.push_back( p1 );
          temp = "";
        }
      else
        {
          temp = temp + expression[ i ];
        }
    }
  equation.push_back( ")" );

  // equation = ["(",   "(", "a", "&", "b", ")",   "(", "a", "&", "~", "c", ")",
  // "(", "~", "a", "&", "~", "c", ")"   ")"]

  /***********/
  // deal with "~"
  for ( int i = 0; i < equation.size(); i++ )
    {
      if ( equation[ i ] == "~" )
        {
          equation[ i ] = "~(" + equation[ i + 1 ] + ")";
          equation.erase( equation.begin() + i + 1 );
        }
      else if ( equation[ i ] == "&" || equation[ i ] == "|" ||
                equation[ i ] == "(" || equation[ i ] == ")" )
        {
          continue;
        }
      else
        {
          equation[ i ] = "(" + equation[ i ] + ")";
        }
    }

  // equation = ["(",   "(", "(a)", "&", "(b)", ")",   "(", "(a)", "&", "~(c)",
  // ")",  "(", "~(a)", "&", "~(c)", ")"   ")"]
  std::vector<int> left_bracket;
  for ( int i = 0; i < equation.size(); i++ )
    {
      if ( equation[ i ] == "(" )
        {
          left_bracket.push_back( i );
        }
      if ( equation[ i ] == ")" )
        {
          std::string equ = "";
          for ( int j = left_bracket[ left_bracket.size() - 1 ] + 1; j < i;
                j++ )  // 遍历左右括号之间的表达式
            {
              if ( equation[ j ] == "|" || equation[ j ] == "&" )
                {
                  equ = "(" + equation[ j ] + ")" + equ;
                }
              else
                {
                  equ = equ + "(" + equation[ j ] + ")";
                }
            }

          //"(" -> equ
          equation[ left_bracket[ left_bracket.size() - 1 ] ] = equ;
          equation.erase(
              equation.begin() + left_bracket[ left_bracket.size() - 1 ] + 1,
              equation.begin() + i + 1 );
          i = left_bracket[ left_bracket.size() - 1 ] - 1;
          left_bracket.pop_back();
        }
    }

  std::string equ = equation[ 0 ];
  equation.clear();
  equation = matrix_split( equ, "()" );
  matrix_chain mc;

  Eigen::MatrixXi Mc( 2, 4 );
  Mc << 1, 0, 0, 0, 0, 1, 1, 1;

  Eigen::MatrixXi Md( 2, 4 );
  Md << 1, 1, 1, 0, 0, 0, 0, 1;

  Eigen::MatrixXi Mn( 2, 2 );
  Mn << 0, 1, 1, 0;

  for ( int i = 0; i < equation.size(); i++ )
    {
      if ( equation[ i ] == "&" )
        {
          mc.push_back( Mc );
        }
      else if ( equation[ i ] == "|" )
        {
          mc.push_back( Md );
        }
      else if ( equation[ i ] == "~" )
        {
          mc.push_back( Mn );
        }
      else if ( equation[ i ] == "" || equation[ i ] == " " )
        {
          continue;
        }
      else  // variable
        {
          for ( int j = 0; j < input_names.size(); j++ )
            {
              if ( equation[ i ] == input_names[ j ] )
                {
                  Eigen::MatrixXi M( 2, 1 );
                  M << j + 1, j + 1;
                  mc.push_back( M );
                  break;
                }
            }
        }
    }
  // print matrix_chain
  if ( print )
    {
      std::cout << "Print\n";
      auto mat_to_var = [&]( const matrix& mat ) {
        return input_names[ mat( 0, 0 ) - 1 ];
      };
      for ( const auto& mat : mc )
        {
          if ( is_variable( mat ) )
            {
              std::cout << mat_to_var( mat ) << std::endl;
            }
          else
            {
              std::cout << mat << std::endl;
            }
        }
    }

    matrix p = normalize_matrix( mc );
    return p;
  }
   /*************************************************************************************************************/
  
  inline matrix expr_normalize( const std::string& expr, const std::vector<std::string>& input_names, bool verbose = false )
  {
    expr_normalize_impl p( expr, input_names, verbose );
    auto mat = p.run();


matrix expr_normalize( const std::string& expr,
                       const std::vector<std::string>& input_names,
                       bool verbose = false )
{
  expr_normalize_impl p( expr, input_names, verbose );
  auto mat = p.run();

  if ( verbose )
    {
      std::cout << "[i] The matrix is " << std::endl << mat << std::endl;
    }

    return mat;
  }
  
  inline matrix_chain expr_normalize_to_chain( const std::string& expr, const std::vector<std::string>& input_names, bool verbose = false )
  {
    expr_normalize_impl p( expr, input_names, verbose );
    auto mc = p.normalizing();

  if ( verbose )
    {
      std::cout << "[i] The matrix chain size is " << mc.size() << std::endl;
    }

  return mc;
}

void test()
{
  std::string input = "m_d m_c x_1 x_2 m_d m_c x_1 m_n x_3 m_c m_n x_2 m_n x_3";
  std::vector<std::string> input_names{"x_1", "x_2", "x_3"};
  std::cout << expr_normalize( input, input_names, true ) << std::endl;
}

}  // namespace stp
