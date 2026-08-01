// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <stp/core/circuit_graph.hpp>
#include <stp/io/expr_parser.hpp>

class SimulationThreadPool
{
public:
  explicit SimulationThreadPool(const unsigned worker_count) : worker_count(worker_count)
  {
    workers.reserve(worker_count);
    for (unsigned id = 0; id < worker_count; ++id)
      workers.emplace_back([this, id]() { worker_loop(id); });
  }

  ~SimulationThreadPool()
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
    }
    work_available.notify_all();
    for (auto &worker : workers)
      worker.join();
  }

  void parallel_for(const size_t count, const std::function<void(size_t, size_t)> &function)
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      current_count = count;
      current_function = function;
      active_workers = worker_count;
      ++generation;
    }
    work_available.notify_all();

    std::unique_lock<std::mutex> lock(mutex);
    work_finished.wait(lock, [this]() { return active_workers == 0; });
  }

private:
  void worker_loop(const unsigned id)
  {
    size_t seen_generation = 0;
    while (true)
    {
      std::function<void(size_t, size_t)> function;
      size_t count = 0;
      size_t task_generation = 0;
      {
        std::unique_lock<std::mutex> lock(mutex);
        work_available.wait(lock, [this, seen_generation]()
                            { return stopping || generation != seen_generation; });
        if (stopping)
          return;
        task_generation = generation;
        count = current_count;
        function = current_function;
      }

      const size_t begin = (count * id) / worker_count;
      const size_t end = (count * (id + 1)) / worker_count;
      function(begin, end);

      {
        std::lock_guard<std::mutex> lock(mutex);
        seen_generation = task_generation;
        if (--active_workers == 0)
          work_finished.notify_one();
      }
    }
  }

  unsigned worker_count;
  std::vector<std::thread> workers;
  std::mutex mutex;
  std::condition_variable work_available;
  std::condition_variable work_finished;
  std::function<void(size_t, size_t)> current_function;
  size_t current_count = 0;
  size_t generation = 0;
  unsigned active_workers = 0;
  bool stopping = false;
};

namespace stp::detail
{
inline void
apply_local_truth_table_bit_sliced(const std::vector<uint8_t> &local_truth,
                                   const std::vector<const std::vector<uint64_t> *> &inputs,
                                   const size_t pattern_count, std::vector<uint64_t> &output)
{
  if (inputs.size() >= std::numeric_limits<size_t>::digits)
    throw std::invalid_argument("local truth table has too many inputs");

  const size_t assignment_count = size_t{1} << inputs.size();
  if (local_truth.size() != assignment_count)
    throw std::invalid_argument("local truth table size does not match its input count");

  constexpr size_t bits_per_word = 64;
  const size_t word_count = (pattern_count + bits_per_word - 1) / bits_per_word;
  for (const auto *input : inputs)
    if (input == nullptr || input->size() < word_count)
      throw std::invalid_argument("local truth table input is not initialized");

  output.assign(word_count, 0);
  for (size_t word = 0; word < word_count; ++word)
  {
    uint64_t result = 0;
    for (size_t assignment = 0; assignment < assignment_count; ++assignment)
    {
      if (local_truth[assignment] == 0)
        continue;

      uint64_t minterm = ~uint64_t{0};
      for (size_t input_index = 0; input_index < inputs.size(); ++input_index)
      {
        const bool one = ((assignment >> (inputs.size() - 1 - input_index)) & size_t{1}) != 0;
        const uint64_t value = (*inputs[input_index])[word];
        minterm &= one ? value : ~value;
      }
      result |= minterm;
    }
    output[word] = result;
  }

  const size_t valid_bits = pattern_count % bits_per_word;
  if (valid_bits != 0 && !output.empty())
    output.back() &= (uint64_t{1} << valid_bits) - 1;
}
} // namespace stp::detail

enum class SimulationBackend
{
  StpCpu,
  StpGpu,
  BitSlice,
  HybridCpu,
  HybridGpu
};

