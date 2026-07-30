// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <stp/dsd/decomposer.hpp>

namespace stp
{

struct ApproximateDsdResult
{
  bool valid = false;
  std::string error;
  std::string expression;
  size_t hamming_distance = 0;
  size_t weighted_error = 0;
  size_t max_fanin = 0;
  size_t lut_cost = 0;
  size_t nodes = 0;
  size_t logic_levels = 0;
};

class ApproximateDsdDecomposer
{
public:
  ApproximateDsdResult run(const std::string &truth_table,
                           const std::vector<std::string> &variables_msb,
                           const std::string &output_name, const std::string &bench_filename,
                           unsigned thread_count = 1, size_t max_error = 1)
  {
    if (variables_msb.empty() || variables_msb.size() > 20)
      return {false, "the number of variables must be between 1 and 20"};
    std::vector<unsigned char> truth;
    if (!parse_hex(truth_table, 1ULL << variables_msb.size(), truth))
      return {false, "truth table must be a hexadecimal value with 2^n bits"};

    threads = thread_count == 0 ? std::thread::hardware_concurrency() : thread_count;
    if (threads == 0)
      threads = 1;
    if (threads > 1)
      pool = std::make_unique<DsdThreadPool>(threads);

    std::vector<Choice> choices;
    for (size_t m = 2; m < variables_msb.size(); ++m)
    {
      std::vector<size_t> masks;
      for (size_t mask = 0; mask < (1ULL << variables_msb.size()); ++mask)
      {
        if (__builtin_popcountll(mask) != static_cast<int>(m))
          continue;
        masks.push_back(mask);
      }
      std::vector<std::optional<Choice>> found(masks.size());
      const auto check_range = [&](size_t begin, size_t end)
      {
        for (size_t index = begin; index < end; ++index)
        {
          Choice choice;
          if (find_choice(truth, variables_msb, masks[index], max_error, choice))
            found[index] = std::move(choice);
        }
      };
      if (pool)
        pool->parallel_for(masks.size(), check_range);
      else
        check_range(0, masks.size());
      for (auto &choice : found)
        if (choice)
          choices.push_back(std::move(*choice));
    }
    if (choices.empty())
      return {false, "no approximate two-pattern decomposition found"};

    const Choice *best = &choices.front();
    for (const auto &choice : choices)
      if (better(choice, *best))
        best = &choice;

    for (auto it = variables_msb.rbegin(); it != variables_msb.rend(); ++it)
      graph.add_input(*it);
    graph.add_output(output_name);
    const std::string inner_name = "approx_dsd_n0";
    const Type inner_type = make_type(best->selector);
    const Type outer_type = make_type(best->outer);
    std::vector<std::string> inner_inputs(best->bound.rbegin(), best->bound.rend());
    graph.add_gate(inner_type, inner_inputs, inner_name);
    std::vector<std::string> outer_variables{inner_name};
    outer_variables.insert(outer_variables.end(), best->free.begin(), best->free.end());
    std::vector<std::string> outer_inputs(outer_variables.rbegin(), outer_variables.rend());
    graph.add_gate(outer_type, outer_inputs, output_name);
    graph.match_logic_depth();

    const std::string inner_expression = make_expression(lut_hex(inner_type), inner_inputs);
    std::vector<std::string> expression_inputs = outer_inputs;
    for (auto &input : expression_inputs)
      if (input == inner_name)
        input = inner_expression;
    ApproximateDsdResult result{true,
                                {},
                                make_expression(lut_hex(outer_type), expression_inputs),
                                best->distance,
                                best->weighted_error,
                                best->max_fanin,
                                best->lut_cost,
                                graph.get_gates().size(),
                                static_cast<size_t>(graph.get_mld() + 1)};
    write_bench(bench_filename);
    return result;
  }

  CircuitGraph graph;

private:
  struct Choice
  {
    std::vector<unsigned char> selector;
    std::vector<unsigned char> outer;
    std::vector<std::string> bound;
    std::vector<std::string> free;
    size_t distance = std::numeric_limits<size_t>::max();
    size_t weighted_error = std::numeric_limits<size_t>::max();
    size_t max_fanin = std::numeric_limits<size_t>::max();
    size_t lut_cost = std::numeric_limits<size_t>::max();
  };

  static bool parse_hex(const std::string &text, size_t length, std::vector<unsigned char> &truth)
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
    truth.clear();
    for (size_t i = 0; i < length; ++i)
    {
      const size_t bit = padding + i;
      const unsigned digit =
          static_cast<unsigned>(std::stoi(value.substr(bit / 4, 1), nullptr, 16));
      truth.push_back(static_cast<unsigned char>((digit >> (3U - bit % 4)) & 1U));
    }
    return true;
  }

  static std::vector<unsigned char> reorder(const std::vector<unsigned char> &truth,
                                            const std::vector<std::string> &variables,
                                            const std::vector<std::string> &bound,
                                            const std::vector<std::string> &free)
  {
    std::vector<size_t> positions;
    for (const auto &name : bound)
      positions.push_back(std::find(variables.begin(), variables.end(), name) - variables.begin());
    for (const auto &name : free)
      positions.push_back(std::find(variables.begin(), variables.end(), name) - variables.begin());
    const size_t n = variables.size();
    std::vector<unsigned char> result;
    for (size_t combined = 0; combined < truth.size(); ++combined)
    {
      const size_t assignment = truth.size() - 1 - combined;
      size_t original = 0;
      for (size_t i = 0; i < n; ++i)
        original |= ((assignment >> (n - 1 - i)) & 1U) << (n - 1 - positions[i]);
      result.push_back(truth[truth.size() - 1 - original]);
    }
    return result;
  }

