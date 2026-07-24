# stp

## Introduction

`stp` is a Semi-Tensor Product (STP) engine for Electronic Design Automation (EDA).
The default implementation uses a compressed-vector representation for logic simulation.

Optional backends serve distinct purposes:

- Eigen: reference backend for validating the compressed-vector implementation.
- CUDA: accelerated backend for larger workloads.

[Read the full documentation.](https://stp-based-logic-synthesis-tool.readthedocs.io/en/latest/ )

## Build

The default build does not require Eigen or CUDA.

```bash
git clone https://github.com/nbulsi/stp.git
cd stp
cmake -S . -B build
cmake --build build -j
./build/bin/stp
```

### Eigen reference backend

Enable Eigen only when building or validating the Eigen-based reference backend:

```bash
cmake -S . -B build/eigen -DSTP_ENABLE_EIGEN=ON
cmake --build build/eigen -j
```

If CMake cannot locate Eigen3, install it and provide its installation prefix:

```bash
cmake -S . -B build/eigen -DSTP_ENABLE_EIGEN=ON \
  -DCMAKE_PREFIX_PATH=/path/to/eigen-install-prefix
```

### CUDA acceleration backend

Enable CUDA only when the host has an NVIDIA GPU, a compatible NVIDIA driver, and a CUDA Toolkit
whose `nvcc` compiler is available on `PATH`. Verify the toolchain with `nvcc --version`.

```bash
cmake -S . -B build/cuda -DSTP_ENABLE_CUDA=ON
cmake --build build/cuda -j
```

The Eigen and CUDA options are independent and can be enabled together when needed.

## Use as a command-line tool

Start the interactive shell:

```bash
./build/bin/stp
```

Simulate a LUT BENCH circuit. The default report prints each output truth table in hexadecimal and
the simulation runtime:

```bash
sim -l yourcase.bench
```

Print the detailed input/output truth table as well:

```bash
sim -l --verbose yourcase.bench
```
