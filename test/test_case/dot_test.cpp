#include <catch2/catch.hpp>
#include <sstream>
#include <stp/io/lut_parser.hpp>
#include <stp/visualization/dot.hpp>

TEST_CASE("DOT export preserves LUT tables and BENCH port order", "[visualization]")
{
  std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x2 (a, b)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream output;
  stp::write_dot(graph, output);
  const auto dot = output.str();
  REQUIRE(dot.find("label=\"AND\"") != std::string::npos);
  REQUIRE(dot.find("pi0 -> g0 [label=\"in0\"]") != std::string::npos);
  REQUIRE(dot.find("pi1 -> g0 [label=\"in1\", style=dashed]") != std::string::npos);
  REQUIRE(dot.find("g0 -> po2") != std::string::npos);
}

TEST_CASE("DOT escaping and wide truth tables do not truncate tooltip data", "[visualization]")
{
  REQUIRE(stp::dot_quote("a\"\\\nb") == "\"a\\\"\\\\\\nb\"");
  CircuitGraph graph;
  std::istringstream input(
      "INPUT(a)\nINPUT(b)\nINPUT(c)\nINPUT(d)\nINPUT(e)\nINPUT(f)\nINPUT(g)\nOUTPUT(y)\ny = LUT "
      "0x0123456789abcdef0123456789abcdef (a,b,c,d,e,f,g)\n");
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream output;
  stp::write_dot(graph, output);
  REQUIRE(output.str().find("tooltip=\"0x0123456789abcdef0123456789abcdef\"") != std::string::npos);
}

TEST_CASE("DOT export handles direct outputs and rejects undriven signals", "[visualization]")
{
  CircuitGraph direct;
  const auto id = direct.add_input("a");
  direct.add_output("a");
  std::ostringstream output;
  stp::write_dot(direct, output);
  REQUIRE(output.str().find("pi" + std::to_string(id) + " -> po" + std::to_string(id)) !=
          std::string::npos);

  CircuitGraph invalid;
  invalid.add_output("missing");
  REQUIRE_THROWS_AS(stp::write_dot(invalid, output), std::runtime_error);
}

TEST_CASE("Common LUT functions use compact symbols", "[visualization]")
{
  const std::vector<std::pair<std::string, std::string>> functions = {
      {"8", "AND"}, {"e", "OR"}, {"6", "XOR"}, {"e8", "MAJ"}, {"96", "XOR"}, {"ca", "MUX"}};
  std::set<std::string> colors;
  for (const auto &function : functions)
  {
    const bool three = function.first.size() == 2;
    std::istringstream input("INPUT(a)\nINPUT(b)\nINPUT(c)\nOUTPUT(y)\ny = LUT 0x" +
                             function.first + (three ? " (a,b,c)\n" : " (a,b)\n"));
    CircuitGraph graph;
    LutParser parser;
    REQUIRE(parser.parse(input, graph));
    std::ostringstream output;
    stp::write_dot(graph, output);
    REQUIRE(output.str().find(function.second == "MUX" ? "g0 [shape=trapezium"
                                                       : "g0 [shape=circle") != std::string::npos);
    REQUIRE(output.str().find("label=\"" + function.second + "\"") != std::string::npos);
    REQUIRE(output.str().find(function.second) != std::string::npos);
    REQUIRE(output.str().find("tooltip=\"0x" + function.first + "\"") != std::string::npos);
    if (function.second == "MUX")
    {
      REQUIRE(output.str().find("pi2 -> g0:e [label=\"S\"]") != std::string::npos);
      REQUIRE(output.str().find("pi0 -> g0:s [label=\"D0\"]") != std::string::npos);
      REQUIRE(output.str().find("pi1 -> g0:s [label=\"D1\"]") != std::string::npos);
    }
    colors.insert(stp::lut_appearance(three ? 3 : 2, "0x" + function.first).color);
  }
  REQUIRE(colors.size() == functions.size());
  for (const auto &hex : {"0xac", "0xca", "0xb8", "0xd8", "0xe2", "0xe4"})
    REQUIRE(stp::lut_appearance(3, hex).name == "MUX");
  // Unrelated functions remain generic LUTs.
  for (const auto &hex : {"0x35", "0x00", "0xff"})
    REQUIRE(stp::lut_appearance(3, hex).name.empty());
}

TEST_CASE("Complemented LUT functions retain their family colors", "[visualization]")
{
  struct Function
  {
    size_t inputs;
    std::string positive, negative, name;
  };
  const std::vector<Function> functions = {{2, "8", "7", "AND"},
                                           {2, "e", "1", "OR"},
                                           {2, "6", "9", "XOR"},
                                           {3, "e8", "17", "MAJ"},
                                           {3, "96", "69", "XOR"}};
  for (const auto &function : functions)
  {
    std::istringstream input("INPUT(a)\nINPUT(b)\nINPUT(c)\nOUTPUT(y)\ny = LUT 0x" +
                             function.negative +
                             (function.inputs == 3 ? " (a,b,c)\n" : " (a,b)\n"));
    CircuitGraph graph;
    LutParser parser;
    REQUIRE(parser.parse(input, graph));
    std::ostringstream dot;
    stp::write_dot(graph, dot);
    REQUIRE(dot.str().find("g0 [shape=circle") != std::string::npos);
    REQUIRE(dot.str().find("label=\"" + function.name + "\"") != std::string::npos);
    REQUIRE(dot.str().find("g0 -> po3 [style=dashed]") != std::string::npos);
    REQUIRE(dot.str().find("pi0 -> g0 [label=\"in0\"]") != std::string::npos);
    REQUIRE(dot.str().find("pi1 -> g0 [label=\"in1\"]") != std::string::npos);
    REQUIRE(stp::lut_appearance(function.inputs, "0x" + function.positive).color ==
            stp::lut_appearance(function.inputs, "0x" + function.negative).color);
  }
}

