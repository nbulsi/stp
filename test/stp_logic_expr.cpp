#include <catch.hpp>

#include <stp/stp_logic_expr.hpp>
#include <stp/stp_utils.hpp>

using namespace stp;
using matrix = Eigen::MatrixXi;

TEST_CASE( "STP calculation for basic operators", "expr" )
{
  // AND x_1 /\ x_2 /\ x_3 ( x_1 is the MSB and x_3 is the LSB )
  std::string input1 = "m_c x_1 m_c x_2 x_3";
  std::vector<std::string> input_names1{"x_3", "x_2", "x_1"};
  matrix mat1 = expr_normalize( input1, input_names1 );
  CHECK( stp::to_binary( mat1 ) == "10000000" );
  CHECK( stp::to_hex( mat1 ) == "80" );

  // OR x_1 \/ x_2 \/ x_3
  std::string input2 = "m_d x_1 m_d x_2 x_3";
  std::vector<std::string> input_names2{"x_3", "x_2", "x_1"};
  matrix mat2 = expr_normalize( input2, input_names2 );
  CHECK( stp::to_binary( mat2 ) == "11111110" );
  CHECK( stp::to_hex( mat2 ) == "FE" );

  // IMPLY x_1 -> ( x_2 -> x_3 )
  std::string input3 = "m_i x_1 m_i x_2 x_3";
  std::vector<std::string> input_names3{"x_3", "x_2", "x_1"};
  matrix mat3 = expr_normalize( input3, input_names3 );
  CHECK( stp::to_binary( mat3 ) == "11110111" );
  CHECK( stp::to_hex( mat3 ) == "F7" );

  // XNOR and XOR
  std::string input4 = "m_e x_1 m_p x_2 x_3";
  std::vector<std::string> input_names4{"x_3", "x_2", "x_1"};
  matrix mat4 = expr_normalize( input4, input_names4 );
  CHECK( stp::to_binary( mat4 ) == "01101001" );
  CHECK( stp::to_hex( mat4 ) == "69" );

  // NAND and NOR
  std::string input5 = "m_t x_1 m_b x_2 x_3";
  std::vector<std::string> input_names5{"x_3", "x_2", "x_1"};
  matrix mat5 = expr_normalize( input5, input_names5 );
  CHECK( stp::to_binary( mat5 ) == "11111101" );
  CHECK( stp::to_hex( mat5 ) == "FD" );
}

TEST_CASE( "expression to tt", "[expr]" )
{
  std::string expr1 = "(a & b) | (a & ~c) | (~b & ~c)";

  std::vector<std::string> inputs_order1 = {"c", "b", "a"};
  matrix mat1 = stp::from_exp_to_nmx( expr1, inputs_order1 );
  CHECK( stp::to_binary( mat1 ) == "10001011" );
  CHECK( stp::to_hex( mat1 ) == "8B" );

  std::vector<std::string> inputs_order2 = {"a", "c", "b"};
  matrix mat2 = stp::from_exp_to_nmx( expr1, inputs_order2 );
  CHECK( stp::to_binary( mat2 ) == "10110001" );
  CHECK( stp::to_hex( mat2 ) == "B1" );

  std::vector<std::string> inputs_order3 = {"a", "b", "c"};
  matrix mat3 = stp::from_exp_to_nmx( expr1, inputs_order3 );
  CHECK( stp::to_binary( mat3 ) == "11010001" );
  CHECK( stp::to_hex( mat3 ) == "D1" );
}

TEST_CASE( "STP expression to tt with different input orders", "[expr]" )
{
  std::string input = "m_d m_c x_1 x_2 m_d m_c x_1 m_n x_3 m_c m_n x_2 m_n x_3";

  std::vector<std::string> input_names1{"x_3", "x_2", "x_1"};
  matrix mat1 = expr_normalize( input, input_names1 );
  CHECK( stp::to_binary( mat1 ) == "10001011" );
  CHECK( stp::to_hex( mat1 ) == "8B" );

  std::vector<std::string> input_names2{"x_1", "x_3", "x_2"};
  matrix mat2 = expr_normalize( input, input_names2 );
  CHECK( stp::to_binary( mat2 ) == "10110001" );
  CHECK( stp::to_hex( mat2 ) == "B1" );

  std::vector<std::string> input_names3{"x_1", "x_2", "x_3"};
  matrix mat3 = expr_normalize( input, input_names3 );
  CHECK( stp::to_binary( mat3 ) == "11010001" );
  CHECK( stp::to_hex( mat3 ) == "D1" );
}

