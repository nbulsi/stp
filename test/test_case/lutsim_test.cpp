#include <catch2/catch.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <stp/dsd/approximate_decomposer.hpp>
#include <stp/dsd/decomposer.hpp>
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

TEST_CASE("lutsim does not expand primary inputs outside the output cone", "[lutsim][scaling]")
{
  std::istringstream input("INPUT(unused0)\nINPUT(a)\nINPUT(unused1)\nINPUT(b)\nOUTPUT(y)\n"
                           "y = LUT 0x8 (a, b)\n");

  const std::string report = simulate_lut_bench(input);
  CHECK(report.find("Inputs  : 4") != std::string::npos);
  CHECK(report.find("Cone support : 2 (unused inputs are not expanded)") != std::string::npos);
  CHECK(report.find("Input order (LSB -> MSB) : a, b") != std::string::npos);
  CHECK(report.find("0x8") != std::string::npos);
}

TEST_CASE("lutsim simulates bundled LF benchmarks", "[lutsim][regression]")
{
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"c17_lut.bench", "0xF313F333"},
      {"t1.bench", "CONST ZERO"},
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

TEST_CASE("lutsim can suppress truth-table output", "[lutsim][report]")
{
  std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x8 (a, b)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));

  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report, false);
  CHECK(report.str().find("Truth tables : not printed") != std::string::npos);
  CHECK(report.str().find("0x8") == std::string::npos);
}

TEST_CASE("lutsim abbreviates an all-zero output", "[lutsim][report]")
{
  std::istringstream input("INPUT(a)\nOUTPUT(y)\ny = LUT 0x0 (a)\n");
  const std::string report = simulate_lut_bench(input);
  CHECK(report.find("y                CONST ZERO") != std::string::npos);
}

TEST_CASE("dsd decomposes and writes a functionally equivalent BENCH", "[dsd]")
{
  const std::string filename = "/tmp/stp_dsd_regression.bench";
  stp::DsdDecomposer decomposer;
  const auto result = decomposer.run("0xA0", {"c", "b", "a"}, "po", filename);
  REQUIRE(result.valid);
  CHECK(result.expression == "0x8(c, 0xa(a, b))");

  std::ifstream input(filename);
  REQUIRE(input.good());
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));

  _using_CUDA = false;
  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report);
  CHECK(report.str().find("0xA0") != std::string::npos);
  std::remove(filename.c_str());
}

TEST_CASE("dsd enumeration preserves variables in recursive expressions", "[dsd]")
{
  const std::string filename = "/tmp/stp_dsd_enumeration_regression.bench";
  stp::DsdDecomposer decomposer;
  const auto result = decomposer.run("0x8000", {"d", "c", "b", "a"}, "po", filename, 1, true);
  REQUIRE(result.valid);
  REQUIRE(!result.solutions.empty());
  CHECK(result.solutions[0].find("po = 0x8(0x8(a, b), 0x8(c, d))") != std::string::npos);

  std::ifstream input(filename);
  REQUIRE(input.good());
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));

  _using_CUDA = false;
  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report);
  CHECK(report.str().find("0x8000") != std::string::npos);
  std::remove(filename.c_str());
}

TEST_CASE("idsd completes don't-care entries while preserving specified values", "[idsd]")
{
  const std::string filename = "/tmp/stp_idsd_regression.bench";
  stp::DsdDecomposer decomposer;
  const auto result =
      decomposer.run_incomplete("101-0011", {"c", "b", "a"}, "po", filename, 1, true);
  REQUIRE(result.valid);
  REQUIRE(!result.solutions.empty());

  std::ifstream input(filename);
  REQUIRE(input.good());
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));

  _using_CUDA = false;
  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report);
  // The selected completion is 10110011; the fourth entry was don't-care.
  CHECK(report.str().find("0xB3") != std::string::npos);
  std::remove(filename.c_str());
}

TEST_CASE("approximate dsd finds a zero-error decomposition", "[approximate_dsd]")
{
  const std::string filename = "/tmp/stp_approximate_dsd_regression.bench";
  stp::ApproximateDsdDecomposer decomposer;
  const auto result = decomposer.run("0xA0", {"c", "b", "a"}, "po", filename, 2);
  REQUIRE(result.valid);
  CHECK(result.hamming_distance == 0);
  CHECK(result.nodes == 2);
  std::remove(filename.c_str());
}

TEST_CASE("approximate dsd reports the distance of the emitted BENCH", "[approximate_dsd]")
{
  const std::string filename = "/tmp/stp_approximate_dsd_distance.bench";
  stp::ApproximateDsdDecomposer decomposer;
  const auto result = decomposer.run("0x1234", {"d", "c", "b", "a"}, "po", filename, 1, 1);
  REQUIRE(result.valid);
  CHECK(result.hamming_distance == 1);

  std::ifstream input(filename);
  REQUIRE(input.good());
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  _using_CUDA = false;
  simulator sim(graph);
  REQUIRE(sim.simulate());
  std::ostringstream report;
  sim.print_simulation_summary(report);
  CHECK(report.str().find("0x1230") != std::string::npos);
  std::remove(filename.c_str());
}
