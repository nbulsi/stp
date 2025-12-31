#include <string>
#include <cstdint>
#include <limits>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <cassert>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stp/utils/stp_vector.hpp>
#include <map>

#ifndef CIRCUIT_GRAPH_H
#define CIRCUIT_GRAPH_H
#pragma once

#define NULL_INDEX -1
#define NO_LEVEL -10000

using gate_idx = int;
using line_idx = int; 
using node_level = int;

using Type = stp_vec;

class Gate;
class CircuitGraph;

struct Line
{
	void connect_as_input(gate_idx gate)
	{
		destination_gates.insert(gate);
	}

	gate_idx source = NULL_INDEX;  // nullptr means input port
	std::set<gate_idx> destination_gates;
	bool is_input = false;
	bool is_output = false;
	int id_line = NULL_INDEX;
	std::string name;
};


class Gate
{
public:
	Gate(Type type, line_idx output, std::vector<line_idx> &&inputs) : m_type(type), m_inputs(inputs), m_output(output) {}

	Type get_type() const { return m_type; }
	Type &type() { return m_type; }

	const std::vector<line_idx> &get_inputs() const { return m_inputs; }
	std::vector<line_idx> &inputs() { return m_inputs; }

	const line_idx& get_output() const { return m_output; }
	line_idx &output() { return m_output; }

	const int& get_id() const { return m_id; }
	int &id() { return m_id; }

	const int& get_level() const { return m_level; }
	int &level() { return m_level; }

	bool is_input() const { return m_type.cols() == 0; }

	int make_gate_name(Type type)
	{
		int lut = 0;
		for(int i = 1; i < type.cols(); i++)
		{
			lut = (lut << 1) + 1 - type(i);
		}
		return lut;
	}
	bool calculated = false;
private:
	Type m_type;
	int m_id = NULL_INDEX;
	node_level m_level = NO_LEVEL;
	std::vector<line_idx> m_inputs;
	line_idx m_output = NULL_INDEX;

};

class CircuitGraph
{
public:

	CircuitGraph()
	{
		m_gates.reserve(5000u);
		m_lines.reserve(5000u);
	}
	line_idx add_input(const std::string &name)
	{
		line_idx p_line = ensure_line(name);
		if (!m_lines[p_line].is_input) 
		{
			m_lines[p_line].is_input = true;
			m_inputs.push_back(p_line);
		}
		return p_line;
	}
	
	line_idx add_output(const std::string& name)
	{
		line_idx p_line = ensure_line(name);
		if (!m_lines[p_line].is_output) 
		{
			m_lines[p_line].is_output = true;
			m_outputs.push_back(p_line);
		}
		return p_line;
	}
	
	gate_idx add_gate(Type type, const std::vector<std::string>& input_names, const std::string& output_name)
	{
		std::vector<line_idx> inputs;

		for (int i = input_names.size() - 1; i >= 0; i--) 
		{
			line_idx p_input = ensure_line(input_names[i]);
			inputs.push_back(p_input);
		}
	
		line_idx p_output = ensure_line(output_name);
	
		m_gates.emplace_back(type, p_output, std::move(inputs));
		gate_idx gate = m_gates.size() - 1;
		m_lines[p_output].source = gate;
		m_gates.back().id() = gate; 
		
		for (size_t i = 0; i < m_gates[gate].get_inputs().size(); ++i) {
			m_lines[m_gates[gate].get_inputs().at(i)].connect_as_input(gate);
		}
		return gate;
	}

	line_idx line(const std::string &name)
	{
		auto it = m_name_to_line_idx.find(name);
	
		if (it != m_name_to_line_idx.end())
		{
			return it->second;
		}
	
		return NULL_INDEX;
	}
	
	const line_idx get_line(const std::string &name) const
	{
		auto it = m_name_to_line_idx.find(name);
	
		if (it != m_name_to_line_idx.end())
		{
			return it->second;
		}
		return NULL_INDEX;
	}

	const std::vector<line_idx> &get_inputs() const
	{
		return m_inputs;
	}

	const std::vector<line_idx> &get_outputs() const
	{
		return m_outputs;
	}

