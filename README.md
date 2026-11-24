# stp

`stp` is a Semi-Tensor Product (STP) engine for Electronic Design Automation
(EDA). It provides Boolean-expression and LUT-network simulation, truth-table
decomposition, visualization, and ternary-input SOP equivalence checking.
CPU simulation is available by default; NVIDIA CUDA acceleration is optional.

[Full documentation](https://stp-based-logic-synthesis.readthedocs.io/en/latest/)

## Build

Run the following commands from the repository root. Requirements:

- CMake 3.19 or newer.
- A C++17 compiler and a CMake-supported build tool.
- Python 3 for the CLI session tests and benchmark verification scripts.
  The C++ executable can be built without Python.

### CPU build

```bash
git clone https://github.com/nbulsi/stp.git
cd stp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTP_ENABLE_CUDA=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

The executable is `build/bin/stp`. CUDA is not required for this build.
Use `-DBUILD_TESTING=OFF` when configuring if you only need the executable.

### CUDA build

GPU backends require an NVIDIA GPU, a compatible NVIDIA driver, and a CUDA
Toolkit containing `nvcc`, headers, and development libraries. Check both the
GPU driver and compiler before building:

```bash
nvidia-smi
nvcc --version
```

The CUDA version shown by `nvidia-smi` describes driver support; it does not
confirm that the CUDA Toolkit is installed. If the Toolkit is installed in a
custom location, add its `bin` directory to `PATH` or pass
`-DCMAKE_CUDA_COMPILER=/path/to/cuda/bin/nvcc` when configuring.

For an A100 or A800 GPU (architecture `80`):

```bash
cmake -S . -B build/cuda -DCMAKE_BUILD_TYPE=Release \
  -DSTP_ENABLE_CUDA=ON -DSTP_CUDA_ARCHITECTURES=80
cmake --build build/cuda --parallel 4
ctest --test-dir build/cuda --output-on-failure
```

Choose an architecture supported by your GPU and installed Toolkit. Multiple
architectures can be specified as a quoted list, such as
`-DSTP_CUDA_ARCHITECTURES="80;86"`.

The GPU-enabled executable is **`build/cuda/bin/stp`**. Enabling CUDA at build
time does not change the default simulation backend: select a GPU backend at
runtime with `--backend`.

## Run simulations

### From a terminal

Pass an STP command using the executable's `-c` option. Quote the complete
command so that the host shell does not interpret expression parentheses:

```bash
./build/bin/stp -c 'lutsim test/benchmarks/mcnc/c17_lut.bench'
./build/bin/stp -c 'exprsim (and (or a b) (not c))'
./build/bin/stp -c 'lutsim --backend bitslice --threads 8 test/benchmarks/mcnc/c17_lut.bench'
```

The report includes the backend, active input support, input order, output
truth tables, and simulation runtime. The reported runtime measures the
simulation call, rather than the complete process startup, parsing, and output.

### From the interactive shell

Start the executable, then enter STP commands without the executable prefix:

```bash
./build/bin/stp
```

```text
help
lutsim test/benchmarks/mcnc/c17_lut.bench
lutsim --verbose test/benchmarks/mcnc/c17_lut.bench
exprsim --inputs b,a (imply a b)
quit
```

`lutsim` is the current circuit-simulation command; the old `sim -l` syntax is
not supported. File paths are relative to the directory where you launch STP.

### Simulation backends

Both `lutsim` and `exprsim` accept these values for `--backend`:

| Backend | Execution method | Requires CUDA |
| --- | --- | --- |
| `stp-cpu` | Partitioned STP with scalar truth-table application; default | No |
| `stp-gpu` | Partitioned STP using CUDA matrix operations | Yes |
| `stp-full-cpu` | Full STP expansion for each output cone | No |
| `stp-full-gpu` | Full STP expansion using CUDA matrix operations | Yes |
| `bitslice` | CPU simulation of 64 assignments per machine word | No |
| `bitslice-gpu` | GPU simulation using a compiled Boolean plan | Yes |
| `hybrid-cpu` | CPU STP compilation with bit-sliced truth-table application | No |
| `hybrid-gpu` | CUDA STP compilation with CPU bit-sliced truth-table application | Yes |

For example, use the CUDA-enabled executable to select GPU word simulation:

```bash
./build/cuda/bin/stp -c 'lutsim --backend bitslice-gpu test/benchmarks/mcnc/c17_lut.bench'
```

Useful simulation options:

- `--verbose`: print every input/output assignment; output grows exponentially
  with the number of active inputs.
- `--no-truth-table`: omit the hexadecimal output tables from the report.
- `--threads N`: request CPU bit-slice threads; `0` selects automatically.
  Other backends currently report one thread, and small bit-slice workloads
  may also run with one thread.
- `--inputs a,b`: set expression inputs in LSB-to-MSB order (`exprsim` only).
- `--cuda`: select `bitslice-gpu` for `lutsim`, or `stp-gpu` for `exprsim`.

The executable's `-c` passes an STP command; inside a simulation command, `-c`
is also an alias for `--cuda`. Explicit `--backend` names avoid this ambiguity.
GPU initialization and data transfer can outweigh computation on small
circuits, so compare backends on representative workloads.

### Supported BENCH format

`lutsim` accepts LUT-style BENCH files with `INPUT`, `OUTPUT`, and hexadecimal
LUT assignments. For example, a two-input AND gate is:

```text
INPUT(a)
INPUT(b)
OUTPUT(y)
y = LUT 0x8 (a, b)
```

Named gate statements such as `y = AND(a, b)` are not currently accepted by
`LutParser`; convert them to LUT assignments before simulation.

## Other commands

Run these examples in the interactive shell. Decomposition commands write
BENCH files; `-o` chooses the destination:

```text
dsd 8 -o exact.bench
idsd 1--- -o incomplete.bench
approximate_dsd 1234 --max-error 1 -o approximate.bench
lutxx 8 -o lut.bench
sopcec --lhs "A0+A1+A2" --rhs "1" --stats
```

Use `read_truth` and `read_bench` to populate session stores. See the
[documentation](https://stp-based-logic-synthesis.readthedocs.io/en/latest/)
for storage, visualization, expression syntax, and decomposition details.
Enter `command -h`, such as `lutsim -h`, to inspect command options.

## Tests and benchmark verification

```bash
ctest --test-dir build --output-on-failure
python3 test/verify_abc_benchmarks.py --stp build/bin/stp
```

With Python 3 available, CTest also runs these 60 generated ABC fixtures as
`abc_truth_table_tests`, comparing their expected hexadecimal truth tables.
A broader BENCH smoke test is also available:

```bash
python3 test/run_lutsim_benchmarks.py --stp build/bin/stp --timeout 30
```

The smoke test checks process completion and report generation, rather than
expected output values. Large miter fixtures may exceed the timeout, available
memory, or exhaustive truth-table support limits.

For a CUDA build, run GPU runtime tests with:

```bash
ctest --test-dir build/cuda -L cuda --output-on-failure
```

CPU-only checks in the same build can be selected with `-L cpu`. Without an
explicit `STP_CUDA_ARCHITECTURES`, the build follows the CUDA toolchain's default;
select your GPU architecture explicitly for performance testing.
