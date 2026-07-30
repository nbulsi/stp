// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <alice/alice.hpp>
#include <cctype>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stp/dsd/decomposer.hpp>

namespace alice
{
namespace
{
class command_option_reset_guard
{
public:
  explicit command_option_reset_guard(std::function<void()> reset) : reset(std::move(reset)) {}
  ~command_option_reset_guard()
  {
    reset();
  }

private:
  std::function<void()> reset;
};

std::vector<std::string> split_dsd_inputs(const std::string &text)
{
  std::vector<std::string> result;
  std::stringstream stream(text);
  std::string name;
  while (std::getline(stream, name, ','))
  {
    if (!name.empty())
      result.push_back(name);
  }
  return result;
}

bool infer_dsd_variables(const std::string &truth_table, std::vector<std::string> &variables_msb,
                         std::string &error)
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
  {
    if (!std::isxdigit(static_cast<unsigned char>(c)))
    {
      error = "truth table must be a hexadecimal value";
      return false;
    }
  }

  const size_t bits = value.size() * 4;
  size_t variable_count = 0;
  size_t entries = 1;
  while (entries < bits)
  {
    entries <<= 1;
    ++variable_count;
  }
  if (entries != bits || variable_count == 0 || variable_count > 20)
  {
    error = "truth table width must be a power of two between 2 and 2^20 bits";
    return false;
  }

  // The user-facing default is LSB -> MSB: a, b, c, ... .  The
  // decomposition engine receives the reverse order, MSB -> LSB.
  std::vector<std::string> variables_lsb;
  for (size_t i = 0; i < variable_count; ++i)
    variables_lsb.emplace_back(1, static_cast<char>('a' + i));
  variables_msb.assign(variables_lsb.rbegin(), variables_lsb.rend());
  return true;
}

bool infer_idsd_variables(const std::string &truth_table, std::vector<std::string> &variables_msb,
                          std::string &error)
{
  if (truth_table.empty())
  {
    error = "truth table must contain 0, 1, and -";
    return false;
  }
  for (const char c : truth_table)
  {
    if (c != '0' && c != '1' && c != '-')
    {
      error = "truth table must contain only 0, 1, and -";
      return false;
    }
  }

  const size_t bits = truth_table.size();
  size_t entries = 1;
  size_t variable_count = 0;
  while (entries < bits)
  {
    entries <<= 1;
    ++variable_count;
  }
  if (entries != bits || variable_count == 0 || variable_count > 20)
  {
    error = "truth table width must be a power of two between 2 and 2^20 entries";
    return false;
  }

  std::vector<std::string> variables_lsb;
  for (size_t i = 0; i < variable_count; ++i)
    variables_lsb.emplace_back(1, static_cast<char>('a' + i));
  variables_msb.assign(variables_lsb.rbegin(), variables_lsb.rend());
  return true;
}
} // namespace

class dsd_command : public command
{
public:
  explicit dsd_command(const environment::ptr &env)
      : command(env, "Exact STP-based ACD/DSD decomposition")
  {
    add_option("truth_table", truth_table, "hexadecimal truth table", true);
    add_option("--inputs", inputs, "comma-separated variables in MSB-to-LSB order", true);
    add_option("--output", output, "output name", true);
    add_option("--filename,-o", filename, "output BENCH filename", true);
    add_option("--threads", threads, "candidate-search threads; 0 means automatic", true);
    add_flag("--enumerate", "enumerate all feasible ACD decompositions");
  }

protected:
  void execute() override
  {
    command_option_reset_guard reset([this]() { reset_options(); });
    std::vector<std::string> variables_msb;
    std::string error;
    if (!is_set("inputs"))
    {
      if (!infer_dsd_variables(truth_table, variables_msb, error))
      {
        env->out() << "[e] " << error << std::endl;
        return;
      }
    }
    else
    {
      variables_msb = split_dsd_inputs(inputs);
    }

    stp::DsdDecomposer decomposer;
    const auto start = std::chrono::high_resolution_clock::now();
    const bool enumerate_all = is_set("enumerate");
    const auto result =
        decomposer.run(truth_table, variables_msb, output, filename, threads, enumerate_all);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (!result.valid)
    {
      env->out() << "[e] " << result.error << std::endl;
      return;
    }

    if (enumerate_all)
    {
      env->out() << "[i] " << result.solutions.size() << " ACD solutions found. " << std::endl;
    }

    for (size_t index = 0; index < result.solutions.size(); ++index)
      env->out() << "[solution " << index << "] " << result.solutions[index] << std::endl
                 << std::endl;
    env->out() << "[i] expr: " << output << " = " << result.expression << std::endl;
    env->out() << "[i] #nodes = " << result.nodes << ", #levels = " << result.logic_levels
               << std::endl;
    env->out() << "[i] DSD result written to " << filename << std::endl;
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
  }

  std::string truth_table;
  std::string inputs;
  std::string output = "po";
  std::string filename = "result.bench";
  unsigned threads = 1;
};

ALICE_ADD_COMMAND(dsd, "Decomposition");

class idsd_command : public command
{
public:
  explicit idsd_command(const environment::ptr &env)
      : command(env, "ACD/DSD decomposition for incompletely specified truth tables")
  {
    add_option("truth_table", truth_table, "binary truth table using 0, 1, and -", true);
    add_option("--inputs", inputs, "comma-separated variables in MSB-to-LSB order", true);
    add_option("--output", output, "output name", true);
    add_option("--filename,-o", filename, "output BENCH filename", true);
    add_option("--threads", threads, "candidate-search threads; 0 means automatic", true);
    add_flag("--enumerate", "enumerate all feasible ACD decompositions");
  }

protected:
  void execute() override
  {
    command_option_reset_guard reset([this]() { reset_options(); });
    std::vector<std::string> variables_msb;
    std::string error;
    if (!is_set("inputs"))
    {
      if (!infer_idsd_variables(truth_table, variables_msb, error))
      {
        env->out() << "[e] " << error << std::endl;
        return;
      }
    }
    else
      variables_msb = split_dsd_inputs(inputs);

    stp::DsdDecomposer decomposer;
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = decomposer.run_incomplete(truth_table, variables_msb, output, filename,
                                                  threads, is_set("enumerate"));
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (!result.valid)
    {
      env->out() << "[e] " << result.error << std::endl;
      return;
    }

    if (is_set("enumerate"))
      env->out() << "[i] " << result.solutions.size() << " ACD solutions found. " << std::endl;
    for (size_t index = 0; index < result.solutions.size(); ++index)
      env->out() << "[solution " << index << "] " << result.solutions[index] << std::endl
                 << std::endl;
    env->out() << "[i] expr: " << output << " = " << result.expression << std::endl;
    env->out() << "[i] #nodes = " << result.nodes << ", #levels = " << result.logic_levels
               << std::endl;
    env->out() << "[i] IDSD result written to " << filename << std::endl;
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
  }

  std::string truth_table;
  std::string inputs;
  std::string output = "po";
  std::string filename = "result.bench";
  unsigned threads = 1;
};

ALICE_ADD_COMMAND(idsd, "Decomposition");
} // namespace alice