	const std::vector<Gate> &get_gates() const
	{
		return m_gates;
	}
	
	std::vector<Gate> &get_gates()
	{
		return m_gates;
	}
	
	const std::vector<Line> &get_lines() const
	{
		return m_lines;
	}
	const Gate& get_gate(const gate_idx& idx) const { return m_gates[idx]; }
	Gate& gate(const gate_idx& idx) { return m_gates[idx]; }

	const Line& get_line(const line_idx &idx) const { return m_lines[idx]; }
	Line& line(const line_idx &idx) { return m_lines[idx]; }
	std::vector<line_idx> &inputs() { return m_inputs; }
	std::vector<line_idx> &outputs() { return m_outputs; }
	std::vector<Line> &lines() { return m_lines; }

	const std::vector<std::vector<gate_idx>>& get_m_node_level() const { return m_node_level; }
	std::vector<std::vector<gate_idx>>& get_m_node_level() { return m_node_level; }

	const int& get_mld() const { return max_logic_depth; }

	void match_logic_depth()
	{
		for(int i = 0, num = m_outputs.size(); i < num; i++)
		{
			int level = compute_node_depth(m_lines[m_outputs[i]].source);
			if(level > max_logic_depth)
				max_logic_depth = level;
		}
		m_node_level.resize(max_logic_depth + 1);
		for(int i = 0; i < m_gates.size(); i++)
		{
			m_node_level[m_gates[i].get_level()].push_back(i);
		}
	}
	void print_graph()
	{
		for(unsigned i = 0, length = m_inputs.size(); i < length; i++)
		{
			std::cout << "INPUT(" << m_lines[m_inputs[i]].name << ")" << std::endl;
		}
		for(unsigned i = 0, length = m_outputs.size(); i < length; i++)
		{
			std::cout << "OUTPUT(" << m_lines[m_outputs[i]].name << ")" << std::endl;
		}
		for(unsigned i = 0, length = m_gates.size(); i < length; i++)
		{
			auto& gate = m_gates[i];
			std::cout << m_lines[gate.get_output()].name << " = LUT 0x" << std::hex << int(gate.make_gate_name(gate.get_type())) << "(";
			std::vector<std::string> inputs_name;
			for(int i = gate.get_inputs().size() - 1; i > -1; i--)
			{
				inputs_name.push_back(m_lines[gate.get_inputs()[i]].name);
				inputs_name.push_back(", ");
			}
			inputs_name.pop_back();
			for(const auto& temp : inputs_name)
				std::cout << temp;
			std::cout << ")" << std::endl;
		}
	}

private:
	line_idx ensure_line(const std::string& name)
	{
		auto it = m_name_to_line_idx.find(name);

		if (it != m_name_to_line_idx.end()) {
			return it->second;
		}

		m_lines.emplace_back();
		Line& line = m_lines.back();

		line.name = name;
		line.id_line = m_lines.size() - 1;

		m_name_to_line_idx[name] = m_lines.size() - 1;

		return line.id_line;
	}

	int compute_node_depth(const gate_idx g_id)
	{
		Gate& gate = m_gates[g_id];

		if(gate.get_level() != NO_LEVEL) 
			return gate.get_level();	
		int max_depth = NO_LEVEL;
		int level = -1;
		for(const auto& child : gate.get_inputs())
		{
			if(m_lines[child].is_input)
				continue;
			level = compute_node_depth(m_lines[child].source);
			if(level > max_depth)
			{
				max_depth = level;
			}
		}
		if(max_depth == NO_LEVEL) 
			max_depth = -1;
		m_gates[g_id].level() = max_depth + 1;
		return m_gates[g_id].level();
	}

private:
	std::vector<Line> m_lines;
	std::vector<Gate> m_gates;  
  
	std::vector<line_idx> m_inputs;
	std::vector<line_idx> m_outputs;

	std::vector<std::vector<gate_idx>> m_node_level;
	int max_logic_depth = -1;
	
public:
	std::unordered_map<std::string, line_idx> m_name_to_line_idx;
};
#endif