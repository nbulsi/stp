#pragma once
#include <fstream>
#include <cctype>

namespace stp::lutxx {
inline int LUT_PAIR_FANIN_LIMIT = 6;
inline int mx_lut_fanin_limit() { return LUT_PAIR_FANIN_LIMIT - 1; }
}

#include "lut_pair_bidec.hpp"
#include "lut_pair_dsd.hpp"
#include <stdexcept>

namespace stp::lutxx {
struct Result {
  bool found = false;
  std::string method, my, mx;
  std::vector<int> my_vars, mx_vars; // MSB to LSB; 0 denotes MY.
};

inline std::string parse_table(std::string text) {
  std::string bits;
  if (text.rfind("0b", 0) == 0 || text.rfind("0B", 0) == 0) {
    bits = text.substr(2);
    if (bits.find_first_not_of("01") != std::string::npos)
      throw std::invalid_argument("binary truth table must contain only 0 and 1");
  } else {
    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) text.erase(0, 2);
    if (text.size() > (1u << 18)) throw std::invalid_argument("truth table exceeds 20 variables");
    for (char c : text) {
      auto value = std::string("0123456789abcdef").find(c);
      if (value == std::string::npos) value = std::string("0123456789ABCDEF").find(c);
      if (value == std::string::npos) throw std::invalid_argument("invalid hexadecimal truth table");
      for (int j = 3; j >= 0; --j) bits += ((value >> j) & 1) ? '1' : '0';
    }
  }
  if (bits.empty() || bits.size() > (1u << 20) || (bits.size() & (bits.size() - 1)))
    throw std::invalid_argument("truth table width must be a power of two, at most 2^20");
  return bits;
}

inline Result decompose(const std::string &bits, int k) {
  if (k != 4 && k != 6) throw std::invalid_argument("-x only supports 4 or 6");
  if (bits.empty() || bits.size() > (1u << 20) || (bits.size() & (bits.size()-1)) ||
      bits.find_first_not_of("01") != std::string::npos)
    throw std::invalid_argument("invalid binary truth table");
  struct Restore { int old; ~Restore() { LUT_PAIR_FANIN_LIMIT = old; } } restore{LUT_PAIR_FANIN_LIMIT};
  LUT_PAIR_FANIN_LIMIT = k;
  unsigned n = 0;
  while ((size_t{1} << n) < bits.size()) ++n;
  std::vector<int> order;
  // Remove variables whose paired cofactors are identical, retaining MSB order.
  for (int v = static_cast<int>(n); v > 0; --v) {
    bool active = false;
    for (size_t i = 0; i < bits.size(); ++i)
      if (bits[i] != bits[i ^ (size_t{1} << (v-1))]) { active = true; break; }
    if (active) order.push_back(v);
  }
  std::string table(size_t{1} << order.size(), '0');
  for (size_t i = 0; i < table.size(); ++i) {
    size_t old = 0;
    for (size_t j = 0; j < order.size(); ++j)
      old |= ((i >> (order.size()-1-j)) & 1) << (order[j]-1);
    table[i] = bits[old];
  }
  if (order.size() <= static_cast<size_t>(k)) return {true, "single LUT", {}, table, {}, order};
  if (order.size() > static_cast<size_t>(2*k-1)) return {};
  auto dsd = run_66lut_dsd_by_mx_subset(table, order);
  if (dsd.found) {
    auto mx_order = dsd.mx_vars_msb2lsb;
    mx_order.insert(mx_order.begin(), 0);
    return {true, "disjoint decomposition", dsd.My, dsd.Mx, dsd.my_vars_msb2lsb, mx_order};
  }
  for (auto [x,y,z] : enumerate_xyz(static_cast<int>(order.size()))) {
    for (const auto &[a,b,c] : enumerate_variable_partitions(order,x,y,z)) {
      std::string mx, symbolic, my;
      if (!solve_MX_MY_from_cofactors(table,order,a,b,c,mx,symbolic,my)) continue;
      auto my_order = a;
      my_order.insert(my_order.end(),b.begin(),b.end());
      std::vector<int> mx_order{0};
      mx_order.insert(mx_order.end(),b.begin(),b.end());
      mx_order.insert(mx_order.end(),c.begin(),c.end());
      // Use merged MX constraints; the legacy symbolic string can retain
      // x in positions constrained by later blocks.
      return {true, "bi-decomposition", my, mx, my_order, mx_order};
    }
  }
  return {};
}
}

