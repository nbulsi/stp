// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT
#pragma once

#include <ostream>
#include <stp/core/circuit_graph.hpp>

namespace stp
{
inline std::string dot_quote(const std::string &text)
{
  std::string result = "\"";
  for (char c : text)
  {
    if (c == '\\' || c == '"')
      result += '\\';
    if (c == '\n')
      result += "\\n";
    else if (c == '\r')
      result += "\\r";
    else
      result += c;
  }
  return result + '"';
}

inline std::string lut_hex(const Gate &gate)
{
  const auto type = gate.get_type();
  const size_t count = type.cols() - 1;
  std::string hex;
  unsigned value = 0;
  const size_t padding = (4 - count % 4) % 4;
  for (size_t i = 0; i < padding + count; ++i)
  {
    value = value * 2 + (i < padding ? 0 : 1 - type(i - padding + 1));
    if (i % 4 == 3)
    {
      hex += "0123456789abcdef"[value];
      value = 0;
    }
  }
  return "0x" + hex;
}

struct LutAppearance
{
  std::string name;
  std::string color = "#dcfce7";
  int select = -1;
  int data0 = -1;
  unsigned inverted_inputs = 0;
  bool inverted_output = false;
};

inline LutAppearance lut_appearance(size_t inputs, const std::string &hex)
{
  if (inputs == 1 && hex == "0x1")
    return {"INV", "#fecaca"};
  if (inputs == 2)
  {
    if (hex == "0x2" || hex == "0x4")
      return {"AND", "#86efac", -1, -1, hex == "0x2" ? 2u : 1u};
    if (hex == "0xb" || hex == "0xd")
      return {"IMP", "#67e8f9"};
    if (hex == "0x8")
      return {"AND", "#86efac"};
    if (hex == "0x7")
      return {"AND", "#86efac", -1, -1, 0, true};
    if (hex == "0xe")
      return {"OR", "#fde047"};
    if (hex == "0x1")
      return {"OR", "#fde047", -1, -1, 0, true};
    if (hex == "0x6")
      return {"XOR", "#c4b5fd"};
    if (hex == "0x9")
      return {"XOR", "#c4b5fd", -1, -1, 0, true};
  }
  if (inputs == 3)
  {
    if (hex == "0xe8")
      return {"MAJ", "#f9a8d4"};
    if (hex == "0x17")
      return {"MAJ", "#f9a8d4", -1, -1, 0, true};
    if (hex == "0x96")
      return {"XOR", "#a5b4fc"};
    if (hex == "0x69")
      return {"XOR", "#a5b4fc", -1, -1, 0, true};
    // Recognize a non-inverting MUX for every selector/data input permutation.
    const auto table = std::stoul(hex, nullptr, 16);
    for (unsigned select = 0; select < 3; ++select)
      for (unsigned swap = 0; swap < 2; ++swap)
      {
        const unsigned d0 = (select + 1 + swap) % 3;
        const unsigned d1 = (select + 2 - swap) % 3;
        unsigned mux = 0;
        for (unsigned row = 0; row < 8; ++row)
        {
          const unsigned data = (row >> select) & 1 ? d1 : d0;
          mux |= ((row >> data) & 1) << row;
        }
        if (table == mux)
          return {"MUX", "#fdba74", static_cast<int>(select), static_cast<int>(d0)};
      }
  }
  return {};
}

inline void write_dot(const CircuitGraph &graph, std::ostream &out)
{
  out << "digraph STP {\n  rankdir=BT;\n"
         "  graph [bgcolor=white, nodesep=0.22, ranksep=0.35];\n"
         "  node [fontname=Helvetica, style=filled, fontsize=11, margin=\"0.02,0.01\", "
         "width=0, height=0];\n"
         "  edge [fontname=Helvetica, fontsize=10];\n";
  struct Signal
  {
    line_idx line = NULL_INDEX;
    bool inverted = false;
  };
  std::vector<Signal> aliases(graph.get_gates().size());
  std::vector<bool> inverted_outputs(graph.get_gates().size(), false);
  for (const auto &gate : graph.get_gates())
  {
    const auto &inputs = gate.get_inputs();
    const auto hex = lut_hex(gate);
    inverted_outputs[gate.get_id()] = lut_appearance(inputs.size(), hex).inverted_output;
    if (inputs.size() == 1 && (hex == "0x1" || hex == "0x2"))
      aliases[gate.get_id()] = {inputs[0], hex == "0x1"};
    // Two-input LUTs that only depend on one input are wires/inverted wires too.
    if (inputs.size() == 2)
    {
      if (hex == "0xa" || hex == "0x5")
        aliases[gate.get_id()] = {inputs[1], hex == "0x5"};
      if (hex == "0xc" || hex == "0x3")
        aliases[gate.get_id()] = {inputs[0], hex == "0x3"};
    }
  }
  auto source = [&](line_idx id, bool inverted = false) {
    for (size_t steps = 0; steps <= graph.get_gates().size(); ++steps)
    {
      const auto &line = graph.get_line(id);
      if (line.is_input)
        return std::make_pair(std::string("pi") + std::to_string(id), inverted);
      if (line.source == NULL_INDEX)
        throw std::runtime_error("undriven signal: " + line.name);
      const auto alias = aliases.at(line.source);
      if (alias.line == NULL_INDEX)
        return std::make_pair(std::string("g") + std::to_string(line.source),
                              inverted != inverted_outputs[line.source]);
      inverted ^= alias.inverted;
      id = alias.line;
    }
    throw std::runtime_error("cycle in buffer/inverter chain");
  };
  for (auto id : graph.get_inputs())
    out << "  pi" << id
        << " [shape=circle, fillcolor=\"#dbeafe\", label=" << dot_quote(graph.get_line(id).name)
        << "];\n";
  out << "  { rank=source;";
  for (auto id : graph.get_inputs())
    out << " pi" << id << ';';
  out << " }\n";
  for (const auto &gate : graph.get_gates())
  {
    if (aliases[gate.get_id()].line != NULL_INDEX)
      continue;
    const auto hex = lut_hex(gate);
    const auto short_hex = hex.size() > 20 ? hex.substr(0, 18) + "..." : hex;
    const auto appearance = lut_appearance(gate.get_inputs().size(), hex);
    const auto label = appearance.name.empty()
                           ? "LUT" + std::to_string(gate.get_inputs().size()) + " " + short_hex
                           : appearance.name;
    out << "  g" << gate.get_id() << " [shape="
        << (appearance.name.empty()    ? "box"
            : appearance.name == "MUX" ? "trapezium"
            : appearance.name == "INV" ? "triangle"
                                       : "circle")
        << ", fillcolor=" << dot_quote(appearance.color) << ", label="
        << dot_quote(appearance.name.empty() ? graph.get_line(gate.get_output()).name + "\n" + label
                                             : label)
        << ", tooltip=" << dot_quote(hex) << "];\n";
    const auto &inputs = gate.get_inputs();
    // CircuitGraph stores BENCH inputs in reverse order.
    for (size_t port = 0; port < inputs.size(); ++port)
    {
      const auto signal =
          source(inputs[inputs.size() - 1 - port], (appearance.inverted_inputs >> port) & 1u);
      out << "  " << signal.first << " -> g" << gate.get_id();
      std::string port_label = "in" + std::to_string(port);
      if (appearance.name == "MUX")
      {
        const bool select = static_cast<int>(port) == appearance.select;
        out << (select ? ":e" : ":s");
        port_label = select ? "S" : static_cast<int>(port) == appearance.data0 ? "D0" : "D1";
      }
      if (appearance.name == "IMP")
      {
        const size_t antecedent = hex == "0xd" ? 0 : 1;
        port_label = port == antecedent ? "A" : "B";
      }
      out << " [label=" << dot_quote(port_label);
      if (signal.second)
        out << ", style=dashed";
      out << "];\n";
    }
  }
  for (auto id : graph.get_outputs())
  {
    const auto signal = source(id);
    out << "  po" << id
        << " [shape=ellipse, fillcolor=\"#ffedd5\", label=" << dot_quote(graph.get_line(id).name)
        << "];\n  " << signal.first << " -> po" << id;
    if (signal.second)
      out << " [style=dashed]";
    out << ";\n";
  }
  out << "  { rank=sink;";
  for (auto id : graph.get_outputs())
    out << " po" << id << ';';
  out << " }\n";
  out << "}\n";
  if (!out)
    throw std::runtime_error("failed to write DOT output");
}
} // namespace stp
