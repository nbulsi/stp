#include <Eigen/Dense>
#include <iostream>

using Eigen::MatrixXd;

void stp_demo()
{
  MatrixXd m( 2, 2 );
  m( 0, 0 ) = 1;
  m( 1, 0 ) = 2;
  m( 0, 1 ) = 3;
  m( 1, 1 ) = 4;
  std::cout << m << std::endl;
}

int main()
{
  stp_demo();

  return 0;
}
