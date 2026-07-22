#ifndef SIM_HPP
#define SIM_HPP
#include <alice/alice.hpp>
#include <chrono>
#include <fstream>
#include <stp/core/circuit_graph.hpp>
#include <stp/io/expr_parser.hpp>
#include <stp/io/lut_parser.hpp>
#include <stp/sim/execute.hpp>
#include <stp/sim/simulator.hpp>

using namespace stp;

std::string remove_quotes(const std::string &str)
{
  std::string result = str;

  if (!result.empty() && result.front() == '"' && result.back() == '"')
  {
    result.erase(result.begin());
    result.pop_back();
  }
  return result;
}

namespace alice
{
//./your_tool sim --aig your_file.bench --verbose
class sim_command : public command
{
public:
  explicit sim_command(const environment::ptr &env) : command(env, "sim")
  {
    add_flag("--verbose", "verbose output");
    add_flag("--lut, -l", "using lut network");
    add_flag("--aig, -a", "using aig network");
    add_flag("--cuda, -c", "using cuda");
    add_flag("--print, -p", "print result");
    add_option("filename", filename, "input file name", true);
  }

protected:
  void execute()
  {
    if (filename.size() == 0)
    {
      std::cout << "please specify the file " << std::endl;
      return;
    }
    // get path of input file
    std::string file_path = remove_quotes(filename);

    std::ifstream ifs(file_path);

    if (!ifs.good())
    {
      std::cout << "can't open file " << file_path << std::endl;
      return;
    }

    if (is_set("lut") || is_set("-l"))
    {
      auto Parser = 0;
      auto Solver = 0;
      CircuitGraph graph;
      LutParser parser;
      if (!parser.parse(ifs, graph))
      {
        std::cout << "can't parse file" << file_path << std::endl;
        return;
      }

      if (is_set("cuda") || is_set("-c"))
      {
#ifdef ENABLE_CUDA
        _using_CUDA = true;
        Get_Total_Thread_Num();
        simulator sim(graph);
        auto start = std::chrono::high_resolution_clock::now();
        sim.simulate();
        auto end = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        // print result
        if (is_set("print") || is_set("-p"))
        {
          sim.print_simulation_result();
        }
        std::cout << "time: " << time / 1000 << " ms\n" << std::endl;
#else
        std::cout << "can't find cuda" << std::endl;
#endif
      }
      else
      {
        _using_CUDA = false;
        simulator sim(graph);
        auto start = std::chrono::high_resolution_clock::now();
        sim.simulate();
        auto end = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        // print result
        if (is_set("print") || is_set("-p"))
        {
          sim.print_simulation_result();
        }
        std::cout << "time: " << std::fixed << std::setprecision(3)
                  << static_cast<double>(time) / 1000.0 << " ms\n"
                  << std::endl;
      }
    }
    else
    {
      std::cout << "please specify the network type" << std::endl;
    }
  }

private:
  std::string filename{};
};
ALICE_ADD_COMMAND(sim, "New Command");
} // namespace alice

#endif
