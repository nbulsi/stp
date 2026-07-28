#include <catch2/catch.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <stp/io/lut_parser.hpp>
#include <stp/sim/simulator.hpp>

namespace
{
std::string simulate_lut_bench(std::istream &input)
{
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));

  _using_CUDA = false;
  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report);
  return report.str();
}
} // namespace

TEST_CASE("lutsim accepts LF and CRLF BENCH files", "[lutsim]")
{
  constexpr const char *lf_bench = "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x8 (a, b)\n";
  constexpr const char *crlf_bench = "INPUT(a)\r\nINPUT(b)\r\nOUTPUT(y)\r\ny = LUT 0x8 (a, b)\r\n";

  std::istringstream lf_input(lf_bench);
  std::istringstream crlf_input(crlf_bench);

  CHECK(simulate_lut_bench(lf_input).find("0x8") != std::string::npos);
  CHECK(simulate_lut_bench(crlf_input).find("0x8") != std::string::npos);
}

TEST_CASE("lutsim simulates the CRLF cma152a benchmark", "[lutsim][regression]")
{
  std::ifstream input(std::string(STP_SOURCE_DIR) + "/test/benchmarks/mcnc/cma152a.bench");
  REQUIRE(input.good());

  const std::string report = simulate_lut_bench(input);
  CHECK(report.find("Input order (LSB -> MSB) : n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11") !=
        std::string::npos);
  CHECK(report.find("0x") != std::string::npos);
}

TEST_CASE("lutsim simulates bundled LF benchmarks", "[lutsim][regression]")
{
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"c17_lut.bench", "0xF313F333"},
      {"t1.bench", "0x0"},
      {"t2.bench", "0xF8"},
  };

  for (const auto &[filename, truth_table] : cases)
  {
    std::ifstream input(std::string(STP_SOURCE_DIR) + "/test/benchmarks/mcnc/" + filename);
    INFO(filename);
    REQUIRE(input.good());
    CHECK(simulate_lut_bench(input).find(truth_table) != std::string::npos);
  }
}
