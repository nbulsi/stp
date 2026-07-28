#!/usr/bin/env python3
"""Verify generated ABC BENCH fixtures against their embedded lutsim truth tables."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys
import time


TEST_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TEST_DIR.parent
DEFAULT_STP = PROJECT_DIR / "build" / "bin" / "stp"
GENERATED_BENCHMARK_DIR = TEST_DIR / "benchmarks" / "vars"
ACTUAL_PATTERN = re.compile(r"\b0x[0-9A-F]+")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stp", type=Path, default=DEFAULT_STP, help="path to the stp executable")
    parser.add_argument("--timeout", type=float, default=120.0, help="maximum seconds per BENCH file")
    parser.add_argument("--pattern", default="case_*.bench", help="BENCH filename glob within each *var directory")
    return parser.parse_args()


def alice_quote(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def expected_truth_table(benchmark: Path) -> str:
    expected_path = benchmark.with_suffix(".expected.hex")
    if not expected_path.is_file():
        raise ValueError(f"missing expected truth-table file: {expected_path.name}")
    expected = expected_path.read_text(encoding="ascii").strip()
    if not re.fullmatch(r"0x[0-9A-F]+", expected):
        raise ValueError(f"invalid expected truth table: {expected_path.name}")
    return expected


def verify_one(stp: Path, benchmark: Path, timeout: float) -> tuple[bool, str]:
    try:
        expected = expected_truth_table(benchmark)
    except ValueError as error:
        return False, str(error)

    try:
        result = subprocess.run(
            [str(stp), "-c", f"lutsim {alice_quote(str(benchmark))}"],
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return False, f"timed out after {timeout:g} seconds"

    output = result.stdout + result.stderr
    if result.returncode != 0:
        return False, f"stp exited with status {result.returncode}"
    actual_matches = ACTUAL_PATTERN.findall(output)
    if not actual_matches:
        return False, "lutsim did not print a hexadecimal truth table"
    actual = actual_matches[-1]
    if actual != expected:
        return False, f"expected {expected}, got {actual}"
    return True, ""


def main() -> int:
    args = parse_arguments()
    stp = args.stp.resolve()
    if not stp.is_file():
        print(f"error: stp executable not found: {stp}", file=sys.stderr)
        return 2

    benchmarks = sorted(
        path for directory in GENERATED_BENCHMARK_DIR.glob("*var") for path in directory.glob(args.pattern)
    )
    if not benchmarks:
        print(f"error: no generated BENCH files found below {GENERATED_BENCHMARK_DIR}", file=sys.stderr)
        return 2

    failures: list[tuple[Path, str]] = []
    start = time.monotonic()
    for index, benchmark in enumerate(benchmarks, start=1):
        passed, detail = verify_one(stp, benchmark, args.timeout)
        status = "PASS" if passed else "FAIL"
        print(f"[{index:2}/{len(benchmarks):2}] {status} {benchmark.relative_to(PROJECT_DIR)}")
        if not passed:
            failures.append((benchmark, detail))

    print(f"\n{len(benchmarks) - len(failures)}/{len(benchmarks)} files passed in {time.monotonic() - start:.2f}s")
    if not failures:
        return 0
    for benchmark, detail in failures:
        print(f"FAIL {benchmark.relative_to(PROJECT_DIR)}: {detail}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
