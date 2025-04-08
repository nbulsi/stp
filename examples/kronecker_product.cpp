#include <Eigen/Dense>
#include <iostream>

int main()
{
  Eigen::MatrixXd A( 2, 4 );
  A << 1, 0, 0, 0, 0, 1, 1, 1;

  Eigen::MatrixXd B( 2, 4 );
  B << 1, 1, 0, 1, 0, 0, 1, 0;

  Eigen::MatrixXd I = Eigen::MatrixXd::Identity( 2, 2 );

  Eigen::MatrixXd kroneckerProduct( B.rows() * I.rows(), B.cols() * I.cols() );

  for ( int i = 0; i < B.rows(); ++i )
    {
      for ( int j = 0; j < B.cols(); ++j )
        {
          kroneckerProduct.block( i * I.rows(), j * I.cols(), I.rows(),
                                  I.cols() ) = B( i, j ) * I;
        }
    }

  std::cout << "Matrix A: \n" << A << std::endl;

  std::cout << "Kronecker Product of B and I:\n"
            << kroneckerProduct << std::endl;

  std::cout << "STP Product of A by B:\n" << A * kroneckerProduct << std::endl;

  return 0;
}
