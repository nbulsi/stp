// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <stp/core/circuit_graph.hpp>

namespace stp
{

struct DsdResult
{
  bool valid = false;
  std::string error;
  std::string expression;
};

class DsdDecomposer
{
public:
  DsdResult run(const std::string &truth_table, const std::vector<std::string> &variables_msb,
                const std::string &output_name, const std::string &bench_filename)
  {
    if (variables_msb.empty() || variables_msb.size() > 20)
      return {false, "the number of variables must be between 1 and 20"};

    for (const auto &name : variables_msb)
      if (name.empty())
        return {false, "variable names must not be empty"};

    std::unordered_set<std::string> names(variables_msb.begin(), variables_msb.end());
    if (names.size() != variables_msb.size())
      return {false, "variable names must be unique"};

    std::vector<unsigned char> truth;
    if (!parse_truth_table(truth_table, 1U << variables_msb.size(), truth))
      return {false, "truth table must be a hexadecimal value with 2^n bits"};

    // CircuitGraph/bench input declarations follow the user-facing order
    // (LSB -> MSB), while the decomposition algorithm uses MSB -> LSB.
    for (auto it = variables_msb.rbegin(); it != variables_msb.rend(); ++it)
      graph.add_input(*it);
    graph.add_output(output_name);

    next_node = 0;
    const std::string expression = decompose(truth, variables_msb, output_name);
    try
    {
      write_bench(bench_filename);
    }
    catch (const std::exception &error)
    {
      return {false, error.what()};
    }
    return {true, {}, expression};
  }

private:
  struct Candidate
  {
    std::vector<unsigned char> inner_truth;
    std::vector<unsigned char> outer_truth;
    std::vector<std::string> bound;
    std::vector<std::string> free;
  };

  static bool parse_truth_table(const std::string &text, size_t length,
                                std::vector<unsigned char> &truth)
  {
    std::string value = text;
    if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0)
      value.erase(0, 2);
    if (value.empty() || value.size() * 4 < length)
      return false;
    for (const char c : value)
      if (!std::isxdigit(static_cast<unsigned char>(c)))
        return false;

    const size_t padding = value.size() * 4 - length;
    for (size_t bit = 0; bit < padding; ++bit)
    {
      const size_t digit = bit / 4;
      const unsigned shift = 3U - static_cast<unsigned>(bit % 4);
      const unsigned digit_value =
          static_cast<unsigned>(std::stoi(value.substr(digit, 1), nullptr, 16));
      if (((digit_value >> shift) & 1U) != 0U)
        return false;
    }

    truth.assign(length, 0);
    for (size_t i = 0; i < length; ++i)
    {
      const size_t bit = padding + i;
      const size_t digit = bit / 4;
      const unsigned shift = 3U - static_cast<unsigned>(bit % 4);
      const unsigned digit_value =
          static_cast<unsigned>(std::stoi(value.substr(digit, 1), nullptr, 16));
      truth[i] = static_cast<unsigned char>((digit_value >> shift) & 1U);
    }
    return true;
  }

  static stp_vec make_type(const std::vector<unsigned char> &truth)
  {
    stp_vec type(truth.size() + 1);
    type(0) = 2;
    for (size_t i = 0; i < truth.size(); ++i)
      type(static_cast<unsigned>(i + 1)) = 1U - truth[i];
    return type;
  }

  static std::string lut_hex(const Type &type)
  {
    const size_t entries = type.cols() - 1;
    const size_t digits = (entries + 3) / 4;
    std::string bits;
    bits.reserve(entries);
    for (size_t i = 0; i < entries; ++i)
      bits.push_back(type(static_cast<unsigned>(i + 1)) == 0 ? '1' : '0');

    std::string result;
    result.reserve(digits + 2);
    result = "0x";
    const size_t padding = digits * 4 - entries;
    unsigned nibble = 0;
    for (size_t i = 0; i < padding + entries; ++i)
    {
      nibble <<= 1;
      if (i >= padding && bits[i - padding] == '1')
        nibble |= 1U;
      if ((i + 1) % 4 == 0)
      {
        result.push_back("0123456789abcdef"[nibble]);
        nibble = 0;
      }
    }
    return result;
  }

  static std::vector<unsigned char> reorder(const std::vector<unsigned char> &truth,
                                            const std::vector<std::string> &variables,
                                            const std::vector<std::string> &bound,
                                            const std::vector<std::string> &free)
  {
    const size_t n = variables.size();
    const size_t r = bound.size();
    const size_t s = free.size();
    std::vector<size_t> positions;
    positions.reserve(n);
    for (const auto &name : bound)
      positions.push_back(static_cast<size_t>(std::find(variables.begin(), variables.end(), name) -
                                              variables.begin()));
    for (const auto &name : free)
      positions.push_back(static_cast<size_t>(std::find(variables.begin(), variables.end(), name) -
                                              variables.begin()));

    std::vector<unsigned char> result;
    result.reserve(truth.size());
    // Truth-table strings and LUT hex values use the conventional order
    // b_(2^n-1), ..., b_0: the first entry is the all-ones assignment.
    for (size_t combined = 0; combined < truth.size(); ++combined)
    {
      const size_t combined_assignment = truth.size() - 1 - combined;
      size_t original = 0;
      for (size_t i = 0; i < n; ++i)
      {
        const size_t reordered_bit = (combined_assignment >> (n - 1 - i)) & 1U;
        original |= reordered_bit << (n - 1 - positions[i]);
      }
      result.push_back(truth[truth.size() - 1 - original]);
    }
    return result;
  }

