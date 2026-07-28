Quick Start
===========

STP is a C++17 logic-simulation tool built around semi-tensor-product (STP)
evaluation.  The default build uses the compressed-vector implementation; an
Eigen reference backend and CUDA acceleration are optional.

Build
-----

The default build has no Eigen or CUDA requirement::

  git clone https://github.com/nbulsi/stp.git
  cd stp
  cmake -S . -B build
  cmake --build build --parallel

The resulting interactive executable is ``build/bin/stp``.  Start it with::

  ./build/bin/stp

Optional backends
-----------------

Enable the Eigen reference backend when validating the compressed-vector
implementation::

  cmake -S . -B build/eigen -DSTP_ENABLE_EIGEN=ON
  cmake --build build/eigen --parallel

If Eigen is installed outside CMake's default search locations, pass its
prefix with ``-DCMAKE_PREFIX_PATH=/path/to/eigen``.

Enable CUDA only on a system with a compatible NVIDIA driver and CUDA Toolkit::

  cmake -S . -B build/cuda -DSTP_ENABLE_CUDA=ON
  cmake --build build/cuda --parallel

When CUDA support is compiled in, add ``--cuda`` (or ``-c``) to a simulation
command to select it at runtime.

Command-line mode
-----------------

The executable accepts Alice command strings through ``--command`` (or
``-c``).  Quote the complete command once for the host shell; the expression
itself does not need a second layer of double quotes::

  ./build/bin/stp -c 'lutsim test/benchmarks/mcnc/c17_lut.bench'
  ./build/bin/stp -c 'exprsim (and (or a b) (not c))'

See :doc:`simulation` for the supported simulation commands and
:doc:`expressions` for the expression language.
