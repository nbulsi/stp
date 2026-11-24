// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT
#pragma once

#include <alice/alice.hpp>
#include <stp/io/expr_parser.hpp>  // expr_node and expr_chain_parser

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Self-contained parser and CEC adapter.  No CircuitGraph or simulator.
namespace stp {
namespace sopcec_detail {

struct Node {
  enum class Kind { Detector, Constant, And, Or };
  Kind kind;
  std::string variable;
  int value = 0; // detector value (0,1,2) or Boolean constant (0,1)
  std::unique_ptr<Node> left;
  std::unique_ptr<Node> right;

  explicit Node(Kind k) : kind(k) {}
};

using NodePtr = std::unique_ptr<Node>;

inline NodePtr make_binary(Node::Kind kind, NodePtr lhs, NodePtr rhs) {
  NodePtr n(new Node(kind));
  n->left = std::move(lhs);
  n->right = std::move(rhs);
  return n;
}

class SopParser {
public:
  explicit SopParser(const std::string &text) : text_(text) {}

  NodePtr parse() {
    NodePtr root = parse_or();
    skip_ws();
    if (pos_ != text_.size())
      fail("unexpected character");
    return root;
  }

private:
  void skip_ws() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
  }
  char peek() {
    skip_ws();
    return pos_ < text_.size() ? text_[pos_] : '\0';
  }
  bool eat(char c) {
    if (peek() != c) return false;
    ++pos_;
    return true;
  }
  [[noreturn]] void fail(const std::string &why) const {
    throw std::invalid_argument("SOP parse error at offset " +
                                std::to_string(pos_) + ": " + why);
  }
  static bool atom_start(char c) {
    return c == '(' || c == '0' || c == '1' ||
           std::isalpha(static_cast<unsigned char>(c));
  }
  NodePtr parse_or() {
    NodePtr lhs = parse_and();
    while (peek() == '+' || peek() == '|') {
      ++pos_;
      lhs = make_binary(Node::Kind::Or, std::move(lhs), parse_and());
    }
    return lhs;
  }
  NodePtr parse_and() {
    NodePtr lhs = parse_atom();
    while (true) {
      const char c = peek();
      if (c == '*' || c == '&') {
        ++pos_;
        lhs = make_binary(Node::Kind::And, std::move(lhs), parse_atom());
      } else if (atom_start(c)) { // implicit AND: A2B1, A2(B2+B1)
        lhs = make_binary(Node::Kind::And, std::move(lhs), parse_atom());
      } else {
        break;
      }
    }
    return lhs;
  }
  NodePtr parse_atom() {
    if (eat('(')) {
      NodePtr n = parse_or();
      if (!eat(')')) fail("expected ')'");
      return n;
    }

    const char first = peek();
    if (first == '0' || first == '1') {
      ++pos_;
      NodePtr n(new Node(Node::Kind::Constant));
      n->value = first - '0';
      return n;
    }
    if (!std::isalpha(static_cast<unsigned char>(first)))
      fail("expected detector, 0, 1, or '('");

    std::string name;
    if (std::isupper(static_cast<unsigned char>(first))) {
      // Compact notation A0B1 (one-letter uppercase variable names).
      name.push_back(first);
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '_' || text_[pos_] == '='))
        ++pos_;
      // The next character must be 0,1,2.
    } else {
      // Longer variable names: x12_2 or x12=2 (separate factors with *).
      const size_t begin = pos_;
      while (pos_ < text_.size() &&
             (std::isalnum(static_cast<unsigned char>(text_[pos_])) ||
              text_[pos_] == '_'))
        ++pos_;
      std::string token = text_.substr(begin, pos_ - begin);
      if (token.size() >= 3 && token[token.size()-2] == '_' &&
          token.back() >= '0' && token.back() <= '2') {
        name = token.substr(0, token.size()-2);
        NodePtr n(new Node(Node::Kind::Detector));
        n->variable = std::move(name);
        n->value = token.back() - '0';
        return n;
      }
      if (!eat('=')) fail("multi-character variables need '=0/1/2' or '_0/_1/_2'");
      name = token;
    }

    if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '2')
      fail("detector needs a ternary value 0, 1 or 2");
    const int value = text_[pos_++] - '0';
    NodePtr n(new Node(Node::Kind::Detector));
    n->variable = std::move(name);
    n->value = value;
    return n;
  }

  const std::string &text_;
  size_t pos_ = 0;
};