inline const char *simulation_backend_name(const SimulationBackend backend)
{
  switch (backend)
  {
    case SimulationBackend::StpCpu:
      return "stp-cpu";
    case SimulationBackend::StpGpu:
      return "stp-gpu";
    case SimulationBackend::BitSlice:
      return "bitslice";
    case SimulationBackend::HybridCpu:
      return "hybrid-cpu";
    case SimulationBackend::HybridGpu:
      return "hybrid-gpu";
  }
  return "unknown";
}

// Truth tables are stored in bit-sliced form.  One machine word evaluates 64
// input assignments at once, instead of storing one uint16_t per assignment.
class simulator
{
public:
  explicit simulator(CircuitGraph &graph,
                     const SimulationBackend backend = SimulationBackend::StpCpu,
                     const unsigned requested_threads = 0)
      : graph(graph), backend(backend)
  {
    const auto line_count = graph.get_lines().size();
    sim_info.resize(line_count);
    live_gate.assign(graph.get_gates().size(), false);
    find_output_cone_and_support();

    if (active_inputs.size() >= std::numeric_limits<size_t>::digits)
      throw std::length_error("truth table support is too large for this platform");

    pattern_num = size_t{1} << active_inputs.size();
    word_num = (pattern_num + word_bits - 1) / word_bits;

    thread_count = requested_threads == 0 ? std::min(8u, std::thread::hardware_concurrency())
                                          : requested_threads;
    if (thread_count == 0)
      thread_count = 1;
    thread_count = static_cast<unsigned>(std::min<size_t>(thread_count, word_num));
    if (backend == SimulationBackend::BitSlice && thread_count > 1 &&
        word_num >= parallel_word_threshold)
      thread_pool = std::make_unique<SimulationThreadPool>(thread_count);
    else
      thread_count = 1;
  }

  bool simulate()
  {
    graph.match_logic_depth();

    switch (backend)
    {
      case SimulationBackend::StpCpu:
        return simulate_stp(false, false);
      case SimulationBackend::StpGpu:
        return simulate_stp(true, false);
      case SimulationBackend::HybridCpu:
        initialize_inputs();
        return simulate_stp(false, true);
      case SimulationBackend::HybridGpu:
        initialize_inputs();
        return simulate_stp(true, true);
      case SimulationBackend::BitSlice:
        initialize_inputs();
        break;
    }

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
    os << "  Backend : " << simulation_backend_name(backend) << '\n';
    os << "  Threads : " << thread_count << '\n';
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
  static constexpr size_t parallel_word_threshold = 4096;
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
    static constexpr uint64_t low_bit_patterns[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                                    0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                                    0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL};

    for (size_t input_index = 0; input_index < active_inputs.size(); ++input_index)
    {
      auto &table = sim_info[active_inputs[input_index]];
      table.resize(word_num);
      if (input_index < 6)
      {
        std::fill(table.begin(), table.end(), low_bit_patterns[input_index]);
        continue;
      }

      const size_t word_bit = input_index - 6;
      for (size_t word_index = 0; word_index < word_num; ++word_index)
        table[word_index] = ((word_index >> word_bit) & 1u) ? 0 : ~uint64_t{0};
    }
  }

