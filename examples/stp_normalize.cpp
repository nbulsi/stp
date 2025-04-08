#include <stp/bench_parser.hpp>
#include <stp/stp_circuit.hpp>
#include <stp/stp_normalize.hpp>

using namespace stp;

int main( int argc, char **argv )
{
  if ( argc < 2 )
    {
      std::cout << "no input file specified" << std::endl;
      return 1;
    }
  std::ifstream ifs( argv[ 1 ] );
  if ( !ifs.good() )
    {
      std::cout << "can't open file" << argv[ 1 ] << std::endl;
      return 1;
    }

  stp_circuit c;
  bench_reader parser;

  if ( !parser.parse( ifs, c ) )
    {
      std::cout << "can't parse file" << argv[ 1 ] << std::endl;
      return 1;
    }
  c.update_levels();

  // stp_normalize( c );
  stp_normalize_string( c );

  return 0;
}
