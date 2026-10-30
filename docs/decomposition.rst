Decomposition Commands
======================

Run these commands inside the STP shell. Each command writes a BENCH network;
use ``-o`` to choose its filename (default: ``result.bench``).

Exact decomposition: ``dsd``
----------------------------

Decompose a hexadecimal truth table without changing its function::

  dsd 8 -o exact.bench

Incomplete truth tables: ``idsd``
------------------------------------------------------------

Use a binary truth table containing ``0``, ``1``, and ``-`` (don't-care).
The result preserves specified values and chooses values for don't-cares::

  idsd 1--- -o incomplete.bench

Approximate decomposition: ``approximate_dsd``
------------------------------------------------------------

Allow a limited number of truth-table entries to change. ``--max-error`` is
the maximum Hamming distance (default: 1)::

  approximate_dsd 1234 --max-error 1 -o approximate.bench

LUT-pair decomposition: ``lutxx``
------------------------------------------------------------

Map a truth table to a 4-LUT or 6-LUT pair. Enter the truth table directly,
as with ``dsd``. The LUT size defaults to 4; use ``-x 6`` for a 6-LUT pair.
Hexadecimal input accepts an optional ``0x`` prefix and either letter case;
use ``0b`` for binary. The command reports when no decomposition is found::

  lutxx 8 -o lut.bench

Common options
--------------

* ``--output name`` sets the output signal name (default: ``po``).
* For ``dsd``, ``idsd``, and ``approximate_dsd``, ``--inputs a,b`` names inputs
  in MSB-to-LSB order; names are inferred when omitted. ``--threads N`` sets
  search threads (default: 1; 0 selects automatically).
* ``dsd`` and ``idsd`` accept ``--enumerate`` to list all feasible ACD
  decompositions.
* Add ``-h`` to any command for its complete option list.