  bool simulate_stp(const bool use_gpu, const bool bit_sliced_apply)
  {
    if (use_gpu)
    {
#ifndef ENABLE_CUDA
      std::cerr << "GPU STP backend is unavailable in this build\n";
      return false;
#else
      if (!Get_Total_Thread_Num())
      {
        std::cerr << "GPU STP backend could not initialize a CUDA device\n";
        return false;
      }
#endif
    }
    stp_lines_flag.assign(graph.get_lines().size(), false);

    if (!bit_sliced_apply)
    {
      scalar_sim_info.clear();
      scalar_sim_info.resize(graph.get_lines().size());
      for (size_t input_index = 0; input_index < active_inputs.size(); ++input_index)
      {
        const auto line = active_inputs[input_index];
        auto &values = scalar_sim_info[line];
        values.resize(pattern_num);
        for (size_t pattern = 0; pattern < pattern_num; ++pattern)
        {
          const size_t assignment = pattern_num - 1 - pattern;
          values[pattern] = static_cast<uint16_t>((assignment >> input_index) & size_t{1});
        }
      }
    }

    for (const auto line : active_inputs)
      stp_lines_flag[line] = true;

    const auto nodes = get_stp_simulation_nodes();
    for (const auto node : nodes)
      simulate_stp_node(node, bit_sliced_apply, use_gpu);

    for (const auto output : graph.get_outputs())
    {
      if (bit_sliced_apply && sim_info[output].size() != word_num)
      {
        std::cerr << "hybrid STP simulation did not produce output "
                  << graph.get_lines()[output].name << '\n';
        return false;
      }
      if (!bit_sliced_apply && scalar_sim_info[output].size() != pattern_num)
      {
        std::cerr << "STP simulation did not produce output " << graph.get_lines()[output].name
                  << '\n';
        return false;
      }
    }

    if (!bit_sliced_apply)
    {
      for (const auto output : graph.get_outputs())
      {
        auto &packed = sim_info[output];
        packed.assign(word_num, 0);
        for (size_t pattern = 0; pattern < pattern_num; ++pattern)
          if (scalar_sim_info[output][pattern] != 0)
            packed[pattern / word_bits] |= uint64_t{1} << (pattern % word_bits);
      }
      scalar_sim_info.clear();
    }

    stp_lines_flag.clear();
    return true;
  }

  std::deque<gate_idx> get_stp_simulation_nodes()
  {
    std::deque<gate_idx> nodes;
    for (const auto &level : graph.get_m_node_level())
    {
      for (const auto node_id : level)
      {
        const auto &node = graph.get_gates()[node_id];
        const auto &output = graph.get_lines()[node.get_output()];
        if (output.is_output || output.destination_gates.size() > stp_fanout_limit)
        {
          nodes.clear();
          stp_lines_flag[node.get_output()] = true;
          nodes.push_back(node_id);
          cut_stp_tree(nodes);
        }
        nodes.push_back(node_id);
      }
    }
    nodes.clear();

    for (const auto &level : graph.get_m_node_level())
      for (const auto node_id : level)
        if (stp_lines_flag[graph.get_gates()[node_id].get_output()])
          nodes.push_back(node_id);
    return nodes;
  }

  void cut_stp_tree(std::deque<gate_idx> &nodes)
  {
    std::deque<gate_idx> pending;
    const auto &lines = graph.get_lines();
    const auto &gates = graph.get_gates();
    while (!nodes.empty())
    {
      pending.clear();
      pending.push_back(nodes.front());
      nodes.pop_front();
      int branch_count = 0;
      while (!pending.empty())
      {
        for (const auto input : gates[pending.front()].get_inputs())
        {
          ++branch_count;
          if (!stp_lines_flag[input] && lines[input].source != NULL_INDEX)
            pending.push_back(lines[input].source);
        }
        pending.pop_front();
        --branch_count;
        if (branch_count > stp_max_branch)
          break;
      }

      for (const auto node_id : pending)
      {
        stp_lines_flag[gates[node_id].get_output()] = true;
        nodes.push_back(node_id);
      }
    }
  }

