#include <iostream>
#include <stp/stp_eigen.hpp>
#include <stp/stp_logic_expr.hpp>
#include <stp/stp_multi_thread.hpp>

using matrix = Eigen::MatrixXi;

int main()
{
  std::string expr =
      "m_t m_d m_c x_1 x_2 m_c x_3 x_4 m_i m_d m_c x_5 x_6 m_c x_7 x_8 m_d m_c "
      "x_9 x_10 m_c x_11 x_12";
  std::vector<std::string> input_names{"x_1", "x_2",  "x_3",  "x_4",
                                       "x_5", "x_6",  "x_7",  "x_8",
                                       "x_9", "x_10", "x_11", "x_12"};
  auto mc = stp::expr_normalize_to_chain( expr, input_names );

  auto mat1 = stp::matrix_chain_multiply_by_multi_thread( mc, 1, false, true );
  auto mat2 = stp::matrix_chain_multiply_by_multi_thread( mc, 2, true, true );

  assert( mat1 == mat2 );
  std::cout << "The normal matrix for expression " << expr << " is "
            << std::endl;

  // std::cout << mat1 << std::endl;
  // std::cout << "The binary truth table is: " << stp::to_binary( mat1 ) <<
  // std::endl;
  std::cout << "The hex truth table is: 0X" << stp::to_hex( mat1 ) << std::endl;

  // std::cout << mat2 << std::endl;
  // std::cout << "The binary truth table is: " << stp::to_binary( mat2 ) <<
  // std::endl;
  std::cout << "The hex truth table is: 0X" << stp::to_hex( mat2 ) << std::endl;
  return 0;
}
