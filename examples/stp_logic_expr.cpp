#include <iostream>
#include <stp/stp_logic_expr.hpp>

using Eigen::MatrixXi;

int main()
{
  std::cout << "---------------------------------------------------------------"
               "-------------"
            << std::endl;

  std::string expr1 = "(a & b) | (a & ~c) | (~b & ~c)";
  std::vector<std::string> inputs_order1 = {"a", "b", "c"};
  MatrixXi mat1 = stp::from_exp_to_nmx( expr1, inputs_order1, false );
  std::cout << "The normal matrix for expression " << expr1 << " is "
            << std::endl;
  std::cout << mat1 << std::endl;
  std::cout << "The binary truth table is: " << stp::to_binary( mat1 )
            << std::endl;
  std::cout << "The hex truth table is: 0X" << stp::to_hex( mat1 ) << std::endl;

  std::cout << "---------------------------------------------------------------"
               "-------------"
            << std::endl;

  std::string expr2 = "(a & b & ~c) | (a & ~c) | (~b & ~c)";
  std::vector<std::string> inputs_order2 = {"a", "b", "c"};
  MatrixXi mat2 = stp::from_exp_to_nmx( expr2, inputs_order2, false );
  std::cout << "The normal matrix for expression " << expr2 << " is "
            << std::endl;
  std::cout << mat2 << std::endl;
  std::cout << "The binary truth table is: " << stp::to_binary( mat2 )
            << std::endl;
  std::cout << "The hex truth table is: 0X" << stp::to_hex( mat2 ) << std::endl;

  std::cout << "---------------------------------------------------------------"
               "-------------"
            << std::endl;

  std::string expr3 = "m_d m_c x_1 x_2 m_d m_c x_1 m_n x_3 m_c m_n x_2 m_n x_3";
  std::vector<std::string> input_names{"x_1", "x_2", "x_3"};
  MatrixXi mat3 = stp::expr_normalize( expr3, input_names, false );
  std::cout << "The normal matrix for expression " << expr3 << " is "
            << std::endl;
  std::cout << mat3 << std::endl;
  std::cout << "The binary truth table is: " << stp::to_binary( mat3 )
            << std::endl;
  std::cout << "The hex truth table is: 0X" << stp::to_hex( mat3 ) << std::endl;
}
