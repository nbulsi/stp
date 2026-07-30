// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <alice/alice.hpp>
#include <chrono>
#include <functional>
#include <iomanip>
#include <stp/dsd/approximate_decomposer.hpp>

namespace alice
{
namespace
{
class approximate_dsd_option_reset_guard
{
public:
  explicit approximate_dsd_option_reset_guard(std::function<void()> reset) : reset(std::move(reset))
  {
  }
  ~approximate_dsd_option_reset_guard()
  {
    reset();
  }

private:
  std::function<void()> reset;
};

bool infer_approximate_dsd_variables(const std::string &truth_table,
                                     std::vector<std::string> &variables_msb, std::string &error)
{
  std::string value = truth_table;
  if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0)
    value.erase(0, 2);
  if (value.empty())
  {
    error = "truth table must be a hexadecimal value";
    return false;
  }
  for (const char c : value)
    if (!std::isxdigit(static_cast<unsigned char>(c)))
    {
      error = "truth table must be a hexadecimal value";
      return false;
    }
  const size_t bits = value.size() * 4;
  size_t entries = 1, count = 0;
  while (entries < bits)
  {
    entries <<= 1;
    ++count;
  }
  if (entries != bits || count == 0 || count > 20)
  {
    error = "truth table width must be a power of two between 2 and 2^20 bits";
    return false;
  }
  for (size_t i = 0; i < count; ++i)
    variables_msb.emplace_back(1, static_cast<char>('a' + count - 1 - i));
  return true;
}
} // namespace

class approximate_dsd_command : public command
{
public:
  explicit approximate_dsd_command(const environment::ptr &env)
      : command(env, "Approximate ACD/DSD decomposition with bounded Hamming error")
  {
    add_option("truth_table", truth_table, "hexadecimal truth table", true);
    add_option("--inputs", inputs, "comma-separated variables in MSB-to-LSB order", true);
    add_option("--output", output, "output name", true);
    add_option("--filename,-o", filename, "output BENCH filename", true);
    add_option("--threads", threads, "candidate-search threads; 0 means automatic", true);
    add_option("--max-error", max_error, "maximum allowed Hamming distance", true);
  }

protected:
  void execute() override
  {
    approximate_dsd_option_reset_guard reset([this]() { reset_options(); });
    std::vector<std::string> variables_msb;
    std::string error;
    if (!is_set("inputs"))
    {
      if (!infer_approximate_dsd_variables(truth_table, variables_msb, error))
      {
        env->out() << "[e] " << error << std::endl;
        return;
      }
    }
    else
    {
      std::stringstream stream(inputs);
      std::string name;
      while (std::getline(stream, name, ','))
        if (!name.empty())
          variables_msb.push_back(name);
    }

    stp::ApproximateDsdDecomposer decomposer;
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result =
        decomposer.run(truth_table, variables_msb, output, filename, threads, max_error);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (!result.valid)
    {
      env->out() << "[e] " << result.error << std::endl;
      return;
    }
    env->out() << "[i] Hamming distance = " << result.hamming_distance << " (limit " << max_error
               << ")" << std::endl;
    env->out() << "[i] LSB-aware error rank = " << result.weighted_error
               << ", LUT cost = " << result.lut_cost << ", max fanin = " << result.max_fanin
               << std::endl;
    env->out() << "[i] expr: " << output << " = " << result.expression << std::endl;
    env->out() << "[i] #nodes = " << result.nodes << ", #levels = " << result.logic_levels
               << std::endl;
    env->out() << "[i] Approximate DSD result written to " << filename << std::endl;
    env->out() << "[i] Runtime : " << std::fixed << std::setprecision(3)
               << static_cast<double>(time) / 1000.0 << " ms" << std::endl;
  }

private:
  void reset_options()
  {
    truth_table.clear();
    inputs.clear();
    output = "po";
    filename = "result.bench";
    threads = 1;
    max_error = 1;
  }

  std::string truth_table;
  std::string inputs;
  std::string output = "po";
  std::string filename = "result.bench";
  unsigned threads = 1;
  size_t max_error = 1;
};

ALICE_ADD_COMMAND(approximate_dsd, "Decomposition");
} // namespace alice
