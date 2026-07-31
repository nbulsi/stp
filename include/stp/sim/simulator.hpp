// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <stp/core/circuit_graph.hpp>
#include <stp/io/expr_parser.hpp> // compatibility: exposes the legacy CUDA mode flag

// Truth tables are stored in bit-sliced form.  One machine word evaluates 64
// input assignments at once, instead of storing one uint16_t per assignment.
class simulator
{
public:
  explicit simulator(CircuitGraph &graph) : graph(graph)
  {
    const auto line_count = graph.get_lines().size();
    sim_info.resize(line_count);
    live_gate.assign(graph.get_gates().size(), false);
    find_output_cone_and_support();

    if (active_inputs.size() >= std::numeric_limits<size_t>::digits)
      throw std::length_error("truth table support is too large for this platform");

    pattern_num = size_t{1} << active_inputs.size();
    word_num = (pattern_num + word_bits - 1) / word_bits;
  }

  bool simulate()
  {
    graph.match_logic_depth();
    initialize_inputs();

    std::vector<size_t> remaining_uses(graph.get_lines().size(), 0);
    for (size_t gate_id = 0; gate_id < graph.get_gates().size(); ++gate_id)
      if (live_gate[gate_id])
        for (const auto input : graph.get_gates()[gate_id].get_inputs())
          ++remaining_uses[input];

    for (const auto &level : graph.get_m_node_level())
    {
      for (const auto gate_id : level)
      {
        if (!live_gate[gate_id])
          continue;
        simulate_gate(gate_id);
        for (const auto input : graph.get_gates()[gate_id].get_inputs())
        {
          if (--remaining_uses[input] == 0 && !graph.get_lines()[input].is_output)
            std::vector<uint64_t>().swap(sim_info[input]);
        }
      }
    }
    return true;
  }

  void print_simulation_result(std::ostream &os = std::cout) const
  {
    for (const auto input : active_inputs)
      os << graph.get_lines()[input].name << ' ';
    os << ": ";
    for (const auto output : graph.get_outputs())
      os << graph.get_lines()[output].name << ' ';
    os << '\n';

    for (size_t pattern = 0; pattern < pattern_num; ++pattern)
    {
      const size_t assignment = pattern_num - 1 - pattern;
      for (size_t input = 0; input < active_inputs.size(); ++input)
        os << ((assignment >> input) & 1u) << "  ";
      os << ":  ";
      for (const auto output : graph.get_outputs())
        os << value_at(sim_info[output], pattern) << ' ';
      os << '\n';
    }
  }

  void print_simulation_summary(std::ostream &os = std::cout,
                                const bool print_truth_tables = true) const
  {
    os << "  Inputs  : " << graph.get_inputs().size() << '\n';
    os << "  Cone support : " << active_inputs.size();
    if (active_inputs.size() != graph.get_inputs().size())
      os << " (unused inputs are not expanded)";
    os << '\n';
    os << "  Input order (LSB -> MSB) : ";
    for (size_t index = 0; index < active_inputs.size(); ++index)
    {
      if (index != 0)
        os << ", ";
      os << graph.get_lines()[active_inputs[index]].name;
    }
    os << '\n';
    os << "  Outputs : " << graph.get_outputs().size() << '\n';
    if (!print_truth_tables)
    {
      os << "  Truth tables : not printed\n";
      return;
    }

    os << "  Truth tables (over cone support)\n";

    for (const auto output : graph.get_outputs())
      os << "    " << std::left << std::setw(16) << graph.get_lines()[output].name << ' '
         << (is_const_zero(output) ? "CONST ZERO" : output_truth_table_hex(output)) << '\n';
  }

private:
  static constexpr size_t word_bits = 64;
  static unsigned value_at(const std::vector<uint64_t> &table, const size_t pattern)
  {
    return static_cast<unsigned>((table[pattern / word_bits] >> (pattern % word_bits)) & 1u);
  }

  bool is_const_zero(const line_idx output) const
  {
    return std::all_of(sim_info[output].begin(), sim_info[output].end(),
                       [](const uint64_t word) { return word == 0; });
  }

  void find_output_cone_and_support()
  {
    std::vector<bool> live_line(graph.get_lines().size(), false);
    std::deque<line_idx> pending(graph.get_outputs().begin(), graph.get_outputs().end());
    while (!pending.empty())
    {
      const auto line = pending.front();
      pending.pop_front();
      if (live_line[line])
        continue;
      live_line[line] = true;
      const auto source = graph.get_lines()[line].source;
      if (source == NULL_INDEX)
        continue;
      live_gate[source] = true;
      for (const auto input : graph.get_gates()[source].get_inputs())
        pending.push_back(input);
    }

    // Preserve the graph's public input order, which defines LSB -> MSB.
    for (const auto input : graph.get_inputs())
      if (live_line[input])
      {
        active_inputs.push_back(input);
      }
  }

  void initialize_inputs()
  {
    for (size_t input_index = 0; input_index < active_inputs.size(); ++input_index)
    {
      auto &table = sim_info[active_inputs[input_index]];
      table.assign(word_num, 0);
      for (size_t pattern = 0; pattern < pattern_num; ++pattern)
      {
        const size_t assignment = pattern_num - 1 - pattern;
        if ((assignment >> input_index) & 1u)
          table[pattern / word_bits] |= uint64_t{1} << (pattern % word_bits);
      }
    }
  }

  void simulate_gate(const gate_idx gate_id)
  {
    const auto &gate = graph.get_gates()[gate_id];
    const auto &inputs = gate.get_inputs();
    const auto &type = gate.get_type();
    auto &output = sim_info[gate.get_output()];
    output.assign(word_num, 0);

    const size_t combinations = size_t{1} << inputs.size();
    for (size_t truth_index = 0; truth_index < combinations; ++truth_index)
    {
      // stp_vec stores the complement of each Boolean truth-table entry.
      if (type(combinations - truth_index) != 0)
        continue;
      for (size_t word_index = 0; word_index < word_num; ++word_index)
      {
        uint64_t minterm = ~uint64_t{0};
        for (size_t input_index = 0; input_index < inputs.size(); ++input_index)
        {
          const bool one = (truth_index >> (inputs.size() - 1 - input_index)) & 1u;
          const uint64_t value = sim_info[inputs[input_index]][word_index];
          minterm &= one ? value : ~value;
        }
        output[word_index] |= minterm;
      }
    }

    const size_t valid_bits = pattern_num % word_bits;
    if (valid_bits != 0)
      output.back() &= (uint64_t{1} << valid_bits) - 1;
  }

  std::string output_truth_table_hex(const line_idx output) const
  {
    static constexpr char digits[] = "0123456789ABCDEF";
    const size_t padding = (4 - pattern_num % 4) % 4;
    std::string result = "0x";
    result.reserve(2 + (pattern_num + padding) / 4);
    size_t bit = 0;
    if (padding != 0)
    {
      unsigned nibble = 0;
      for (size_t shift = padding; shift < 4; ++shift)
        nibble = (nibble << 1) | value_at(sim_info[output], bit++);
      result.push_back(digits[nibble]);
    }
    while (bit < pattern_num)
    {
      unsigned nibble = 0;
      for (size_t shift = 0; shift < 4; ++shift)
        nibble = (nibble << 1) | value_at(sim_info[output], bit++);
      result.push_back(digits[nibble]);
    }
    return result;
  }

  std::vector<std::vector<uint64_t>> sim_info;
  std::vector<line_idx> active_inputs;
  std::vector<bool> live_gate;
  CircuitGraph &graph;
  size_t pattern_num = 0;
  size_t word_num = 0;
};
