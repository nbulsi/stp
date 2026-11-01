// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT
#pragma once
#include "../stores/session.hpp"
#include <utility>

namespace alice
{
class read_truth_command : public command
{
public:
  explicit read_truth_command(const environment::ptr &env)
      : command(env, "Append a truth table to the session store")
  {
    add_option("truth_table", text, "hexadecimal truth table (optional 0x); 0b for binary");
  }

protected:
  void execute() override
  {
    const auto value = std::exchange(text, {});
    try
    {
      save_truth(env, stp::make_truth(value));
      env->out() << "[i] Truth [" << store<stp::StoredTruth>().current_index() << "] "
                 << to_string(store<stp::StoredTruth>().current()) << '\n';
    }
    catch (const std::exception &error)
    {
      env->out() << "[e] " << error.what() << '\n';
    }
  }

private:
  std::string text;
};
ALICE_ADD_COMMAND(read_truth, "Storage");

class read_bench_command : public command
{
public:
  explicit read_bench_command(const environment::ptr &env)
      : command(env, "Append a LUT BENCH snapshot to the session store")
  {
    add_option("filename", filename, "LUT BENCH filename");
  }

protected:
  void execute() override
  {
    const auto path = std::exchange(filename, {});
    try
    {
      save_bench(env, stp::read_bench_snapshot(path));
      env->out() << "[i] BENCH [" << store<stp::StoredBench>().current_index() << "] "
                 << to_string(store<stp::StoredBench>().current()) << '\n';
    }
    catch (const std::exception &error)
    {
      env->out() << "[e] " << error.what() << '\n';
    }
  }

private:
  std::string filename;
};
ALICE_ADD_COMMAND(read_bench, "Storage");
} // namespace alice
