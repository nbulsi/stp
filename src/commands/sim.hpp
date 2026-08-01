// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#ifndef SIM_HPP
#define SIM_HPP

#include <alice/alice.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stp/core/circuit_graph.hpp>
#include <stp/io/expr_parser.hpp>
#include <stp/io/logic_expr_parser.hpp>
#include <stp/io/lut_parser.hpp>
#include <stp/sim/execute.hpp>
#include <stp/sim/simulator.hpp>

namespace alice
{
namespace
{
bool configure_backend(const std::string &name, const bool cuda_alias, SimulationBackend &backend)
{
  const std::string selected = cuda_alias ? "stp-gpu" : name;
  if (selected == "stp-cpu")
    backend = SimulationBackend::StpCpu;
  else if (selected == "stp-gpu")
    backend = SimulationBackend::StpGpu;
  else if (selected == "bitslice")
    backend = SimulationBackend::BitSlice;
  else if (selected == "hybrid-cpu")
    backend = SimulationBackend::HybridCpu;
  else if (selected == "hybrid-gpu")
    backend = SimulationBackend::HybridGpu;
  else
  {
    std::cerr << "unknown simulation backend '" << selected
              << "'; expected stp-cpu, stp-gpu, bitslice, hybrid-cpu, or hybrid-gpu\n";
    return false;
  }

  const bool use_gpu =
      backend == SimulationBackend::StpGpu || backend == SimulationBackend::HybridGpu;
  if (!use_gpu)
    return true;

#ifdef ENABLE_CUDA
  if (!Get_Total_Thread_Num())
  {
    std::cerr << "CUDA initialization failed; select a CPU backend to continue\n";
    return false;
  }
  return true;
#else
  std::cerr << "GPU backend requested, but CUDA support is not enabled in this build\n";
  return false;
#endif
}

void simulate_and_report(CircuitGraph &graph, const std::string &design, const bool verbose,
                         const bool print_truth_tables, const SimulationBackend backend,
                         const unsigned threads)
{
  simulator sim(graph, backend, threads);
  const auto start = std::chrono::high_resolution_clock::now();
  if (!sim.simulate())
  {
    std::cerr << "simulation failed\n";
    return;
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  std::cout << "\n****************************************\n";
  std::cout << "Report : STP logic simulation\n";
  std::cout << "Design : " << design << '\n';
  std::cout << "----------------------------------------\n";
  sim.print_simulation_summary(std::cout, print_truth_tables);
  std::cout << "----------------------------------------\n";
  std::cout << "  Runtime : " << std::fixed << std::setprecision(3)
            << static_cast<double>(time) / 1000.0 << " ms\n";
  std::cout << "****************************************\n";

  if (verbose)
  {
    std::cout << "\nDetailed truth table\n";
    sim.print_simulation_result();
  }
}

std::string join_expression(const std::vector<std::string> &tokens)
{
  std::stringstream stream;
  for (size_t i = 0; i < tokens.size(); ++i)
  {
    if (i != 0)
      stream << ' ';
    stream << tokens[i];
  }
  return stream.str();
}

std::vector<std::string> split_input_order(const std::string &input_order)
{
  if (input_order.empty())
    return {};

  std::vector<std::string> result;
  std::stringstream stream(input_order);
  std::string name;
  while (std::getline(stream, name, ','))
    result.push_back(name);
  return result;
}
} // namespace

class lutsim_command : public command
{
public:
  explicit lutsim_command(const environment::ptr &env)
      : command(env, "Simulate a LUT BENCH circuit and print truth tables")
  {
    add_flag("--verbose", "print the detailed input/output truth table");
    add_flag("--no-truth-table", "do not print output truth tables");
    add_flag("--cuda, -c", "alias for --backend stp-gpu");
    add_option("--backend", backend_name,
               "stp-cpu (default), stp-gpu, bitslice, hybrid-cpu, or hybrid-gpu");
    add_option("--threads", threads, "simulation threads; 0 means automatic", true);
    add_option("filename", filename, "input bench file", true);
  }

protected:
  void execute() override
  {
    std::ifstream input(filename);
    if (!input.good())
    {
      std::cout << "can't open file " << filename << std::endl;
      return;
    }

    CircuitGraph graph;
    LutParser parser;
    if (!parser.parse(input, graph))
    {
      std::cout << "can't parse file " << filename << std::endl;
      return;
    }
    SimulationBackend backend;
    if (configure_backend(backend_name, is_set("cuda") || is_set("-c"), backend))
      simulate_and_report(graph, filename, is_set("verbose"), !is_set("no-truth-table"), backend,
                          threads);
  }

private:
  std::string filename;
  std::string backend_name = "stp-cpu";
  unsigned threads = 0;
};

class exprsim_command : public command
{
public:
  explicit exprsim_command(const environment::ptr &env)
      : command(env, "Simulate a Boolean expression and print its truth table")
  {
    add_flag("--verbose", "print the detailed input/output truth table");
    add_flag("--no-truth-table", "do not print output truth tables");
    add_flag("--cuda, -c", "alias for --backend stp-gpu");
    add_option("--backend", backend_name,
               "stp-cpu (default), stp-gpu, bitslice, hybrid-cpu, or hybrid-gpu");
    add_option("--threads", threads, "simulation threads; 0 means automatic", true);
    add_option("--inputs", input_order, "comma-separated input order; the first name is the LSB");
    add_option("expression", expression_tokens, "Lisp-style Boolean expression", true);
  }

protected:
  void execute() override
  {
    CircuitGraph graph;
    LogicExprParser parser;
    std::string error;
    const std::string expression = join_expression(expression_tokens);
    const std::vector<std::string> requested_order =
        is_set("inputs") ? split_input_order(input_order) : std::vector<std::string>{};
    if (!parser.parse(expression, graph, error, requested_order))
    {
      std::cout << "can't parse expression: " << error << std::endl;
      return;
    }
    SimulationBackend backend;
    if (configure_backend(backend_name, is_set("cuda") || is_set("-c"), backend))
      simulate_and_report(graph, expression, is_set("verbose"), !is_set("no-truth-table"), backend,
                          threads);
  }

private:
  std::vector<std::string> expression_tokens;
  std::string input_order;
  std::string backend_name = "stp-cpu";
  unsigned threads = 0;
};

ALICE_ADD_COMMAND(lutsim, "Simulation");
ALICE_ADD_COMMAND(exprsim, "Simulation");
} // namespace alice

#endif