inline void collect_variables(const Node &node, std::set<std::string> &names) {
  if (node.kind == Node::Kind::Detector) names.insert(node.variable);
  if (node.left) collect_variables(*node.left, names);
  if (node.right) collect_variables(*node.right, names);
}

// STP vectors use: [output radix, zero-based output row of each column].
// A 3-valued basis is ordered (2,1,0); Boolean basis is ordered (1,0).
// A Boolean true is encoded as row 0, false as row 1.
inline void emit_chain(const Node &node, const std::map<std::string, int> &id,
                       std::vector<expr_node> &chain) {
  switch (node.kind) {
    case Node::Kind::Detector: {
      std::vector<stp_data> mat = {2, 1, 1, 1};
      mat[static_cast<size_t>(3 - node.value)] = 0;
      chain.emplace_back(NodeType_Gate, GateType_Lut, 0, 0, mat);
      chain.emplace_back(NodeType_Variable, id.at(node.variable), static_cast<stp_data>(3));
      break;
    }
    case Node::Kind::Constant: {
      const std::vector<stp_data> mat = {2, static_cast<stp_data>(1 - node.value)};
      chain.emplace_back(NodeType_Gate, GateType_Lut, 0, 0, mat);
      break;
    }
    case Node::Kind::And:
    case Node::Kind::Or: {
      // Binary input column order: (1,1), (1,0), (0,1), (0,0).
      const std::vector<stp_data> mat = node.kind == Node::Kind::And
          ? std::vector<stp_data>{2, 0, 1, 1, 1}
          : std::vector<stp_data>{2, 0, 0, 0, 1};
      chain.emplace_back(NodeType_Gate, GateType_Lut, 0, 0, mat);
      emit_chain(*node.left, id, chain);
      emit_chain(*node.right, id, chain);
      break;
    }
  }
}

inline size_t pow3(size_t count) {
  size_t result = 1;
  for (size_t i = 0; i < count; ++i) {
    if (result > std::numeric_limits<size_t>::max() / 3)
      throw std::overflow_error("3^N overflows size_t");
    result *= 3;
  }
  return result;
}

struct Result {
  std::vector<std::string> variables;  // lexicographically sorted local variables
  std::vector<stp_data> truth;         // encoded by expr_chain_parser::out_vec
  size_t combinations = 0;
  size_t chain_nodes = 0;
  double compute_ms = 0;
};

inline Result evaluate(const Node &root) {
  std::set<std::string> names;
  collect_variables(root, names);
  Result r;
  r.variables.assign(names.begin(), names.end());
  r.combinations = pow3(r.variables.size());

  std::map<std::string, int> ids;
  for (size_t i = 0; i < r.variables.size(); ++i)
    ids.emplace(r.variables[i], static_cast<int>(i));

  std::vector<expr_node> chain;
  emit_chain(root, ids, chain);
  r.chain_nodes = chain.size();
  std::vector<int64_t> old_pi_index(r.variables.size());
  for (size_t i = 0; i < r.variables.size(); ++i)
    old_pi_index[i] = static_cast<int64_t>(i);

  const auto start = std::chrono::steady_clock::now();
  expr_chain_parser expression(chain, old_pi_index, false);  // CPU, full STP
  const auto stop = std::chrono::steady_clock::now();
  r.compute_ms = std::chrono::duration<double, std::milli>(stop - start).count();

  r.truth = std::move(expression.out_vec);
  if (r.truth.size() != r.combinations + 1 || r.truth[0] != 2)
    throw std::runtime_error("STP result size or output radix mismatch");
  return r;
}

inline int output_value(const Result &result,
                        const std::map<std::string, size_t> &global_positions,
                        const std::vector<int> &global_assignment) {
  size_t local_index = 0;
  for (const std::string &name : result.variables)
    local_index = local_index * 3 +
                  static_cast<size_t>(global_assignment[global_positions.at(name)]);
  // out_vec stores columns in descending basis order, not numeric 0..3^N-1.
  return 1 - static_cast<int>(result.truth[result.combinations - local_index]);
}

inline std::vector<int> decode_assignment(size_t index, size_t n) {
  std::vector<int> values(n);
  for (size_t i = n; i > 0; --i) {
    values[i - 1] = static_cast<int>(index % 3);
    index /= 3;
  }
  return values;
}

} // namespace sopcec_detail
} // namespace stp

