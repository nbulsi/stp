// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT
#pragma once

#include "../stores/session.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stp/io/lut_parser.hpp>
#include <stp/visualization/dot.hpp>
#include <utility>

namespace alice
{
class circuit_show_command : public command
{
public:
  explicit circuit_show_command(const environment::ptr &env)
      : command(env, "Visualize a LUT BENCH circuit as PNG, SVG, or DOT")
  {
    add_option("filename", filename, "input LUT BENCH file");
    add_option("-o,--output", output,
               "output .png, .svg, or .dot file (default: input basename.png)");
    add_flag("-t,--truth", use_truth, "show the current truth table");
    add_flag("-b,--bench", use_bench, "show the current BENCH circuit");
    add_flag("--no-open", no_open, "generate files without opening a viewer");
  }

protected:
  void execute() override
  {
    auto input_name = std::exchange(filename, {});
    auto output_name = std::exchange(output, {});
    const bool skip_open = std::exchange(no_open, false);
    const bool truth = std::exchange(use_truth, false);
    const bool bench = std::exchange(use_bench, false);
    try
    {
      if ((truth && bench) || (!input_name.empty() && (truth || bench)))
        throw std::runtime_error("choose a filename or one store (-t/-b)");
      CircuitGraph graph;
      if (!input_name.empty())
      {
        auto entry = stp::read_bench_snapshot(input_name);
        graph = std::move(entry.graph);
      }
      else
      {
        const bool choose_truth = truth || (!bench && env->default_option() == "truth");
        if (choose_truth)
        {
          const auto &entries = store<stp::StoredTruth>();
          if (entries.empty())
            throw std::runtime_error("truth store is empty; use read_truth first");
          graph = entries.current().graph;
          input_name = "truth_" + std::to_string(entries.current_index()) + ".bench";
        }
        else
        {
          const auto &entries = store<stp::StoredBench>();
          if (entries.empty())
            throw std::runtime_error("BENCH store is empty; use read_bench or read_truth first");
          graph = entries.current().graph;
          input_name = entries.current().name;
        }
      }
      graph.match_logic_depth();
      if (output_name.empty())
        output_name =
            std::filesystem::path(input_name).filename().replace_extension(".png").string();
      const auto extension = std::filesystem::path(output_name).extension().string();
      if (extension != ".dot" && extension != ".svg" && extension != ".png")
        throw std::runtime_error("output must end in .png, .svg, or .dot");
      auto dot_path = std::filesystem::path(output_name);
      dot_path.replace_extension(".dot");
      if (std::filesystem::absolute(input_name).lexically_normal() ==
              std::filesystem::absolute(dot_path).lexically_normal() ||
          std::filesystem::absolute(input_name).lexically_normal() ==
              std::filesystem::absolute(output_name).lexically_normal())
        throw std::runtime_error("output must differ from input");
      {
        std::ofstream dot(dot_path);
        if (!dot)
          throw std::runtime_error("cannot write " + dot_path.string());
        stp::write_dot(graph, dot);
      }
      env->out() << "[i] DOT written to " << dot_path.string() << '\n';
      if (extension != ".dot")
      {
        if (std::system("command -v dot >/dev/null 2>&1") != 0)
        {
          env->out() << "[w] Graphviz 'dot' was not found on PATH. Only DOT was generated; "
                        "no image was rendered or opened.\n"
                        "[i] Install Graphviz: macOS: brew install graphviz; "
                        "Ubuntu/Debian: sudo apt install graphviz\n"
                     << "[i] Then rerun show, or render the saved DOT file with dot.\n";
          return;
        }
        // Quote filenames as literal POSIX shell arguments, including embedded quotes.
        auto quote = [](const std::string &s) {
          std::string result = "'";
          for (char c : s)
            result += c == '\'' ? "'\\''" : std::string(1, c);
          return result + "'";
        };
        const auto cmd = "dot -T" + extension.substr(1) + " " +
                         quote(std::filesystem::absolute(dot_path).string()) + " -o " +
                         quote(std::filesystem::absolute(output_name).string());
        if (std::system(cmd.c_str()) != 0)
          throw std::runtime_error(
              "Image rendering failed; install Graphviz and ensure dot is on PATH. DOT retained.");
        env->out() << "[i] Image written to " << output_name << '\n';
        if (!skip_open)
        {
          const auto image = quote(std::filesystem::absolute(output_name).string());
#if defined(__APPLE__)
          const auto open_cmd = "open " + image;
#elif defined(__linux__)
          const auto open_cmd =
              "command -v xdg-open >/dev/null 2>&1 && (xdg-open " + image + " >/dev/null 2>&1 &)";
#else
          const std::string open_cmd;
#endif
          if (open_cmd.empty() || std::system(open_cmd.c_str()) != 0)
            env->out() << "[w] Could not open viewer; open " << output_name
                       << " manually or use --no-open.\n";
        }
      }
    }
    catch (const std::exception &error)
    {
      env->out() << "[e] " << error.what() << '\n';
    }
  }

private:
  std::string filename, output;
  bool no_open = false, use_truth = false, use_bench = false;
};
// Replace Alice's generic store viewer while keeping its store/current commands.
template <> class show_command<stp::StoredTruth, stp::StoredBench> : public circuit_show_command
{
public:
  using circuit_show_command::circuit_show_command;
};
} // namespace alice
