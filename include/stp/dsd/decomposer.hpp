// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <stp/core/circuit_graph.hpp>

namespace stp
{

class DsdThreadPool
{
public:
  explicit DsdThreadPool(unsigned worker_count) : worker_count(worker_count)
  {
    workers.reserve(worker_count);
    for (unsigned id = 0; id < worker_count; ++id)
      workers.emplace_back([this, id]() { worker_loop(id); });
  }

  ~DsdThreadPool()
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
    }
    work_available.notify_all();
    for (auto &worker : workers)
      worker.join();
  }

  void parallel_for(size_t count, const std::function<void(size_t, size_t)> &function)
  {
    if (count == 0)
      return;
    if (worker_count <= 1)
    {
      function(0, count);
      return;
    }

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
  void worker_loop(unsigned id)
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

private:
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

struct DsdResult
{
  bool valid = false;
  std::string error;
  std::string expression;
  std::vector<std::string> solutions;
  size_t selected_solution = 0;
  size_t nodes = 0;
  size_t logic_levels = 0;
};

class DsdDecomposer
{
public:
  DsdResult run(const std::string &truth_table, const std::vector<std::string> &variables_msb,
                const std::string &output_name, const std::string &bench_filename,
                unsigned thread_count = 0, bool enumerate_all = false)
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

    return run_truth(truth, variables_msb, output_name, bench_filename, thread_count,
                     enumerate_all);
  }

  DsdResult run_incomplete(const std::string &truth_table,
                           const std::vector<std::string> &variables_msb,
                           const std::string &output_name, const std::string &bench_filename,
                           unsigned thread_count = 0, bool enumerate_all = false)
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
    if (!parse_incomplete_truth_table(truth_table, 1U << variables_msb.size(), truth))
      return {false, "truth table must contain only 0, 1, and - and have 2^n entries"};

    return run_truth(truth, variables_msb, output_name, bench_filename, thread_count,
                     enumerate_all);
  }

