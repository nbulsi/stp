// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#include "commands/approximate_dsd.hpp"
#include "commands/dsd.hpp"
#include "commands/lut_xx.hpp"
#include "commands/show.hpp"
#include "commands/sim.hpp"

#if defined(ALICE_PYTHON) || defined(ALICE_CINTERFACE)
ALICE_MAIN(stp)
#else
int main(int argc, char **argv)
{
  _ALICE_MAIN_BODY(stp)
  cli.set_category("Visualization");
  cli.insert_command("show", std::make_shared<alice::circuit_show_command>(cli.env));

  if (argc == 1)
  {
    std::cout << "Ningbo University, STP - A Semi-Tensor Product Engine\n"
                 "Type 'help' for commands, 'quit' to exit.\n";
  }

  return cli.run(argc, argv);
}
#endif
