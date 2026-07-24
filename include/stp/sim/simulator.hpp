// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#include <bitset>
#include <climits>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <stack>
#include <stp/core/circuit_graph.hpp>
#include <stp/io/expr_parser.hpp>
#include <stp/utils/stp_utils.hpp>
#include <string>
#include <vector>

#pragma once

using need_sim_nodes = std::deque<gate_idx>;
using line_sim_info = std::vector<u_int16_t>;

using namespace stp;

class simulator
{
public:
  simulator(CircuitGraph &graph) : graph(graph)
  {
    pattern_num = pow(2, graph.get_inputs().size()); // pattern num
    max_branch = 8;
    sim_info.resize(graph.get_lines().size());
    lines_flag.resize(graph.get_lines().size(), false);

    for (const line_idx &line_id : graph.get_inputs())
    {
      sim_info[line_id].resize(pattern_num);
      lines_flag[line_id] = true;
    }

    for (int i = 0; i < pattern_num; i++)
    {
      std::bitset<32> pattern(pattern_num - 1 - i);
      for (size_t j = 0; j < graph.get_inputs().size(); j++)
      {
        sim_info[graph.get_inputs()[j]][i] = pattern[j];
      }
    }
  }

  bool simulate()
  {
    graph.match_logic_depth();

    need_sim_nodes nodes = get_need_nodes();

    for (const auto &node : nodes)
    {
      single_node_sim(node);
    }

    // print_simulation_result();
    return true;
  }

  void print_simulation_result()
  {
    for (const auto &input_id : graph.get_inputs())
    {
      std::cout << graph.get_lines()[input_id].name << " ";
    }
    std::cout << ": ";
    for (const auto &output_id : graph.get_outputs())
    {
      std::cout << graph.get_lines()[output_id].name << " ";
    }
    std::cout << std::endl;
    for (int i = 0; i < pattern_num; i++)
    {
      for (const auto &input_id : graph.get_inputs())
      {
        std::cout << sim_info[input_id][i] << "  ";
      }
      std::cout << ":  ";
      for (const auto &output_id : graph.get_outputs())
      {
        std::cout << sim_info[output_id][i] << " ";
      }
      std::cout << std::endl;
    }
  }

  void print_simulation_summary(std::ostream &os = std::cout) const
  {
    os << "  Inputs  : " << graph.get_inputs().size() << '\n';
    os << "  Outputs : " << graph.get_outputs().size() << '\n';
    os << "  Truth tables\n";

    for (const auto &output_id : graph.get_outputs())
    {
      os << "    " << std::left << std::setw(16) << graph.get_lines()[output_id].name << " "
         << output_truth_table_hex(output_id) << '\n';
    }
  }

private:
  std::string output_truth_table_hex(const line_idx output_id) const
  {
    std::string bits;
    bits.reserve(pattern_num);
    for (const auto value : sim_info[output_id])
    {
      bits.push_back(value == 0 ? '0' : '1');
    }

    const auto padding = (4 - bits.size() % 4) % 4;
    bits.insert(0, padding, '0');

    std::stringstream stream;
    stream << "0x";
    print_hex(bits, stream);
    return stream.str();
  }

  bool is_simulated(const line_idx id)
  {
    return sim_info[id].size() == pattern_num;
  }

  need_sim_nodes get_need_nodes()
  {
    need_sim_nodes nodes;
    for (const auto &nodes_id : graph.get_m_node_level())
    {
      for (const auto &node_id : nodes_id)
      {
        const auto &node = graph.get_gates()[node_id];             // get node
        const auto &output = graph.get_lines()[node.get_output()]; // get line

        if (output.is_output || output.destination_gates.size() > fanout_limit)
        {
          nodes.clear();
          lines_flag[node.get_output()] = true;
          nodes.push_back(node_id);
          cut_tree(nodes);
        }

        nodes.push_back(node_id);
      }
    }
    nodes.clear();

    for (unsigned level = 0; level <= graph.get_mld(); level++)
    {
      for (const auto &node_id : graph.get_m_node_level()[level])
      {
        line_idx output = graph.get_gates()[node_id].get_output();
        // lines_flag[output] = true;
        if (lines_flag[output] == true)
        {
          nodes.push_back(node_id);
        }
      }
    }
    return nodes;
  }

