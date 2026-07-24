#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  echo "error: clang-format was not found in PATH" >&2
  exit 1
fi

staged_files=()
while IFS= read -r file; do
  case "$file" in
    include/*|src/*|test/test_case/*) ;;
    *) continue ;;
  esac

  case "$file" in
    test/test_case/catch2/*) continue ;;
  esac

  case "$file" in
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx|*.cu|*.cuh)
      staged_files+=("$file")
      ;;
  esac
done < <(git diff --cached --name-only --diff-filter=ACMR)

if [ "${#staged_files[@]}" -eq 0 ]; then
  exit 0
fi

clang-format -i -- "${staged_files[@]}"
git add -- "${staged_files[@]}"

