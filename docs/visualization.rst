Circuit Visualization
=====================

Use ``show`` inside STP to draw a LUT BENCH circuit::

  show test/benchmarks/mcnc/c17_lut.bench
  show test/benchmarks/mcnc/c17_lut.bench -o circuit.png
  show test/benchmarks/mcnc/c17_lut.bench --no-open

By default, ``show`` writes ``c17_lut.png`` and ``c17_lut.dot`` in the current
directory and opens the image with the system viewer (macOS or Linux).
Use ``--no-open`` on servers or when only generating files.

PNG and SVG rendering require Graphviz's ``dot`` executable on ``PATH``.
Use ``-o circuit.svg`` for SVG, or ``-o circuit.dot`` to export DOT without
rendering or opening a viewer. DOT export needs no external tool.

Inputs are blue circles and outputs are pale orange ellipses. Common LUT
functions show only their function name, without signal names or input counts:

* Circular ``AND``: green; ``OR``: yellow; two-input ``XOR``: purple.
* Circular ``MAJ`` (majority): pink; three-input ``XOR``: indigo.
* Circular ``IMP``: cyan, for two-input implication (``0xb`` or ``0xd``).
  ``A`` is the antecedent and ``B`` the consequent: ``A -> B``.
* ``MUX``: orange trapezoid, with a side selector ``S`` and data inputs
  ``D0`` and ``D1`` (``S=0`` selects ``D0``).

Complemented functions use the base function's name, color, and circular shape,
with inversion on the output edge: NAND becomes ``AND``, NOR becomes ``OR``,
XNOR becomes ``XOR`` (two or three inputs), and MIN becomes ``MAJ``.
XNOR uses output inversion rather than an inverted input.

Buffers (one-input ``0x2``) are omitted. Inverters (one-input ``0x1``) are
represented by dashed edges rather than separate nodes. Two-input ``0x2``
and ``0x4`` appear as ``AND`` with a dashed edge on the inverted input.
Input and output inversions combine along each connection: two inversions cancel to a solid
edge. LUTs that simply copy or invert one of two inputs (``0x3``, ``0x5``,
``0xa``, ``0xc``) are also folded into edges. This changes only the drawing,
not the BENCH circuit.

MUX recognition supports all selector/data input permutations. Other LUTs
remain light green boxes. The node tooltip retains the hexadecimal truth table.

Signals flow from bottom to top, with primary inputs at the bottom and primary
outputs at the top. Generic LUT boxes show their signal name, input count, and
hexadecimal truth table; long tables are shortened, with the full value in the
SVG tooltip. ``in0`` is the first input in the BENCH statement and the least
significant truth-table index bit.

To view a decomposition result::

  dsd A0 -o decomposed.bench
  show decomposed.bench

An existing DOT file can also be rendered outside STP::

  dot -Tpng circuit.dot -o circuit.png
