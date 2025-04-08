#include <stp/bench_parser.hpp>
#include <stp/stp_dag2nmx.hpp>
#include <stp/stp_simulator.hpp>

using namespace stp;

// example test：  ./examples/stp_simulator ../benchmarks/c17_lut.bench
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
  // c.print_circuit();

  std::cout << "***************************************" << std::endl;
  circuit_normalize_impl cn( c, false );
  std::string m1 = cn.run_str( false );
  std::cout << "old method\n";
  std::cout << m1 << "\n";

  std::string m2 = cn.run_str( true );
  std::cout << "new method\n";
  std::cout << m2 << "\n";

  stp_simulator sim( c, true );
  std::cout << "simulation result\n";
  sim.run();
  sim.print_info();
  // std::cout << sim.run() << "\n";

  return 0;
}
