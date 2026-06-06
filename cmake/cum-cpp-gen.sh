#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <input.cum> <output.hpp>" >&2
  exit 2
fi

if [[ -z "${CUM_ROOT:-}" ]]; then
  echo "CUM_ROOT must be set to the cum repository root" >&2
  exit 2
fi

input="$1"
output="$2"
gen="${CUM_ROOT}/generator"

export PYTHONPATH="${gen}${PYTHONPATH:+:${PYTHONPATH}}"

python3 "${gen}/cum_to_ast.py" "${input}" | python3 "${gen}/ast_to_cpp.py" > "${output}"
