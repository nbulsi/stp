Session Stores
==============

Keep multiple truth tables and LUT BENCH circuits in the current STP session::

  read_truth A0
  read_truth 0xe8
  store -t

  read_bench input.bench
  read_bench another.bench
  store -b

``read_truth`` accepts hexadecimal values with an optional ``0x`` prefix
(case-insensitive), or binary values prefixed with ``0b``. Each successful
read appends a new entry and makes it current. ``store -t`` lists truth tables;
``store -b`` lists BENCH circuits. ``*`` marks the current entry in each store.

``show`` without a filename displays the current entry from the most recently
used store. Select a store explicitly or switch to an older entry by its
zero-based index::

  show
  show -t
  show -b --no-open
  current -b 0
  show
  current -t 0
  show

After ``read_truth``, decomposition commands can omit the truth-table argument::

  read_truth A0
  dsd
  show

``dsd``, ``idsd``, ``approximate_dsd``, and ``lutxx`` automatically append
successful BENCH results. This also works when the truth table is supplied
directly to the decomposition command. ``idsd`` uses the stored complete table
as binary; incompletely specified tables still use its positional argument.

BENCH entries contain circuit snapshots, so overwriting the same output file
(e.g. ``result.bench``) does not change older entries. Stores live in memory
and are cleared when STP exits; BENCH files written by commands remain on disk.
Use ``store -t --clear`` or ``store -b --clear`` to clear a store manually.
