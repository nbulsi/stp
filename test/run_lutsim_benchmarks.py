#!/usr/bin/env python3
"""Run lutsim against every BENCH fixture under test/benchmarks."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import time


TEST_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TEST_DIR.parent
DEFAULT_STP = PROJECT_DIR / "build" / "bin" / "stp"
BENCHMARK_DIR = TEST_DIR / "benchmarks"


def alice_quote(value: str) -> str:
    """Quote one Alice shell token for the command passed through ``stp -c``."""
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--stp",
        type=Path,
        default=DEFAULT_STP,
        help=f"path to the stp executable (default: {DEFAULT_STP})",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="maximum seconds per benchmark (default: 60)",
    )
    parser.add_argument(
        "--pattern",
        default="*.bench",
        help="glob pattern below test/benchmarks (default: *.bench)",
    )
    return parser.parse_args()


def run_benchmark(executable: Path, benchmark: Path, timeout: float) -> tuple[bool, str]:
    command = f"lutsim {alice_quote(str(benchmark))}"
    try:
        result = subprocess.run(
            [str(executable), "-c", command],
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return False, f"timed out after {timeout:g} seconds"

    output = result.stdout + result.stderr
    if result.returncode != 0:
        return False, f"stp exited with status {result.returncode}\n{output.strip()}"
    if "Report : STP logic simulation" not in output:
        return False, f"lutsim did not produce a simulation report\n{output.strip()}"
    return True, ""


def main() -> int:
    args = parse_arguments()
    executable = args.stp.resolve()
    if not executable.is_file():
        print(f"error: stp executable not found: {executable}", file=sys.stderr)
        return 2

    benchmarks = sorted(BENCHMARK_DIR.rglob(args.pattern))
    if not benchmarks:
        print(f"error: no benchmarks matched {args.pattern!r} below {BENCHMARK_DIR}", file=sys.stderr)
        return 2

    failures: list[tuple[Path, str]] = []
    start = time.monotonic()
    for index, benchmark in enumerate(benchmarks, start=1):
        relative_name = benchmark.relative_to(PROJECT_DIR)
        passed, detail = run_benchmark(executable, benchmark, args.timeout)
        status = "PASS" if passed else "FAIL"
        print(f"[{index:3}/{len(benchmarks):3}] {status} {relative_name}")
        if not passed:
            failures.append((relative_name, detail))

    elapsed = time.monotonic() - start
    print(f"\n{len(benchmarks) - len(failures)}/{len(benchmarks)} benchmarks passed in {elapsed:.2f}s")
    if not failures:
        return 0

    print("\nFailures:", file=sys.stderr)
    for benchmark, detail in failures:
        print(f"\n- {benchmark}\n{detail}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
