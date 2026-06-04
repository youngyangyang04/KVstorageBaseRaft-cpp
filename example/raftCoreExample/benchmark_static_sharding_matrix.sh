#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

BENCH_CASES="${BENCH_CASES:-4x20 8x30}"
BENCH_REPEATS="${BENCH_REPEATS:-1}"
RESULTS_CSV="${RESULTS_CSV:-${REPO_ROOT}/docs/performance-history.csv}"
REPORT_MD="${REPORT_MD:-${REPO_ROOT}/docs/performance-report.md}"
SUITE_PREFIX="${BENCH_SUITE_PREFIX:-static-shard-suite}"
SUITE_STAMP="$(date '+%Y%m%d-%H%M%S')"

for bench_case in ${BENCH_CASES}; do
  if [[ "${bench_case}" != *x* ]]; then
    echo "invalid BENCH_CASES entry: ${bench_case} (expected <concurrency>x<ops>)" >&2
    exit 1
  fi

  concurrency="${bench_case%x*}"
  ops_per_thread="${bench_case#*x}"
  suite_id="${SUITE_PREFIX}-${SUITE_STAMP}-${concurrency}x${ops_per_thread}"

  echo "=== Running suite ${suite_id} ==="
  BENCH_CONCURRENCY="${concurrency}" \
  BENCH_OPS_PER_THREAD="${ops_per_thread}" \
  BENCH_REPEATS="${BENCH_REPEATS}" \
  BENCH_SUITE_ID="${suite_id}" \
  RESULTS_CSV="${RESULTS_CSV}" \
  REPORT_MD="${REPORT_MD}" \
  bash "${SCRIPT_DIR}/benchmark_static_sharding.sh"
done
