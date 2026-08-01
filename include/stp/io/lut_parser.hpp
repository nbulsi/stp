// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#include <cctype>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <stp/core/circuit_graph.hpp>
#include <stp/utils/stp_utils.hpp>

#ifndef LUT_PARSER_H
#define LUT_PARSER_H

class LutParser
{
public:
  bool parse(std::istream &is, CircuitGraph &graph)
  {
    last_error_.clear();
    std::string line;
    size_t line_number = 0;

    while (std::getline(is, line))
    {
      ++line_number;
      if (!line.empty() && line.back() == '\r')
        line.pop_back();

      const size_t first = line.find_first_not_of(" \t\f\v");
      if (first == std::string::npos || line[first] == '#' ||
          (line[first] == '/' && first + 1 < line.size() && line[first + 1] == '/'))
        continue;

      try
      {
        const std::vector<std::string> tokens = stp::split(line, ",=() \t\f\v");
        if (tokens.empty())
          continue;
        if (tokens[0] == "INPUT")
          match_input(graph, line);
        else if (tokens[0] == "OUTPUT")
          match_output(graph, line);
        else if (tokens.size() >= 2 && tokens[1] == "LUT")
          match_gate(graph, tokens);
        else
          throw std::invalid_argument("unsupported statement");
      }
      catch (const std::exception &error)
      {
        last_error_ = "line " + std::to_string(line_number) + ": " + error.what();
        return false;
      }
    }
    if (is.bad())
    {
      last_error_ = "I/O error while reading input";
      return false;
    }
    return last_error_.empty();
  }

  const std::string &error() const noexcept
  {
    return last_error_;
  }

private:
  void match_input(CircuitGraph &graph, const std::string &line)
  {
    std::string input_name = get_io_name(line);
    if (input_name.empty())
      throw std::invalid_argument("INPUT name is empty");
    graph.add_input(input_name);
  }

  void match_output(CircuitGraph &graph, const std::string &line)
  {
    std::string output_name = get_io_name(line);
    if (output_name.empty())
      throw std::invalid_argument("OUTPUT name is empty");
    graph.add_output(output_name);
  }

  void match_gate(CircuitGraph &graph, const std::vector<std::string> &gate)
  {
    if (gate.size() < 3 || gate[1] != "LUT")
      throw std::invalid_argument("malformed LUT statement");

    const std::string &output = gate[0];
    if (output.empty())
      throw std::invalid_argument("LUT output name is empty");

    std::string tt = gate[2];
    if (tt.size() < 3 || (tt[0] != '0') || (tt[1] != 'x' && tt[1] != 'X'))
      throw std::invalid_argument("truth table must use a 0x hexadecimal prefix");
    tt.erase(0, 2);

    const std::vector<std::string> inputs(gate.begin() + 3, gate.end());
    if (inputs.size() >= 31)
      throw std::invalid_argument("LUT has too many inputs");
    const size_t expected_digits = ((size_t{1} << inputs.size()) + 3) / 4;
    if (tt.size() != expected_digits)
      throw std::invalid_argument("truth table width does not match LUT input count");
    for (const unsigned char digit : tt)
      if (!std::isxdigit(digit))
        throw std::invalid_argument("truth table contains a non-hexadecimal digit");

    Type type = get_stp_vec(tt, inputs.size());
    graph.add_gate(type, inputs, output);
  }
  std::string get_io_name(const std::string &str)
  {
    size_t start = str.find('(');
    size_t end = str.rfind(')'); // from right to left
    // need to delete space
    if (start != std::string::npos && end != std::string::npos && start < end)
    {
      return str.substr(start + 1, end - start - 1);
    }
    throw std::invalid_argument("I/O declaration must use parentheses");
  }

  stp_vec get_stp_vec(std::string tt, const size_t inputs_num)
  {
    for (char &digit : tt)
      digit = static_cast<char>(std::tolower(static_cast<unsigned char>(digit)));

    // buff or not
    if (inputs_num == 1 && tt.size() == 1)
    {
      stp_vec type(3);
      type(0) = 2;
      switch (tt[0])
      {
        case '0':
          type(1) = 1;
          type(2) = 1;
          break;
        case '1':
          type(1) = 1;
          type(2) = 0;
          break;
        case '2':
          type(1) = 0;
          type(2) = 1;
          break;
        case '3':
          type(1) = 0;
          type(2) = 0;
          break;
        default:
          break;
      }
      return type;
    }
    stp_vec type((1U << inputs_num) + 1);
    type(0) = 2;
    int type_idx;
    for (int i = 0, len = tt.size(); i < len; i++)
    {
      type_idx = 4 * i + 1;
      switch (tt[i])
      {
        case '0': // 0000 - > 1111
          type(type_idx) = 1;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 1;
          break;
        case '1': // 0001 - > 1110
          type(type_idx) = 1;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 0;
          break;
        case '2': // 0010 - > 1101
          type(type_idx) = 1;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 1;
          break;
        case '3': // 0011 - > 1100
          type(type_idx) = 1;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 0;
          break;
        case '4': // 0100 - > 1011
          type(type_idx) = 1;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 1;
          break;
        case '5': // 0101 - > 1010
          type(type_idx) = 1;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 0;
          break;
        case '6': // 0110 - > 1001
          type(type_idx) = 1;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 1;
          break;
        case '7': // 0111 - > 1000
          type(type_idx) = 1;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 0;
          break;
        case '8': // 1000 - > 0111
          type(type_idx) = 0;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 1;
          break;
        case '9': // 1001 - > 0110
          type(type_idx) = 0;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 0;
          break;
        case 'a': // 1010 - > 0101
          type(type_idx) = 0;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 1;
          break;
        case 'b': // 1011 - > 0100
          type(type_idx) = 0;
          type(type_idx + 1) = 1;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 0;
          break;
        case 'c': // 1100 - > 0011
          type(type_idx) = 0;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 1;
          break;
        case 'd': // 1101 - > 0010
          type(type_idx) = 0;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 1;
          type(type_idx + 3) = 0;
          break;
        case 'e': // 1110 - > 0001
          type(type_idx) = 0;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 1;
          break;
        case 'f': // 1111 - > 0000
          type(type_idx) = 0;
          type(type_idx + 1) = 0;
          type(type_idx + 2) = 0;
          type(type_idx + 3) = 0;
          break;
        default:
          break;
      }
    }
    return type;
  }

  std::string last_error_;
};

#endif