  static bool find_candidate(const std::vector<unsigned char> &truth,
                             const std::vector<std::string> &variables, Candidate &candidate)
  {
    const size_t n = variables.size();
    if (n < 3)
      return false;

    for (size_t m = 2; m < n; ++m)
    {
      for (size_t mask = 0; mask < (1ULL << n); ++mask)
      {
        if (__builtin_popcountll(mask) != static_cast<int>(m))
          continue;
        std::vector<std::string> bound, free;
        for (size_t i = 0; i < n; ++i)
          (mask & (1ULL << (n - 1 - i)) ? bound : free).push_back(variables[i]);

        const auto reordered = reorder(truth, variables, bound, free);
        const size_t block_length = 1ULL << (n - m);
        std::vector<std::vector<unsigned char>> patterns;
        std::vector<unsigned char> selectors;
        for (size_t i = 0; i < (1ULL << m); ++i)
        {
          std::vector<unsigned char> block(reordered.begin() + i * block_length,
                                           reordered.begin() + (i + 1) * block_length);
          auto it = std::find(patterns.begin(), patterns.end(), block);
          if (it == patterns.end())
          {
            if (patterns.size() == 2)
              break;
            patterns.push_back(block);
            // The first block occupies the y=1 half of M_X, and the
            // second block occupies the y=0 half, matching LUT bit order.
            selectors.push_back(static_cast<unsigned char>(patterns.size() == 1 ? 1 : 0));
          }
          else
            selectors.push_back(static_cast<unsigned char>(it == patterns.begin() ? 1 : 0));
        }
        if (patterns.size() == 2 && selectors.size() == (1ULL << m))
        {
          candidate.bound = std::move(bound);
          candidate.free = std::move(free);
          candidate.inner_truth = selectors;
          candidate.outer_truth = patterns[0];
          candidate.outer_truth.insert(candidate.outer_truth.end(), patterns[1].begin(),
                                       patterns[1].end());
          return true;
        }
      }
    }
    return false;
  }

  std::string decompose(const std::vector<unsigned char> &truth,
                        const std::vector<std::string> &variables, const std::string &output)
  {
    Candidate candidate;
    if (!find_candidate(truth, variables, candidate))
    {
      std::vector<std::string> gate_inputs(variables.rbegin(), variables.rend());
      const Type type = make_type(truth);
      graph.add_gate(type, gate_inputs, output);
      return make_expression(lut_hex(type), gate_inputs);
    }

    const std::string inner_name = "dsd_n" + std::to_string(next_node++);
    const std::string inner_expression =
        decompose(candidate.inner_truth, candidate.bound, inner_name);

    std::vector<std::string> outer_variables{inner_name};
    outer_variables.insert(outer_variables.end(), candidate.free.begin(), candidate.free.end());
    std::string expression = decompose(candidate.outer_truth, outer_variables, output);
    const std::string token = inner_name;
    size_t position = 0;
    while ((position = expression.find(token, position)) != std::string::npos)
    {
      expression.replace(position, token.size(), inner_expression);
      position += inner_expression.size();
    }
    return expression;
  }

  static std::string make_expression(const std::string &lut, const std::vector<std::string> &inputs)
  {
    std::string expression = lut + "(";
    for (size_t i = 0; i < inputs.size(); ++i)
    {
      if (i != 0)
        expression += ", ";
      expression += inputs[i];
    }
    expression += ")";
    return expression;
  }

  void write_bench(const std::string &filename) const
  {
    std::ofstream output(filename);
    if (!output.good())
      throw std::runtime_error("cannot write bench file " + filename);
    for (const auto input : graph.get_inputs())
      output << "INPUT(" << graph.get_line(input).name << ")\n";
    for (const auto output_line : graph.get_outputs())
      output << "OUTPUT(" << graph.get_line(output_line).name << ")\n";
    output << "\n";
    for (const auto &gate : graph.get_gates())
    {
      output << graph.get_line(gate.get_output()).name << " = LUT " << lut_hex(gate.get_type())
             << " (";
      for (size_t i = gate.get_inputs().size(); i-- > 0;)
      {
        if (i + 1 != gate.get_inputs().size())
          output << ", ";
        output << graph.get_line(gate.get_inputs()[i]).name;
      }
      output << ")\n";
    }
  }

public:
  CircuitGraph graph;

private:
  unsigned next_node = 0;
};

} // namespace stp
