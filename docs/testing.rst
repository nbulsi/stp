Testing
=======

The project uses Catch2 for unit tests and CTest for CMake test registration
and execution.  Catch2 contains the assertions; CTest discovers and launches
the compiled test executables.

Build and run
-------------

Configure, build, and run all registered tests::

  cmake -S . -B build
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure

The current expression-simulation test executable can also be invoked
directly::

  ./build/test/exprsim_tests

Simulation tests
----------------

``test/test_case/exprsim_test.cpp`` covers:

* all supported Boolean operators, including ``equ`` and ``imply``;
* nested expressions;
* the default alphabetical LSB-to-MSB order;
* explicit ``--inputs``-style ordering; and
* returning to the default order after an explicit-order simulation.

``test/test_case/lutsim_test.cpp`` covers LF and CRLF BENCH parsing plus the
bundled ``c17_lut``, ``cma152a``, ``t1``, and ``t2`` benchmarks.

When adding a test source, list it in ``test/CMakeLists.txt`` and register the
executable with ``add_test`` so it is visible to CTest.

BENCH sweep
-----------

Run every ``.bench`` fixture through the command-line ``lutsim`` interface::

  python3 test/run_lutsim_benchmarks.py

The script exits nonzero if a benchmark crashes, times out, or does not produce
a simulation report.  Use ``--pattern c17_lut.bench`` for a targeted run, or
``--timeout 120`` to increase the per-benchmark limit.

Generated ABC suites
--------------------

The checked-in ABC-produced BENCH networks are grouped by input count in
``test/benchmarks/vars/6var`` through ``test/benchmarks/vars/16var``.  Each
BENCH file has a sibling ``case_XX.expected.hex`` containing its expected
hexadecimal truth table.  Verify all sixty generated networks with::

  python3 test/verify_abc_benchmarks.py
