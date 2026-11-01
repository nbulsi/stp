// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT
#pragma once

#include <alice/alice.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stp/io/lut_parser.hpp>
#include <stp/lutxx/decompose.hpp>

namespace stp
{
struct StoredTruth
{
  std::string hex;
  std::string bits;
  unsigned inputs = 0;
  CircuitGraph graph;
};
struct StoredBench
{
  std::string name;
  CircuitGraph graph;
};

inline CircuitGraph load_bench(std::istream &input)
{
  CircuitGraph graph;
  LutParser parser;
  if (!parser.parse(input, graph))
    throw std::runtime_error(parser.error());
  if (graph.get_outputs().empty())
    throw std::runtime_error("BENCH has no outputs");
  graph.match_logic_depth();
  return graph;
}

inline StoredTruth make_truth(const std::string &text)
{
  const auto bits = lutxx::parse_table(text);
  if (bits.size() < 2)
    throw std::runtime_error("truth table needs at least two entries");
  StoredTruth result;
  result.hex = lutxx::to_hex(bits);
  result.bits = bits;
  while ((size_t{1} << result.inputs) < bits.size())
    ++result.inputs;
  std::ostringstream bench;
  for (unsigned i = 0; i < result.inputs; ++i)
    bench << "INPUT(x" << i << ")\n";
  bench << "OUTPUT(po)\npo = LUT " << result.hex << " (";
  for (unsigned i = 0; i < result.inputs; ++i)
    bench << (i ? "," : "") << "x" << i;
  bench << ")\n";
  std::istringstream input(bench.str());
  result.graph = load_bench(input);
  return result;
}

inline StoredBench read_bench_snapshot(const std::string &filename)
{
  std::ifstream input(filename);
  if (!input)
    throw std::runtime_error("cannot open " + filename);
  return {filename, load_bench(input)};
}
} // namespace stp

namespace alice
{
ALICE_ADD_STORE(stp::StoredTruth, "truth", "t", "truth table", "truth tables")
ALICE_ADD_STORE(stp::StoredBench, "bench", "b", "BENCH circuit", "BENCH circuits")

ALICE_DESCRIBE_STORE(stp::StoredTruth, element)
{
  return element.hex + " (" + std::to_string(element.inputs) + " inputs)";
}
ALICE_DESCRIBE_STORE(stp::StoredBench, element)
{
  return element.name + " (" + std::to_string(element.graph.get_inputs().size()) + " PI, " +
         std::to_string(element.graph.get_outputs().size()) + " PO, " +
         std::to_string(element.graph.get_gates().size()) + " LUTs)";
}

inline void save_truth(const environment::ptr &env, stp::StoredTruth entry)
{
  env->store<stp::StoredTruth>().extend() = std::move(entry);
  env->set_default_option("truth");
}
inline void save_bench(const environment::ptr &env, stp::StoredBench entry)
{
  env->store<stp::StoredBench>().extend() = std::move(entry);
  env->set_default_option("bench");
}
inline void save_bench_result(const environment::ptr &env, const std::string &filename)
{
  try
  {
    save_bench(env, stp::read_bench_snapshot(filename));
  }
  catch (const std::exception &error)
  {
    env->out() << "[w] Result not added to store: " << error.what() << '\n';
  }
}
inline std::string current_truth(const environment::ptr &env)
{
  const auto &entries = env->store<stp::StoredTruth>();
  if (entries.empty())
    throw std::runtime_error("truth store is empty; use read_truth first");
  return entries.current().hex;
}
inline std::vector<std::string> current_truth_variables(const environment::ptr &env)
{
  const auto &entry = env->store<stp::StoredTruth>().current();
  std::vector<std::string> variables;
  for (unsigned i = entry.inputs; i > 0; --i)
    variables.push_back("x" + std::to_string(i - 1));
  return variables;
}
inline std::string current_truth_bits(const environment::ptr &env)
{
  (void)current_truth(env); // Validate before accessing current().
  return env->store<stp::StoredTruth>().current().bits;
}
} // namespace alice