  void simulate_stp_node(const gate_idx node_id, const bool bit_sliced_apply, const bool use_cuda)
  {
    const auto &node = graph.get_gates()[node_id];
    const line_idx output = node.get_output();
    std::map<line_idx, int> variable_map;
    std::vector<stp::expr_node> lut_chain;
    build_stp_chain(node_id, lut_chain, variable_map);

    std::vector<int64_t> old_input_order(variable_map.size());
    for (size_t index = 0; index < old_input_order.size(); ++index)
      old_input_order[index] = static_cast<int64_t>(index);

    stp::expr_chain_parser expression(lut_chain, old_input_order, use_cuda);
    const std::vector<stp_data> &root_stp_vec = expression.out_vec;
    const size_t local_pattern_count = size_t{1} << variable_map.size();
    if (root_stp_vec.size() <= local_pattern_count)
      throw std::runtime_error("STP expression produced an invalid truth table");

    std::vector<line_idx> variables(variable_map.size());
    for (const auto &entry : variable_map)
      variables[static_cast<size_t>(entry.second - 1)] = entry.first;

    if (bit_sliced_apply)
    {
      std::vector<uint8_t> local_truth(local_pattern_count);
      for (size_t assignment = 0; assignment < local_pattern_count; ++assignment)
        local_truth[assignment] =
            static_cast<uint8_t>(1 - root_stp_vec[local_pattern_count - assignment]);

      std::vector<const std::vector<uint64_t> *> input_tables;
      input_tables.reserve(variables.size());
      for (const auto variable : variables)
        input_tables.push_back(&sim_info[variable]);

      stp::detail::apply_local_truth_table_bit_sliced(local_truth, input_tables, pattern_num,
                                                      sim_info[output]);
    }
    else
    {
      auto &output_values = scalar_sim_info[output];
      output_values.resize(pattern_num);
      for (size_t pattern = 0; pattern < pattern_num; ++pattern)
      {
        size_t index = 0;
        for (const auto variable : variables)
          index = (index << 1) + scalar_sim_info[variable][pattern];
        output_values[pattern] =
            static_cast<uint16_t>(1 - root_stp_vec[local_pattern_count - index]);
      }
    }
    stp_lines_flag[output] = true;
  }

  void build_stp_chain(const gate_idx node_id, std::vector<stp::expr_node> &lut_chain,
                       std::map<line_idx, int> &variable_map)
  {
    const auto &node = graph.get_gates()[node_id];
    lut_chain.emplace_back(stp::NodeType_Gate, stp::GateType_Lut, 0, 0, node.get_type().vec);

    for (const auto line : node.get_inputs())
    {
      if (stp_lines_flag[line])
      {
        auto position = variable_map.find(line);
        if (position == variable_map.end())
        {
          const int next = static_cast<int>(variable_map.size()) + 1;
          position = variable_map.emplace(line, next).first;
        }
        lut_chain.emplace_back(stp::NodeType_Variable, position->second - 1);
      }
      else
      {
        const auto source = graph.get_lines()[line].source;
        if (source == NULL_INDEX)
          throw std::runtime_error("STP chain reached an uninitialized input");
        build_stp_chain(source, lut_chain, variable_map);
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
    const auto evaluate_words = [&](const size_t begin, const size_t end)
    {
      for (size_t word_index = begin; word_index < end; ++word_index)
      {
        uint64_t result = 0;
        for (size_t truth_index = 0; truth_index < combinations; ++truth_index)
        {
          // stp_vec stores the complement of each Boolean truth-table entry.
          if (type(combinations - truth_index) != 0)
            continue;
          uint64_t minterm = ~uint64_t{0};
          for (size_t input_index = 0; input_index < inputs.size(); ++input_index)
          {
            const bool one = (truth_index >> (inputs.size() - 1 - input_index)) & 1u;
            const uint64_t value = sim_info[inputs[input_index]][word_index];
            minterm &= one ? value : ~value;
          }
          result |= minterm;
        }
        output[word_index] = result;
      }
    };

    if (thread_pool)
      thread_pool->parallel_for(word_num, evaluate_words);
    else
      evaluate_words(0, word_num);

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
  std::unique_ptr<SimulationThreadPool> thread_pool;
  std::vector<std::vector<uint16_t>> scalar_sim_info;
  std::vector<bool> stp_lines_flag;
  static constexpr int stp_max_branch = 8;
  static constexpr size_t stp_fanout_limit = 1;
  CircuitGraph &graph;
  SimulationBackend backend = SimulationBackend::StpCpu;
  size_t pattern_num = 0;
  size_t word_num = 0;
  unsigned thread_count = 1;
};
