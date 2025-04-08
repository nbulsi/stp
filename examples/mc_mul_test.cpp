#include <iostream>
#include <stp/stp_eigen.hpp>

using Eigen::MatrixXi;

void print( const matrix_chain& mc )
{
  int i = 1;
  for ( const matrix& m : mc )
    {
      std::cout << "M" << i << ": " << m.rows() << "x" << m.cols() << "\n";
      i++;
    }
}

void test1()
{
  matrix_chain mc;
  for ( int i = 0; i < 22; i++ )
    {
      mc.push_back( stp::matrix_random_generation( 2, 4 ) );
    }
  // print(mc);
  matrix r1 = stp::matrix_chain_multiply( mc, true );
  matrix r2 =
      stp::matrix_chain_multiply( mc, true, stp::mc_multiply_method::sequence );
  assert( r1 == r2 );
}

void test2()
{
  matrix_chain mc;
  for ( int i = 0; i < 22; i++ )
    {
      mc.push_back( stp::matrix_random_generation( 4, 2 ) );
    }
  // print(mc);
  matrix r1 = stp::matrix_chain_multiply( mc, true );
  matrix r2 =
      stp::matrix_chain_multiply( mc, true, stp::mc_multiply_method::sequence );
  assert( r1 == r2 );
}

void test3()
{
  matrix_chain mc;
  for ( int i = 0; i < 100; i++ )
    {
      if ( rand() % 2 == 1 )
        mc.push_back( stp::matrix_random_generation( 4, 2 ) );
      else
        mc.push_back( stp::matrix_random_generation( 2, 4 ) );
    }
  // print(mc);
  matrix r1 = stp::matrix_chain_multiply( mc, true );
  matrix r2 =
      stp::matrix_chain_multiply( mc, true, stp::mc_multiply_method::sequence );
  assert( r1 == r2 );
}

int main()
{
  std::cout << "-------------------------------------------------------\n";
  test1();
  std::cout << "-------------------------------------------------------\n";
  test2();
  std::cout << "-------------------------------------------------------\n";
  test3();
  std::cout << "-------------------------------------------------------\n";
  return 0;
}
