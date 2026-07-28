STP Computation
===============

The basic Semi-Tensor Product (STP) computation.

Basic STP Computation
----------------------

Native Definition
^^^^^^^^^^^^^^^^^^^^^

Given two matrices :math:`A_{m \times n}` and :math:`B_{p \times q}`, the STP
of :math:`A` and :math:`B` is defined as 

.. math::

  A \ltimes B = (A \bigotimes I_{t/n}) \cdot (B \bigotimes I_{t/p})

where :math:`\bigotimes` is Kronecker product, :math:`I` is identity
matrix, and :math:`t` is least common multiple (LCM) of :math:`n` and :math:`p`.
As an example, suppose 

.. math::

  A = \begin{bmatrix}
  1 & 0 & 0 & 0 \\
  0 & 1 & 1 & 1
  \end{bmatrix}

and

.. math::

  B = \begin{bmatrix}
  1 & 1 & 0 & 1 \\
  0 & 0 & 1 & 0
  \end{bmatrix},

then :math:`m=2, n=4` and :math:`p=2, q=4`, the LCM :math:`t` is thus 4.
According to the definition, 

.. math::

  \begin{align}
  A \ltimes B = (A \bigotimes I_{4/4}) \cdot (B \bigotimes I_{4/2}) &=
  \begin{bmatrix}
  1 & 0 & 0 & 0 \\
  0 & 1 & 1 & 1
  \end{bmatrix} \cdot
  \begin{bmatrix}
  1 & 0 & 1 & 0 & 0 & 0 & 1 & 0 \\
  0 & 1 & 0 & 1 & 0 & 0 & 0 & 1 \\
  0 & 0 & 0 & 0 & 1 & 0 & 0 & 0 \\
  0 & 0 & 0 & 0 & 0 & 1 & 0 & 0 
  \end{bmatrix} \notag \\ 
  &= 
  \begin{bmatrix}
  1 & 0 & 1 & 0 & 0 & 0 & 1 & 0 \\
  0 & 1 & 0 & 1 & 1 & 1 & 0 & 1
  \end{bmatrix}. \notag
  \end{align}

Matrix Chain STP Computation
----------------------------
When we have :math:`n` matrices multiplication and :math:`n \ge 3`, we call
this as matrix chain STP computation. 

Sequence
^^^^^^^^^^^^^^^^^^^^^
For four matrices :math:`A`, :math:`B`, :math:`C`, and :math:`D`, sequential
evaluation is:

.. math::
  ABCD = (((AB)C)D).
