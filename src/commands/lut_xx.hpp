#pragma once
#include <alice/alice.hpp>
#include <stp/lutxx/decompose.hpp>
#include <utility>

namespace alice {
class lutxx_command : public command {
public:
  explicit lutxx_command(const environment::ptr &env)
      : command(env, "44-LUT / 66-LUT pair decomposition (disjoint first)") {
    add_option("-f,--function", truth_table, "hexadecimal truth table; use 0b prefix for binary", true);
    add_option("-x", lut_k, "target LUT size for pair decomposition (supported: 4 or 6)", true);
    add_option("--filename,-o", filename, "output BENCH filename", true);
    add_option("--output", output, "output name", true);
  }
protected:
  void execute() override {
    auto text = std::exchange(truth_table, {});
    const int k = std::exchange(lut_k, 6);
    const auto bench_filename = std::exchange(filename, "result.bench");
    const auto output_name = std::exchange(output, "po");
    try {
      if (!is_set("-x")) throw std::invalid_argument("provide -x 4 or -x 6");
      const auto bits = stp::lutxx::parse_table(text);
      auto result = stp::lutxx::decompose(bits, k);
      if (!result.found) { env->out() << "[e] no " << k << k << "-LUT decomposition found\n"; return; }
      env->out() << "[i] " << result.method << " (K=" << k << ")\n";
      if (!result.my.empty()) print("MY", result.my, result.my_vars);
      print("MX", result.mx, result.mx_vars);
      unsigned variable_count = 0;
      while ((size_t{1} << variable_count) < bits.size()) ++variable_count;
      stp::lutxx::write_bench(result, variable_count, bench_filename, output_name);
      env->out() << "[i] LUT result written to " << bench_filename << '\n';
    } catch (const std::exception &e) { env->out() << "[e] " << e.what() << '\n'; }
  }
private:
  void print(const std::string &name, const std::string &tt, const std::vector<int> &vars) {
    env->out() << "[i] " << name << " = " << stp::lutxx::to_hex(tt) << "  inputs (MSB->LSB):";
    for (int v : vars) env->out() << ' ' << (v == 0 ? "MY" : "x" + std::to_string(v));
    env->out() << '\n';
  }
  std::string truth_table;
  int lut_k = 6;
  std::string filename = "result.bench";
  std::string output = "po";
};
ALICE_ADD_COMMAND(lutxx, "Decomposition");
}


