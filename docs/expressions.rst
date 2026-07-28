Boolean Expression Language
===========================

``exprsim`` accepts a parenthesized prefix language.  It converts an
expression into a temporary LUT network and then runs the same simulator used
by ``lutsim``.

Syntax
------

An expression is either a variable or an operation followed by its operands::

  expression := identifier | '(' operation expression... ')'

Identifiers start with a letter or ``_`` and may then contain letters, digits,
or ``_``.  Use lowercase operator names in user input.

For example::

  (and (or a b) (not c))

means :math:`(a \lor b) \land \lnot c`.

Operators
---------

=================  ========================  ================
Operator           Meaning                    Arity
=================  ========================  ================
``and``            conjunction                two or more
``or``             disjunction                two or more
``not``            negation                   exactly one
``nand``           negated conjunction        two or more
``nor``            negated disjunction        two or more
``xor``            exclusive-or               two or more
``xnor`` / ``equ`` equivalence                two or more
``imply``          implication, ``a -> b``    two
=================  ========================  ================

Multi-input operations are lowered to a chain of binary LUT gates.  For
example, ``(xor a b c)`` is evaluated as a binary XOR chain.

Input order and truth tables
----------------------------

By default, variables are sorted alphabetically before simulation.  The first
name is the least-significant bit (LSB).  Thus an expression containing
``a``, ``b``, and ``c`` uses::

  Input order (LSB -> MSB) : a, b, c

The report prints this ordering so that its hexadecimal truth table is
unambiguous.  To override it, pass every variable exactly once with
``--inputs``::

  exprsim --inputs c,a,b (and (not c) (or a b))

Here ``c`` is the LSB and ``b`` is the MSB.  ``--inputs`` applies only to the
current command; a later ``exprsim`` command returns to alphabetical order
unless it supplies its own option.

Examples
--------

Interactive shell::

  exprsim (and (or a b) (not c))
  exprsim --inputs c,a,b (and (not c) (or a b))
  exprsim --verbose (xor a b c)

From a host shell, quote the entire Alice command once::

  ./build/bin/stp -c 'exprsim (imply (equ a b) (or (equ a c) (xor b c)))'

Use ``--verbose`` to print every input/output assignment in addition to the
hexadecimal summary.
