// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <alice/alice.hpp>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stp/dsd/decomposer.hpp>

namespace alice
{
namespace
{
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
} // namespace

class dsd_command : public command
{
public:
  explicit dsd_command(const environment::ptr &env) : command(env, "dsd")
  {
    add_option("truth_table", truth_table, "hexadecimal truth table", true);
    add_option("--inputs", inputs, "comma-separated variables in MSB-to-LSB order");
    add_option("--output", output, "output name", false);
    add_option("--filename,-o", filename, "output BENCH filename", false);
  }

protected:
  void execute() override
  {
    std::vector<std::string> variables_msb;
    std::string error;
    if (inputs.empty())
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
    const auto result = decomposer.run(truth_table, variables_msb, output, filename);
    if (!result.valid)
    {
      env->out() << "[e] " << result.error << std::endl;
      return;
    }
    env->out() << output << " = " << result.expression << std::endl;
    env->out() << "[i] DSD result written to " << filename << std::endl;
  }

private:
  std::string truth_table;
  std::string inputs;
  std::string output = "po";
  std::string filename = "result.bench";
};

ALICE_ADD_COMMAND(dsd, "Decomposition");
} // namespace alice