namespace stp::lutxx {
inline std::string to_hex(const std::string& bits) {
  if (bits.empty() || bits.find_first_not_of("01") != std::string::npos)
    throw std::invalid_argument("BENCH LUT must contain binary values");
  std::string padded((4 - bits.size() % 4) % 4, '0');
  padded += bits;
  std::string hex = "0x";
  for (size_t i = 0; i < padded.size(); i += 4) {
    unsigned value = 0;
    for (size_t j = 0; j < 4; ++j) value = value * 2 + (padded[i+j] - '0');
    hex += "0123456789abcdef"[value];
  }
  return hex;
}

inline void write_bench(const Result& result, unsigned variable_count,
                        const std::string& filename,
                        const std::string& output_name = "po") {
  if (!result.found) throw std::invalid_argument("no decomposition to export");
  if (variable_count > 20) throw std::invalid_argument("too many BENCH inputs");
  if (output_name.empty() ||
      !(std::isalpha(static_cast<unsigned char>(output_name[0])) || output_name[0] == '_'))
    throw std::invalid_argument("invalid output name");
  for (unsigned char c : output_name)
    if (!(std::isalnum(c) || c == '_')) throw std::invalid_argument("invalid output name");
  // Constant LUTs use one ignored input for compatibility with the LUT reader.
  const unsigned input_count = std::max(1u, variable_count);
  for (unsigned i = 1; i <= input_count; ++i)
    if (output_name == "x" + std::to_string(i))
      throw std::invalid_argument("output name conflicts with an input");
  auto validate = [&](const std::string& bits, const std::vector<int>& vars, bool outer) {
    if (vars.size() > 20 || bits.size() != (size_t{1} << vars.size()) ||
        bits.find_first_not_of("01") != std::string::npos)
      throw std::invalid_argument("invalid BENCH LUT table");
    for (size_t i = 0; i < vars.size(); ++i) {
      int v = vars[i];
      if (v < 0 || static_cast<unsigned>(v) > variable_count ||
          (v == 0 && (!outer || result.my.empty())) ||
          std::find(vars.begin(), vars.begin()+i, v) != vars.begin()+i)
        throw std::invalid_argument("invalid BENCH LUT input");
    }
  };
  if (!result.my.empty()) validate(result.my, result.my_vars, false);
  validate(result.mx, result.mx_vars, true);
  std::string inner_name = "lutxx_n0";
  if (inner_name == output_name) inner_name += "_";
  std::ofstream stream(filename);
  if (!stream) throw std::runtime_error("cannot write bench file " + filename);
  for (unsigned i = 1; i <= input_count; ++i)
    stream << "INPUT(x" << i << ")\n";
  stream << "OUTPUT(" << output_name << ")\n\n";
  auto emit = [&](const std::string& name, const std::string& bits,
                  const std::vector<int>& vars) {
    stream << name << " = LUT " << to_hex(vars.empty() ? bits + bits : bits) << " (";
    // The project LUT reader reverses BENCH inputs; emit LSB-to-MSB order.
    if (vars.empty()) stream << "x1";
    for (size_t i = vars.size(); i-- > 0;) {
      if (i + 1 != vars.size()) stream << ", ";
      stream << (vars[i] == 0 ? inner_name : "x" + std::to_string(vars[i]));
    }
    stream << ")\n";
  };
  if (!result.my.empty()) emit(inner_name, result.my, result.my_vars);
  emit(output_name, result.mx, result.mx_vars);
  stream.close();
  if (!stream) throw std::runtime_error("cannot write bench file " + filename);
}
} // namespace stp::lutxx
