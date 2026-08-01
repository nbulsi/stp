#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <stp/io/logic_expr_parser.hpp>
#include <stp/sim/simulator.hpp>

namespace
{
struct SimulationResult
{
  std::string error;
  std::string report;
};

SimulationResult simulate_expression(const std::string &expression,
                                     const std::vector<std::string> &input_order = {})
{
  CircuitGraph graph;
  LogicExprParser parser;
  SimulationResult result;
  if (!parser.parse(expression, graph, result.error, input_order))
    return result;

  simulator sim(graph);
  sim.simulate();
  std::ostringstream report;
  sim.print_simulation_summary(report);
  result.report = report.str();
  return result;
}
} // namespace

TEST_CASE("exprsim evaluates supported Boolean operators", "[exprsim]")
{
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"(and a b)", "0x8"},   {"(or a b)", "0xE"},     {"(not a)", "0x1"},   {"(nand a b)", "0x7"},
      {"(nor a b)", "0x1"},   {"(xor a b)", "0x6"},    {"(equ a b)", "0x9"}, {"(xnor a b)", "0x9"},
      {"(imply a b)", "0xD"}, {"(xor a b c)", "0x96"},
  };

  for (const auto &[expression, truth_table] : cases)
  {
    const SimulationResult result = simulate_expression(expression);
    INFO(expression);
    REQUIRE(result.error.empty());
    CHECK(result.report.find(truth_table) != std::string::npos);
  }
}

TEST_CASE("exprsim handles nested expressions", "[exprsim]")
{
  const SimulationResult result = simulate_expression("(imply (equ a b) (or (equ a c) (xor b c)))");

  REQUIRE(result.error.empty());
  CHECK(result.report.find("0xFF") != std::string::npos);
}

TEST_CASE("exprsim defaults to alphabetical input order", "[exprsim]")
{
  const SimulationResult result = simulate_expression("(and (not c) (or a b))");

  REQUIRE(result.error.empty());
  CHECK(result.report.find("Input order (LSB -> MSB) : a, b, c") != std::string::npos);
  CHECK(result.report.find("0x0E") != std::string::npos);
}

TEST_CASE("exprsim accepts an explicit input order without changing the default", "[exprsim]")
{
  const std::string expression = "(and (not c) (or a b))";
  const SimulationResult explicit_order = simulate_expression(expression, {"c", "a", "b"});
  const SimulationResult default_order = simulate_expression(expression);

  REQUIRE(explicit_order.error.empty());
  CHECK(explicit_order.report.find("Input order (LSB -> MSB) : c, a, b") != std::string::npos);
  CHECK(explicit_order.report.find("0x54") != std::string::npos);

  REQUIRE(default_order.error.empty());
  CHECK(default_order.report.find("Input order (LSB -> MSB) : a, b, c") != std::string::npos);
  CHECK(default_order.report.find("0x0E") != std::string::npos);
}
