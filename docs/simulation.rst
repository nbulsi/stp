Simulation Commands
===================

The interactive executable provides two logic-simulation commands.  Both
print the input count, the input order from LSB to MSB, output truth tables in
hexadecimal, and runtime.  Add ``--verbose`` to include the complete truth
table.

LUT BENCH networks: ``lutsim``
------------------------------

Use ``lutsim`` for the project's LUT-style BENCH files::

  lutsim test/benchmarks/mcnc/c17_lut.bench
  lutsim --verbose test/benchmarks/mcnc/c17_lut.bench
  lutsim --cuda test/benchmarks/mcnc/c17_lut.bench

The parser recognizes primary-input and primary-output declarations plus LUT
assignments.  A representative file is::

  INPUT(n1)
  INPUT(n2)
  OUTPUT(n3)
  n3 = LUT 0x8 (n1, n2)

``lutsim`` is the current BENCH command; no ``-l`` network-type option is
required.

Expressions: ``exprsim``
------------------------

Use ``exprsim`` for Boolean expressions.  It accepts all remaining expression
tokens, so quotes are optional in the interactive shell::

  exprsim (and (or a b) (not c))

For non-interactive execution, quote the complete command for the host shell::

  ./build/bin/stp -c 'exprsim (and (or a b) (not c))'

See :doc:`expressions` for the grammar, supported operators, and input-order
rules.

How simulation is organized
---------------------------

``LutParser`` or ``LogicExprParser`` constructs a ``CircuitGraph``.  The
``simulator`` then groups the graph by logic depth.  Nodes with multiple
fanouts are treated as cut boundaries: their truth tables are computed first,
then downstream regions use those values as variables.  For each region,
``expr_chain_parser`` normalizes the expression chain, combines the LUT and
STP operators, and produces a truth table.  The simulator uses that table to
fill the output values for every primary-input assignment.