TEST_CASE( "STP expression to tt with more expresssions", "[expr]" )
{
  std::string input1 = "m_c m_d x_1 x_2 m_d x_3 x_4";
  std::vector<std::string> input_names1{"x_4", "x_3", "x_2", "x_1"};
  matrix mat1 = expr_normalize( input1, input_names1 );
  CHECK( stp::to_binary( mat1 ) == "1110111011100000" );
  CHECK( stp::to_hex( mat1 ) == "EEE0" );

  std::string input2 = "m_i m_c x_1 x_2 m_d x_3 x_4";
  std::vector<std::string> input_names2{"x_4", "x_3", "x_2", "x_1"};
  matrix mat2 = expr_normalize( input2, input_names2 );
  CHECK( stp::to_binary( mat2 ) == "1111111111110111" );
  CHECK( stp::to_hex( mat2 ) == "FFF7" );

  std::string input3 = "m_i m_c x_1 x_3 m_e x_2 m_n x_3";
  std::vector<std::string> input_names3{"x_3", "x_2", "x_1"};
  matrix mat3 = expr_normalize( input3, input_names3 );
  CHECK( stp::to_binary( mat3 ) == "01111111" );
  CHECK( stp::to_hex( mat3 ) == "7F" );
}

TEST_CASE( "STP expression to tt with more variables", "[expr]" )
{
  std::string input1 = "m_d m_c x_1 x_2 m_d m_c x_3 x_4 x_5";
  std::vector<std::string> input_names1{"x_5", "x_4", "x_3", "x_2", "x_1"};
  matrix mat1 = expr_normalize( input1, input_names1 );
  CHECK( stp::to_binary( mat1 ) == "11111111111111111111100010001000" );
  CHECK( stp::to_hex( mat1 ) == "FFFFF888" );

  std::string input2 = "m_i m_p x_1 x_2 m_d m_c x_3 x_4 m_b x_5 x_6";
  std::vector<std::string> input_names2{"x_6", "x_5", "x_4",
                                        "x_3", "x_2", "x_1"};
  matrix mat2 = expr_normalize( input2, input_names2 );
  CHECK( stp::to_binary( mat2 ) ==
         "1111100110011001111110011001100111111001100110011111111111111111" );
  CHECK( stp::to_hex( mat2 ) == "F999F999F999FFFF" );
}

TEST_CASE( "STP calculation for mixed operators", "[expr]" )
{
  std::string input1 = "m_p m_n x_4 m_c x_2 m_i x_3 x_1";
  std::vector<std::string> input_names1{"x_1", "x_2", "x_3", "x_4"};
  matrix mat1 = expr_normalize( input1, input_names1 );
  CHECK( stp::to_binary( mat1 ) == "1010010101100101" );
  CHECK( stp::to_hex( mat1 ) == "A565" );

  std::string input2 = "m_t m_d x_1 x_2 m_c x_1 x_2";
  std::vector<std::string> input_names2{"x_1", "x_2"};
  matrix mat2 = expr_normalize( input2, input_names2 );
  CHECK( stp::to_binary( mat2 ) == "0111" );
  CHECK( stp::to_hex( mat2 ) == "7" );

  std::string input3 = "m_i m_c x_1 x_3 m_e x_2 m_n x_3";
  std::vector<std::string> input_names3{"x_1", "x_2", "x_3"};
  matrix mat3 = expr_normalize( input3, input_names3 );
  CHECK( stp::to_binary( mat3 ) == "01111111" );
  CHECK( stp::to_hex( mat3 ) == "7F" );
}