  // BFS
  void cut_tree(need_sim_nodes &nodes)
  {
    need_sim_nodes temp_nodes;
    const auto &graph_lines = graph.get_lines(); // get all lines
    const auto &graph_gates = graph.get_gates(); // get all nodes

    while (!nodes.empty())
    {
      temp_nodes.clear();
      temp_nodes.push_back(nodes.front());
      nodes.pop_front();
      int count = 0;
      while (1)
      {
        for (const auto &input : graph_gates[temp_nodes.front()].get_inputs())
        {
          count++;
          if (lines_flag[input] == false)
          {
            temp_nodes.push_back(graph_lines[input].source);
          }
        }
        temp_nodes.pop_front();
        count--;
        if (count > max_branch || temp_nodes.empty())
        {
          break;
        }
      }

      for (const auto &node_id : temp_nodes)
      {
        lines_flag[graph_gates[node_id].get_output()] = true;
        nodes.push_back(node_id);
      }
    }
  }

  void single_node_sim(const gate_idx node_id)
  {
    const auto &node = graph.get_gates()[node_id];
    const gate_idx &output = node.get_output();
    std::map<line_idx, int> map;
    // m_chain matrix_chain;
    std::vector<expr_node> lut_chain;
    get_node_matrix(node_id, lut_chain, map);
    std::vector<int64_t> old_pi_index(map.size());

    for (int i = 0; i < map.size(); i++)
    {
      old_pi_index[i] = i;
    }

    expr_chain_parser lut(lut_chain, old_pi_index);
    std::vector<stp_data> root_stp_vec = lut.out_vec;
    std::vector<line_idx> variable(map.size());
    int inputs_number = variable.size();
    for (const auto &temp : map)
      variable[temp.second - 1] = temp.first;
    sim_info[output].resize(pattern_num);
    int idx;
    int bits = 1 << inputs_number;

    for (int i = 0; i < pattern_num; i++)
    {
      idx = 0;
      for (int j = 0; j < inputs_number; j++)
      {
        idx = (idx << 1) + sim_info[variable[j]][i];
      }
      idx = bits - idx;
      sim_info[output][i] = 1 - root_stp_vec[idx];
    }
    lines_flag[output] = true;
  }

  void get_node_matrix(const gate_idx node_id, std::vector<expr_node> &lut_chain,
                       std::map<line_idx, int> &map)
  {
    const auto &node = graph.get_gates()[node_id];

    lut_chain.emplace_back(NodeType_Gate, GateType_Lut, 0, 0, node.get_type().vec);

    int temp;
    for (const auto &line_id : node.get_inputs())
    {
      if (lines_flag[line_id])
      {
        if (map.find(line_id) == map.end())
        {
          temp = map.size() + 1;
          map.emplace(line_id, map.size() + 1);
        }
        else
          temp = map.at(line_id);
        expr_node new_var(NodeType_Variable, temp - 1);
        lut_chain.emplace_back(new_var);
      }
      else
      {
        get_node_matrix(graph.get_lines()[line_id].source, lut_chain, map);
      }
    }
  }

  bool check_sim_info()
  {
    for (int i = 0; i < sim_info.size(); i++)
    {
      if (sim_info[i].size() != pattern_num)
      {
        std::cout << "!" << std::endl;
        return false;
      }
      for (int j = 0; j < sim_info[i].size(); j++)
      {
        if (sim_info[i][j] != 0 && sim_info[i][j] != 1)
        {
          std::cout << "!" << std::endl;
          return false;
        }
      }
    }
    std::cout << "^ ^" << std::endl;
    return true;
  }

private:
  std::vector<line_sim_info> sim_info;
  std::vector<int> time_interval;
  std::vector<bool> lines_flag;
  CircuitGraph &graph;
  int pattern_num;
  int max_branch;
  int fanout_limit = 1;
};
