// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <bitset>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using stp_data = uint32_t;
using id = stp_data;
using stp_expr = std::vector<id>;

namespace stp
{

inline void print_hex(const std::string &binary_string, std::ostream &os = std::cout)
{
  assert(binary_string.length() % 4 == 0);

  for (size_t i = 0; i < binary_string.length(); i += 4)
  {
    std::string block = binary_string.substr(i, 4);

    int decimal_value = std::bitset<4>(block).to_ulong();
    char hex_digit;

    if (decimal_value < 10)
    {
      hex_digit = '0' + decimal_value;
    }
    else
    {
      hex_digit = 'A' + (decimal_value - 10);
    }

    os << hex_digit;
  }
}

inline std::vector<std::string> split(const std::string &input, const std::string &delimiters)
{
  std::vector<std::string> result;
  std::string token;

  for (const char character : input)
  {
    if (delimiters.find(character) == std::string::npos)
    {
      token += character;
      continue;
    }

    if (!token.empty())
    {
      result.push_back(token);
      token.clear();
    }
  }

  if (!token.empty())
    result.push_back(token);

  return result;
}
} // namespace stp