  static bool find_choice(const std::vector<unsigned char> &truth,
                          const std::vector<std::string> &variables, size_t mask, size_t max_error,
                          Choice &best)
  {
    const size_t n = variables.size();
    for (size_t i = 0; i < n; ++i)
      ((mask & (1ULL << (n - 1 - i))) ? best.bound : best.free).push_back(variables[i]);
    const auto reordered = reorder(truth, variables, best.bound, best.free);
    const size_t block_length = 1ULL << best.free.size();
    const size_t blocks = 1ULL << best.bound.size();
    if (blocks < 2)
      return false;
    for (size_t first = 0; first < blocks; ++first)
      for (size_t second = first + 1; second < blocks; ++second)
      {
        std::vector<unsigned char> p(reordered.begin() + first * block_length,
                                     reordered.begin() + (first + 1) * block_length);
        std::vector<unsigned char> q(reordered.begin() + second * block_length,
                                     reordered.begin() + (second + 1) * block_length);
        std::vector<unsigned char> approximation;
        std::vector<unsigned char> selector;
        size_t distance = 0;
        for (size_t block = 0; block < blocks; ++block)
        {
          size_t dp = 0, dq = 0;
          for (size_t bit = 0; bit < block_length; ++bit)
          {
            const auto value = reordered[block * block_length + bit];
            dp += value != p[bit];
            dq += value != q[bit];
          }
          const bool use_p = dp <= dq;
          selector.push_back(static_cast<unsigned char>(use_p ? 1 : 0));
          distance += use_p ? dp : dq;
          approximation.insert(approximation.end(), use_p ? p.begin() : q.begin(),
                               use_p ? p.end() : q.end());
        }
        if (distance > max_error)
          continue;

        std::vector<unsigned char> original_approximation(reordered.size());
        std::vector<size_t> positions;
        for (const auto &name : best.bound)
          positions.push_back(std::find(variables.begin(), variables.end(), name) -
                              variables.begin());
        for (const auto &name : best.free)
          positions.push_back(std::find(variables.begin(), variables.end(), name) -
                              variables.begin());
        for (size_t combined = 0; combined < reordered.size(); ++combined)
        {
          const size_t assignment = reordered.size() - 1 - combined;
          size_t original = 0;
          for (size_t position = 0; position < n; ++position)
            original |= ((assignment >> (n - 1 - position)) & 1U) << (n - 1 - positions[position]);
          original_approximation[reordered.size() - 1 - original] = approximation[combined];
        }
        // The largest differing bit position is used as an LSB-aware
        // secondary rank.  A smaller value means that all errors are
        // confined closer to the truth-table LSB.
        size_t weighted_error = 0;
        for (size_t bit = 0; bit < truth.size(); ++bit)
          if (truth[bit] != original_approximation[bit])
            weighted_error = std::max(weighted_error, truth.size() - bit);
        const size_t max_fanin = std::max(best.bound.size(), best.free.size() + 1);
        const size_t lut_cost =
            std::max<size_t>(1, best.bound.size() - 1) + std::max<size_t>(1, best.free.size());
        Choice current = best;
        current.distance = distance;
        current.weighted_error = weighted_error;
        current.max_fanin = max_fanin;
        current.lut_cost = lut_cost;
        current.selector = std::move(selector);
        current.outer = p;
        current.outer.insert(current.outer.end(), q.begin(), q.end());
        if (better(current, best))
        {
          best = std::move(current);
        }
      }
    return best.distance != std::numeric_limits<size_t>::max();
  }

  static bool better(const Choice &left, const Choice &right)
  {
    return std::tie(left.max_fanin, left.lut_cost, left.weighted_error, left.distance) <
           std::tie(right.max_fanin, right.lut_cost, right.weighted_error, right.distance);
  }

  static Type make_type(const std::vector<unsigned char> &truth)
  {
    Type type(truth.size() + 1);
    type(0) = 2;
    for (size_t i = 0; i < truth.size(); ++i)
      type(static_cast<unsigned>(i + 1)) = 1U - truth[i];
    return type;
  }

  static std::string lut_hex(const Type &type)
  {
    std::string result = "0x";
    unsigned nibble = 0;
    const size_t entries = type.cols() - 1;
    for (size_t i = 0; i < entries; ++i)
    {
      nibble = (nibble << 1) | (type(static_cast<unsigned>(i + 1)) == 0 ? 1U : 0U);
      if ((i + 1) % 4 == 0)
      {
        result.push_back("0123456789abcdef"[nibble]);
        nibble = 0;
      }
    }
    return result;
  }

  static std::string make_expression(const std::string &lut, const std::vector<std::string> &inputs)
  {
    std::string result = lut + "(";
    for (size_t i = 0; i < inputs.size(); ++i)
      result += (i ? ", " : "") + inputs[i];
    return result + ")";
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
        output << (i + 1 == gate.get_inputs().size() ? "" : ", ")
               << graph.get_line(gate.get_inputs()[i]).name;
      output << ")\n";
    }
  }

  unsigned threads = 1;
  std::unique_ptr<DsdThreadPool> pool;
};

} // namespace stp
