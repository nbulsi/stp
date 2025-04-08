#include <catch.hpp>

#include <stp/stp_utils.hpp>

using namespace stp;

TEST_CASE( "parse stp matrix chain", "[utils]" )
{
  std::string input = "m_i m_c x_1 x_3 m_e x_2 m_n x_3";

  auto mTokens = parse_tokens( input, "m_" );
  auto xTokens = parse_tokens( input, "x_" );
  auto allTokens = parse_tokens( input, "" );

  CHECK( mTokens.size() == 4u );
  CHECK( mTokens[ 0 ] == "m_i" );
  CHECK( mTokens[ 1 ] == "m_c" );
  CHECK( mTokens[ 2 ] == "m_e" );
  CHECK( mTokens[ 3 ] == "m_n" );

  CHECK( xTokens.size() == 4u );
  CHECK( xTokens[ 0 ] == "x_1" );
  CHECK( xTokens[ 1 ] == "x_3" );
  CHECK( xTokens[ 2 ] == "x_2" );
  CHECK( xTokens[ 3 ] == "x_3" );

  CHECK( allTokens.size() == 8u );
  CHECK( allTokens[ 0 ] == "m_i" );
  CHECK( allTokens[ 1 ] == "m_c" );
  CHECK( allTokens[ 2 ] == "x_1" );
  CHECK( allTokens[ 3 ] == "x_3" );
  CHECK( allTokens[ 4 ] == "m_e" );
  CHECK( allTokens[ 5 ] == "x_2" );
  CHECK( allTokens[ 6 ] == "m_n" );
  CHECK( allTokens[ 7 ] == "x_3" );
}