private:
  DsdResult run_truth(const std::vector<unsigned char> &truth,
                      const std::vector<std::string> &variables_msb, const std::string &output_name,
                      const std::string &bench_filename, unsigned thread_count, bool enumerate_all)
  {

    // CircuitGraph/bench input declarations follow the user-facing order
    // (LSB -> MSB), while the decomposition algorithm uses MSB -> LSB.
    for (auto it = variables_msb.rbegin(); it != variables_msb.rend(); ++it)
      graph.add_input(*it);
    graph.add_output(output_name);

    next_node = 0;
    threads = thread_count == 0 ? std::thread::hardware_concurrency() : thread_count;
    if (threads == 0)
      threads = 1;
    if (threads > 1)
      thread_pool = std::make_unique<DsdThreadPool>(threads);

    std::vector<Candidate> candidates;
    if (enumerate_all)
      candidates = enumerate_candidates(truth, variables_msb);

    std::string expression;
    size_t selected_solution = 0;
    size_t selected_nodes = 0;
    size_t selected_levels = 0;
    DsdResult result{true, {}, {}, {}, 0, 0, 0};
    if (enumerate_all && !candidates.empty())
    {
      CircuitGraph best_graph;
      bool have_best = false;
      size_t best_nodes = 0;
      for (size_t index = 0; index < candidates.size(); ++index)
      {
        CircuitGraph candidate_graph;
        for (auto it = variables_msb.rbegin(); it != variables_msb.rend(); ++it)
          candidate_graph.add_input(*it);
        candidate_graph.add_output(output_name);

        const std::string candidate_expression =
            decompose(candidate_graph, truth, variables_msb, output_name, &candidates[index]);
        candidate_graph.match_logic_depth();
        const size_t candidate_nodes = candidate_graph.get_gates().size();
        const size_t candidate_levels =
            candidate_nodes == 0 ? 0 : static_cast<size_t>(candidate_graph.get_mld() + 1);
        result.solutions.push_back(describe_candidate(candidates[index], output_name,
                                                      candidate_expression, candidate_nodes,
                                                      candidate_levels));

        if (!have_best || candidate_nodes > best_nodes ||
            (candidate_nodes == best_nodes && candidate_levels < selected_levels))
        {
          have_best = true;
          best_nodes = candidate_nodes;
          selected_solution = index;
          selected_nodes = candidate_nodes;
          selected_levels = candidate_levels;
          expression = candidate_expression;
          best_graph = std::move(candidate_graph);
        }
      }
      graph = std::move(best_graph);
    }
    else
    {
      expression = decompose(graph, truth, variables_msb, output_name);
      graph.match_logic_depth();
      selected_nodes = graph.get_gates().size();
      selected_levels = selected_nodes == 0 ? 0 : static_cast<size_t>(graph.get_mld() + 1);
    }

    try
    {
      write_bench(bench_filename);
    }
    catch (const std::exception &error)
    {
      return {false, error.what()};
    }
    result.expression = expression;
    result.selected_solution = selected_solution;
    result.nodes = selected_nodes;
    result.logic_levels = selected_levels;
    return result;
  }
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

  static bool parse_incomplete_truth_table(const std::string &text, size_t length,
                                           std::vector<unsigned char> &truth)
  {
    if (text.size() != length)
      return false;
    truth.clear();
    truth.reserve(length);
    for (const char c : text)
    {
      if (c == '0')
        truth.push_back(0);
      else if (c == '1')
        truth.push_back(1);
      else if (c == '-')
        truth.push_back(2);
      else
        return false;
    }
    return true;
  }

  static stp_vec make_type(const std::vector<unsigned char> &truth)
  {
    stp_vec type(truth.size() + 1);
    type(0) = 2;
    for (size_t i = 0; i < truth.size(); ++i)
      // A don't-care is completed to zero when a concrete LUT is emitted.
      type(static_cast<unsigned>(i + 1)) = truth[i] == 1 ? 0 : 1;
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

  static bool check_candidate(const std::vector<unsigned char> &truth,
                              const std::vector<std::string> &variables, size_t m, size_t mask,
                              Candidate &candidate)
  {
    const size_t n = variables.size();
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
      auto compatible =
          [](const std::vector<unsigned char> &left, const std::vector<unsigned char> &right)
      {
        for (size_t position = 0; position < left.size(); ++position)
          if (left[position] != 2 && right[position] != 2 && left[position] != right[position])
            return false;
        return true;
      };
      auto merge = [](std::vector<unsigned char> pattern, const std::vector<unsigned char> &block)
      {
        for (size_t position = 0; position < pattern.size(); ++position)
          if (pattern[position] == 2)
            pattern[position] = block[position];
        return pattern;
      };

      auto it = std::find_if(patterns.begin(), patterns.end(),
                             [&](const auto &pattern) { return compatible(pattern, block); });
      if (it == patterns.end())
      {
        if (patterns.size() == 2)
          return false;
        patterns.push_back(block);
        // The first block occupies the y=1 half of M_X, and the
        // second block occupies the y=0 half, matching LUT bit order.
        selectors.push_back(static_cast<unsigned char>(patterns.size() == 1 ? 1 : 0));
      }
      else
      {
        const size_t pattern_index = static_cast<size_t>(it - patterns.begin());
        *it = merge(*it, block);
        selectors.push_back(static_cast<unsigned char>(pattern_index == 0 ? 1 : 0));
      }
    }
    if (patterns.size() != 2 || selectors.size() != (1ULL << m))
      return false;

    candidate.bound = std::move(bound);
    candidate.free = std::move(free);
    candidate.inner_truth = std::move(selectors);
    candidate.outer_truth = patterns[0];
    candidate.outer_truth.insert(candidate.outer_truth.end(), patterns[1].begin(),
                                 patterns[1].end());
    return true;
  }

  bool find_candidate(const std::vector<unsigned char> &truth,
                      const std::vector<std::string> &variables, Candidate &candidate,
                      const std::string *retained_variable = nullptr)
  {
    const size_t n = variables.size();
    if (n < 3)
      return false;

    // Keep the original search order between layers.  Candidates in one
    // layer are checked in parallel and the smallest mask is selected.
    for (size_t m = 2; m < n; ++m)
    {
      std::vector<size_t> masks;
      for (size_t mask = 0; mask < (1ULL << n); ++mask)
      {
        if (__builtin_popcountll(mask) == static_cast<int>(m))
          masks.push_back(mask);
      }

      std::mutex best_mutex;
      std::optional<Candidate> best_candidate;
      size_t best_mask = std::numeric_limits<size_t>::max();
      const auto candidate_priority = [&](size_t mask)
      {
        if (retained_variable == nullptr)
          return 0;
        const auto retained = std::find(variables.begin(), variables.end(), *retained_variable);
        if (retained == variables.end())
          return 0;
        const size_t position = static_cast<size_t>(retained - variables.begin());
        // Priority 0 means the previous intermediate signal remains in F.
        return (mask & (1ULL << (n - 1 - position))) == 0 ? 0 : 1;
      };

      const auto check_range = [&](size_t begin, size_t end)
      {
        std::optional<Candidate> local_candidate;
        std::pair<int, size_t> local_key{std::numeric_limits<int>::max(),
                                         std::numeric_limits<size_t>::max()};
        for (size_t index = begin; index < end; ++index)
        {
          const size_t mask = masks[index];
          Candidate current;
          const std::pair<int, size_t> key{candidate_priority(mask), mask};
          if (check_candidate(truth, variables, m, mask, current) && key < local_key)
          {
            local_key = key;
            local_candidate = std::move(current);
          }
        }
        if (local_candidate)
        {
          std::lock_guard<std::mutex> lock(best_mutex);
          const std::pair<int, size_t> best_key{
              retained_variable == nullptr ? 0
                                           : (best_mask == std::numeric_limits<size_t>::max()
                                                  ? std::numeric_limits<int>::max()
                                                  : candidate_priority(best_mask)),
              best_mask};
          if (local_key < best_key)
          {
            best_mask = local_key.second;
            best_candidate = std::move(local_candidate);
          }
        }
      };

      if (thread_pool)
        thread_pool->parallel_for(masks.size(), check_range);
      else
        check_range(0, masks.size());

      if (best_candidate)
      {
        candidate = std::move(*best_candidate);
        return true;
      }
    }
    return false;
  }

  std::vector<Candidate> enumerate_candidates(const std::vector<unsigned char> &truth,
                                              const std::vector<std::string> &variables)
  {
    std::vector<Candidate> all_candidates;
    const size_t n = variables.size();
    if (n < 3)
      return all_candidates;

    for (size_t m = 2; m < n; ++m)
    {
      std::vector<size_t> masks;
      for (size_t mask = 0; mask < (1ULL << n); ++mask)
      {
        if (__builtin_popcountll(mask) == static_cast<int>(m))
          masks.push_back(mask);
      }

      std::vector<std::optional<Candidate>> found(masks.size());
      const auto check_range = [&](size_t begin, size_t end)
      {
        for (size_t index = begin; index < end; ++index)
        {
          Candidate candidate;
          if (check_candidate(truth, variables, m, masks[index], candidate))
            found[index] = std::move(candidate);
        }
      };

      if (thread_pool)
        thread_pool->parallel_for(masks.size(), check_range);
      else
        check_range(0, masks.size());

      // Scan in mask order so enumeration numbers are deterministic.
      for (auto &candidate : found)
      {
        if (candidate)
          all_candidates.push_back(std::move(*candidate));
      }
    }
    return all_candidates;
  }

  static std::string set_string(const std::vector<std::string> &variables)
  {
    std::string result = "{";
    for (size_t i = 0; i < variables.size(); ++i)
    {
      if (i != 0)
        result += ",";
      result += variables[i];
    }
    result += "}";
    return result;
  }

  static std::string describe_candidate(const Candidate &candidate, const std::string &output_name,
                                        const std::string &final_expression, size_t nodes,
                                        size_t logic_levels)
  {
    const std::string mx = lut_hex(make_type(candidate.outer_truth));
    const std::string my = lut_hex(make_type(candidate.inner_truth));
    return "B=" + set_string(candidate.bound) + " F=" + set_string(candidate.free) + " MX=" + mx +
           " MY=" + my + "\n             " + output_name + " = " + final_expression +
           "\n             #nodes = " + std::to_string(nodes) +
           ", #levels = " + std::to_string(logic_levels);
  }

  std::string decompose(CircuitGraph &target_graph, const std::vector<unsigned char> &truth,
                        const std::vector<std::string> &variables, const std::string &output,
                        const Candidate *forced_candidate = nullptr,
                        const std::string *retained_variable = nullptr)
  {
    Candidate candidate;
    if (forced_candidate)
      candidate = *forced_candidate;
    else if (!find_candidate(truth, variables, candidate, retained_variable))
    {
      std::vector<std::string> gate_inputs(variables.rbegin(), variables.rend());
      const Type type = make_type(truth);
      target_graph.add_gate(type, gate_inputs, output);
      return make_expression(lut_hex(type), gate_inputs);
    }

    const std::string inner_name = "dsd_n" + std::to_string(next_node++);
    const std::string inner_expression =
        decompose(target_graph, candidate.inner_truth, candidate.bound, inner_name);

    std::vector<std::string> outer_variables{inner_name};
    outer_variables.insert(outer_variables.end(), candidate.free.begin(), candidate.free.end());
    std::string expression = decompose(target_graph, candidate.outer_truth, outer_variables, output,
                                       nullptr, &inner_name);
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
  unsigned threads = 1;
  std::unique_ptr<DsdThreadPool> thread_pool;
};

} // namespace stp
