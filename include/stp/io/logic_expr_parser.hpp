// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <stp/core/circuit_graph.hpp>

class LogicExprParser
{
public:
  bool parse(const std::string &expression, CircuitGraph &graph, std::string &error,
             const std::vector<std::string> &input_order = {})
  {
    input = &expression;
    circuit = &graph;
    position = 0;
    temporary_index = 0;

    std::string root = parse_expression(error);
    skip_whitespace();
    if (!error.empty())
      return false;
    if (position != input->size())
    {
      error = "unexpected text at position " + std::to_string(position);
      return false;
    }

    const std::string output = "$expr_output";
    add_gate("2", {root}, output); // Buffer a variable-only expression as well.
    circuit->add_output(output);
    return order_inputs(input_order, error);
  }

private:
  std::string parse_expression(std::string &error)
  {
    skip_whitespace();
    if (position == input->size())
    {
      error = "expected an expression";
      return {};
    }

    if ((*input)[position] != '(')
    {
      const std::string variable = parse_identifier(error);
      if (!error.empty())
        return {};
      circuit->add_input(variable);
      return variable;
    }

    ++position;
    const std::string operation = parse_identifier(error);
    if (!error.empty())
      return {};

    std::vector<std::string> operands;
    while (true)
    {
      skip_whitespace();
      if (position == input->size())
      {
        error = "missing ')' for " + operation;
        return {};
      }
      if ((*input)[position] == ')')
      {
        ++position;
        break;
      }
      operands.push_back(parse_expression(error));
      if (!error.empty())
        return {};
    }

    return build_operation(operation, operands, error);
  }

  std::string build_operation(std::string operation, const std::vector<std::string> &operands,
                              std::string &error)
  {
    for (char &character : operation)
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    if (operation == "not")
    {
      if (operands.size() != 1)
        error = "not expects exactly one operand";
      else
        return add_gate("1", operands, next_temporary());
      return {};
    }

    const std::string lut = binary_lut(operation);
    if (lut.empty())
    {
      error = "unknown operation '" + operation + "'";
      return {};
    }
    if (operands.size() < 2)
    {
      error = operation + " expects at least two operands";
      return {};
    }

    std::string result = operands.front();
    for (size_t i = 1; i < operands.size(); ++i)
      result = add_gate(lut, {result, operands[i]}, next_temporary());
    return result;
  }

  std::string parse_identifier(std::string &error)
  {
    skip_whitespace();
    if (position == input->size() ||
        !(std::isalpha(static_cast<unsigned char>((*input)[position])) ||
          (*input)[position] == '_'))
    {
      error = "expected an identifier at position " + std::to_string(position);
      return {};
    }

    const size_t start = position++;
    while (
        position < input->size() &&
        (std::isalnum(static_cast<unsigned char>((*input)[position])) || (*input)[position] == '_'))
      ++position;
    return input->substr(start, position - start);
  }

  void skip_whitespace()
  {
    while (position < input->size() && std::isspace(static_cast<unsigned char>((*input)[position])))
      ++position;
  }

  std::string next_temporary()
  {
    return "$expr_" + std::to_string(temporary_index++);
  }

  std::string add_gate(const std::string &hex, const std::vector<std::string> &inputs,
                       const std::string &output)
  {
    stp_vec type(inputs.size() == 1 ? 3 : 5);
    type(0) = 2;
    const unsigned value = static_cast<unsigned>(std::stoi(hex, nullptr, 16));
    const unsigned entries = 1U << inputs.size();
    for (unsigned i = 0; i < entries; ++i)
      type(i + 1) = 1U - ((value >> (entries - 1 - i)) & 1U);
    circuit->add_gate(type, inputs, output);
    return output;
  }

  bool order_inputs(const std::vector<std::string> &requested_order, std::string &error)
  {
    std::vector<line_idx> &inputs = circuit->inputs();
    std::unordered_map<std::string, line_idx> input_by_name;
    for (const line_idx input : inputs)
      input_by_name.emplace(circuit->get_line(input).name, input);

    if (requested_order.empty())
    {
      std::sort(inputs.begin(), inputs.end(), [this](const line_idx lhs, const line_idx rhs)
                { return circuit->get_line(lhs).name < circuit->get_line(rhs).name; });
      return true;
    }

    if (requested_order.size() != inputs.size())
    {
      error = "--inputs must list every expression variable exactly once";
      return false;
    }

    std::unordered_set<std::string> seen;
    std::vector<line_idx> ordered_inputs;
    ordered_inputs.reserve(requested_order.size());
    for (const std::string &name : requested_order)
    {
      const auto input = input_by_name.find(name);
      if (input == input_by_name.end() || !seen.insert(name).second)
      {
        error = "--inputs must list every expression variable exactly once";
        return false;
      }
      ordered_inputs.push_back(input->second);
    }
    inputs = std::move(ordered_inputs);
    return true;
  }

  static std::string binary_lut(const std::string &operation)
  {
    if (operation == "and")
      return "8";
    if (operation == "or")
      return "e";
    if (operation == "nand")
      return "7";
    if (operation == "nor")
      return "1";
    if (operation == "xor")
      return "6";
    if (operation == "xnor")
      return "9";
    return {};
  }

  const std::string *input = nullptr;
  CircuitGraph *circuit = nullptr;
  size_t position = 0;
  unsigned temporary_index = 0;
};