TEST_CASE("Unary inverter is distinguished from buffer", "[visualization]")
{
  std::istringstream input("INPUT(a)\nOUTPUT(y)\ny = LUT 0x1 (a)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream dot;
  stp::write_dot(graph, dot);
  REQUIRE(dot.str().find("g0 [shape=") == std::string::npos);
  REQUIRE(dot.str().find("pi0 -> po1 [style=dashed]") != std::string::npos);
  REQUIRE(stp::lut_appearance(1, "0x2").name.empty());
  REQUIRE(stp::lut_appearance(2, "0x1").name == "OR");
}

TEST_CASE("Buffers and inversion chains collapse independently on each fanout", "[visualization]")
{
  std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(y)\nOUTPUT(z)\nOUTPUT(na)\n"
                           "buf = LUT 0x2 (a)\nna = LUT 0x1 (buf)\n"
                           "again = LUT 0x1 (na)\n"
                           "y = LUT 0x4 (na,b)\nz = LUT 0x8 (na,b)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream out;
  stp::write_dot(graph, out);
  const auto dot = out.str();
  for (int id = 0; id < 3; ++id)
    REQUIRE(dot.find("g" + std::to_string(id) + " [shape=") == std::string::npos);
  // NOT on the AND's first input cancels the explicit inverter on na.
  REQUIRE(dot.find("pi0 -> g3 [label=\"in0\"]") != std::string::npos);
  // The same na signal remains inverted on its other fanout and output.
  REQUIRE(dot.find("pi0 -> g4 [label=\"in0\", style=dashed]") != std::string::npos);
  REQUIRE(dot.find("pi0 -> po" + std::to_string(graph.get_line("na")) + " [style=dashed]") !=
          std::string::npos);
}

TEST_CASE("Projection LUTs and double inversions preserve direct outputs", "[visualization]")
{
  for (const auto &hex : {"2", "a", "c", "5", "3"})
  {
    const bool unary = std::string(hex) == "2";
    const bool second = std::string(hex) == "c" || std::string(hex) == "3";
    const bool inverted = std::string(hex) == "5" || std::string(hex) == "3";
    std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x" + std::string(hex) +
                             (unary ? " (a)\n" : " (a,b)\n"));
    CircuitGraph graph;
    LutParser parser;
    REQUIRE(parser.parse(input, graph));
    std::ostringstream out;
    stp::write_dot(graph, out);
    REQUIRE(out.str().find("g0 [shape=") == std::string::npos);
    REQUIRE(out.str().find(std::string(second ? "pi1" : "pi0") + " -> po2" +
                           (inverted ? " [style=dashed]" : "") + ";") != std::string::npos);
  }
  std::istringstream input("INPUT(a)\nOUTPUT(y)\nx = LUT 0x1 (a)\ny = LUT 0x1 (x)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream out;
  stp::write_dot(graph, out);
  REQUIRE(out.str().find("pi0 -> po1;") != std::string::npos);
  REQUIRE(out.str().find("style=dashed") == std::string::npos);
}

TEST_CASE("Implication shows antecedent A and consequent B for either input order",
          "[visualization]")
{
  for (const auto &hex : {"b", "d"})
  {
    std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x" + std::string(hex) +
                             " (a,b)\n");
    CircuitGraph graph;
    LutParser parser;
    REQUIRE(parser.parse(input, graph));
    std::ostringstream out;
    stp::write_dot(graph, out);
    REQUIRE(out.str().find("label=\"IMP\"") != std::string::npos);
    const bool forward = std::string(hex) == "d";
    REQUIRE(out.str().find(std::string(forward ? "pi0" : "pi1") + " -> g0 [label=\"A\"]") !=
            std::string::npos);
    REQUIRE(out.str().find(std::string(forward ? "pi1" : "pi0") + " -> g0 [label=\"B\"]") !=
            std::string::npos);
  }
}

TEST_CASE("Complemented outputs cancel inversions separately for each fanout", "[visualization]")
{
  std::istringstream input("INPUT(a)\nINPUT(b)\nOUTPUT(n)\nOUTPUT(positive)\nOUTPUT(y)\nOUTPUT(z)\n"
                           "n = LUT 0x7 (a,b)\nbuf = LUT 0x2 (n)\npositive = LUT 0x1 (buf)\n"
                           "y = LUT 0x4 (buf,a)\nz = LUT 0xe (buf,a)\n");
  CircuitGraph graph;
  LutParser parser;
  REQUIRE(parser.parse(input, graph));
  std::ostringstream out;
  stp::write_dot(graph, out);
  const auto dot = out.str();
  REQUIRE(dot.find("g0 -> po2 [style=dashed]") != std::string::npos);
  REQUIRE(dot.find("g0 -> po3;") != std::string::npos);
  REQUIRE(dot.find("g0 -> g3 [label=\"in0\"]") != std::string::npos);
  REQUIRE(dot.find("g0 -> g4 [label=\"in0\", style=dashed]") != std::string::npos);
  REQUIRE(dot.find("label=\"NAND\"") == std::string::npos);
}
