#include <fmt/format.h>
#include <iostream>

namespace stp
{
inline void MyStp::getVersion()
{
  std::cout << fmt::format( "STP engine version is {}\n", 0.02 ) << std::endl;
}
}  // namespace stp