namespace alice {

class sopcec_command : public command {
public:
  explicit sopcec_command(const environment::ptr &env)
      : command(env, "Compare two ternary-input Boolean SOP expressions using full CPU STP") {
    add_option("--lhs", lhs_, "left SOP expression", true);
    add_option("--rhs", rhs_, "right SOP expression", true);
    add_flag("--verbose", "show the complete ternary truth table (up to 8 variables)");
    add_flag("--stats", "show STP matrix-chain statistics");
  }

protected:
  void execute() override {
    using namespace stp::sopcec_detail;
    try {
      SopParser lhs_parser(lhs_);
      SopParser rhs_parser(rhs_);
      const NodePtr lhs_tree = lhs_parser.parse();
      const NodePtr rhs_tree = rhs_parser.parse();

      // Each side only passes its own contiguous 0..N-1 IDs to expr_chain_parser.
      // The comparison lifts local truth tables to the union of input variables.
      std::set<std::string> variable_set;
      collect_variables(*lhs_tree, variable_set);
      collect_variables(*rhs_tree, variable_set);
      std::vector<std::string> variables(variable_set.begin(), variable_set.end());
      std::map<std::string, size_t> positions;
      for (size_t i = 0; i < variables.size(); ++i)
        positions.emplace(variables[i], i);
      const size_t patterns = pow3(variables.size());

      const auto begin = std::chrono::steady_clock::now();
      Result lhs_result = evaluate(*lhs_tree);
      Result rhs_result = evaluate(*rhs_tree);

      bool equivalent = true;
      size_t fail_index = 0;
      int fail_lhs = 0, fail_rhs = 0;
      for (size_t index = 0; index < patterns; ++index) {
        const auto values = decode_assignment(index, variables.size());
        const int a = output_value(lhs_result, positions, values);
        const int b = output_value(rhs_result, positions, values);
        if (a != b) {
          equivalent = false;
          fail_index = index;
          fail_lhs = a;
          fail_rhs = b;
          break;
        }
      }
      const auto end = std::chrono::steady_clock::now();
      const double total_ms = std::chrono::duration<double, std::milli>(end-begin).count();

      std::cout << "\n****************************************\n"
                << "Report : Ternary SOP CEC\n"
                << "LHS    : " << lhs_ << "\n"
                << "RHS    : " << rhs_ << "\n"
                << "Method : Full STP (CPU, no circuit partition)\n"
                << "Inputs : " << variables.size() << "\n"
                << "Patterns: " << patterns << "\n";
      if (is_set("stats")) {
        std::cout << "LHS chain nodes : " << lhs_result.chain_nodes << "\n"
                  << "RHS chain nodes : " << rhs_result.chain_nodes << "\n";
      }
      std::cout << std::fixed << std::setprecision(3)
                << "LHS STP runtime : " << lhs_result.compute_ms << " ms\n"
                << "RHS STP runtime : " << rhs_result.compute_ms << " ms\n"
                << "Total runtime   : " << total_ms << " ms\n"
                << "Equivalent      : " << (equivalent ? "YES" : "NO") << "\n"
                << "CEC result      : " << (equivalent ? "PASS" : "FAIL") << "\n";
      if (!equivalent) {
        const auto values = decode_assignment(fail_index, variables.size());
        std::cout << "Counterexample  : ";
        for (size_t i = 0; i < variables.size(); ++i)
          std::cout << (i ? ", " : "") << variables[i] << "=" << values[i];
        std::cout << "\nLHS output      : " << fail_lhs
                  << "\nRHS output      : " << fail_rhs << "\n";
      }
      if (is_set("verbose")) {
        if (variables.size() > 8) {
          std::cout << "Truth table omitted: more than 8 ternary inputs\n";
        } else {
          std::cout << "Truth table:\n";
          for (const auto &name : variables) std::cout << name << ' ';
          std::cout << "| LHS RHS\n";
          for (size_t index = 0; index < patterns; ++index) {
            const auto values = decode_assignment(index, variables.size());
            for (int v : values) std::cout << v << ' ';
            std::cout << "| " << output_value(lhs_result, positions, values)
                      << "   " << output_value(rhs_result, positions, values) << "\n";
          }
        }
      }
      std::cout << "****************************************\n";
    } catch (const std::exception &e) {
      std::cerr << "sopcec failed: " << e.what() << '\n';
    }
  }

private:
  std::string lhs_;
  std::string rhs_;
};

ALICE_ADD_COMMAND(sopcec, "Verification");

} // namespace alice
