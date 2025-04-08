#pragma once

#include <Eigen/Dense>

#include <bitset>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using matrix = Eigen::MatrixXi;  // Defines the type of matrix to use

namespace stp
{
inline unsigned get_lcm( unsigned m, unsigned n )
{
  return ( m * n ) / std::gcd( m, n );
}

inline void print_strings( const std::vector<std::string>& inputs )
{
  for ( auto s : inputs )
    {
      std::cout << " " << s << " ";
    }
  std::cout << std::endl;
}

inline std::vector<std::string> parse_tokens( const std::string& input,
                                              const std::string& prefix )
{
  std::istringstream iss( input );
  std::string token;
  std::vector<std::string> result;

  while ( iss >> token )
    {
      if ( token.compare( 0, prefix.size(), prefix ) == 0 )
        {
          result.push_back( token );
        }
    }

  return result;
}

inline void print_binary( const matrix& mat, std::ostream& os = std::cout )
{
  for ( auto i = 0; i < mat.cols(); i++ )
    {
      os << mat( 0, i );
    }
}

inline std::string to_binary( const matrix& mat )
{
  std::stringstream st;
  print_binary( mat, st );
  return st.str();
}

inline void print_hex( const std::string& binary_string,
                       std::ostream& os = std::cout )
{
  assert( binary_string.length() % 4 == 0 );

  for ( size_t i = 0; i < binary_string.length(); i += 4 )
    {
      std::string block = binary_string.substr( i, 4 );

      int decimal_value = std::bitset<4>( block ).to_ulong();
      char hex_digit;

      if ( decimal_value < 10 )
        {
          hex_digit = '0' + decimal_value;
        }
      else
        {
          hex_digit = 'A' + ( decimal_value - 10 );
        }

      os << hex_digit;
    }
}

inline std::string to_hex( const matrix& mat )
{
  std::stringstream st;
  print_hex( to_binary( mat ), st );
  return st.str();
}

inline std::vector<std::string> str_split( const std::string& input,
                                           const std::string& pred )
{
  std::vector<std::string> result;
  std::string temp{""};
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
        temp += input[ i ];
      else
        {
          if ( !temp.empty() )
            {
              result.push_back( temp );
              temp.clear();
            }
        }
    }
  return result;
}
}  // namespace stp
