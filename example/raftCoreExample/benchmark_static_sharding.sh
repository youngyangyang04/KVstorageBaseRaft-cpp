#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BIN_ROOT="${REPO_ROOT}/bin"

SINGLE_CONFIG="${SINGLE_CONFIG:-${REPO_ROOT}/bin/test.conf}"
DOUBLE_CONFIG="${DOUBLE_CONFIG:-${SCRIPT_DIR}/static_shard_2g.conf}"
CONCURRENCY="${BENCH_CONCURRENCY:-4}"
OPS_PER_THREAD="${BENCH_OPS_PER_THREAD:-20}"
BENCH_REPEATS="${BENCH_REPEATS:-1}"
SUITE_ID="${BENCH_SUITE_ID:-static-shard-$(date '+%Y%m%d-%H%M%S')-c${CONCURRENCY}-n${OPS_PER_THREAD}}"
RESULTS_CSV="${RESULTS_CSV:-${REPO_ROOT}/docs/performance-history.csv}"
REPORT_MD="${REPORT_MD:-${REPO_ROOT}/docs/performance-report.md}"
TMP_ROOT="${TMP_ROOT:-/tmp/kv-benchmark-suite}"
RAW_LOG_ROOT="${RAW_LOG_ROOT:-${TMP_ROOT}/raw-logs/${SUITE_ID}}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
EXPECTED_HEADER="timestamp,suite,repeat,topology,config,concurrency,ops_per_thread,total_ops,total_time_sec,qps,put_avg_ms,put_p50_ms,put_p95_ms,get_avg_ms,get_p50_ms,get_p95_ms,group_distribution,raw_log"

mkdir -p "$(dirname "${RESULTS_CSV}")"
mkdir -p "${TMP_ROOT}"
mkdir -p "${RAW_LOG_ROOT}"

upgrade_results_csv() {
  if [[ ! -f "${RESULTS_CSV}" ]]; then
    echo "${EXPECTED_HEADER}" >"${RESULTS_CSV}"
    return
  fi

  local current_header
  current_header="$(head -n 1 "${RESULTS_CSV}")"
  if [[ "${current_header}" == "${EXPECTED_HEADER}" ]]; then
    return
  fi

  if [[ "${current_header}" == "timestamp,topology,config,concurrency,ops_per_thread,total_ops,total_time_sec,qps" ]]; then
    local upgraded_csv
    upgraded_csv="$(mktemp)"
    echo "${EXPECTED_HEADER}" >"${upgraded_csv}"
    awk -F',' 'NR > 1 {
      printf "%s,legacy-import,1,%s,%s,%s,%s,%s,%s,%s,,,,,,,,\n",
        $1, $2, $3, $4, $5, $6, $7, $8
    }' "${RESULTS_CSV}" >>"${upgraded_csv}"
    mv "${upgraded_csv}" "${RESULTS_CSV}"
    return
  fi

  if [[ "${current_header}" == "timestamp,suite,repeat,topology,config,concurrency,ops_per_thread,total_ops,total_time_sec,qps,raw_log" ]]; then
    local upgraded_csv
    upgraded_csv="$(mktemp)"
    echo "${EXPECTED_HEADER}" >"${upgraded_csv}"
    awk -F',' 'NR > 1 {
      printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,,,,,,,%s,%s\n",
        $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, "", $11
    }' "${RESULTS_CSV}" >>"${upgraded_csv}"
    mv "${upgraded_csv}" "${RESULTS_CSV}"
    return
  fi

  echo "unsupported CSV header in ${RESULTS_CSV}: ${current_header}" >&2
  exit 1
}

upgrade_results_csv

if ! [[ "${BENCH_REPEATS}" =~ ^[0-9]+$ ]] || [[ "${BENCH_REPEATS}" -lt 1 ]]; then
  echo "BENCH_REPEATS must be a positive integer" >&2
  exit 1
fi

cleanup() {
  pkill -f "${BIN_ROOT}/raftCoreRun -g" || true
  pkill -f "${BIN_ROOT}/benchmark" || true
}

trap cleanup EXIT

run_case() {
  local topology="$1"
  local start_script="$2"
  local config_path="$3"
  local run_root="$4"
  local repeat_index="$5"

  cleanup
  bash "${start_script}" "${config_path}" "${run_root}" "${BIN_ROOT}" >/tmp/kv-start.log 2>&1
  sleep 12

  local output
  local raw_log
  raw_log="${RAW_LOG_ROOT}/${topology}-c${CONCURRENCY}-n${OPS_PER_THREAD}-r${repeat_index}.log"
  output="$("${BIN_ROOT}/benchmark" -f "${config_path}" -c "${CONCURRENCY}" -n "${OPS_PER_THREAD}" | tee "${raw_log}")"
  echo "${output}"

  local total_ops
  local total_time
  local qps
  local put_avg_ms
  local put_p50_ms
  local put_p95_ms
  local get_avg_ms
  local get_p50_ms
  local get_p95_ms
  local group_distribution
  total_ops="$(echo "${output}" | awk -F': ' '/Total operations/ {print $2}')"
  total_time="$(echo "${output}" | awk -F': ' '/Total time/ {gsub(/ seconds/, "", $2); print $2}')"
  qps="$(echo "${output}" | awk -F': ' '/QPS/ {print $2}')"
  put_avg_ms="$(echo "${output}" | awk -F': ' '/Put avg latency ms/ {print $2}')"
  put_p50_ms="$(echo "${output}" | awk -F': ' '/Put p50 latency ms/ {print $2}')"
  put_p95_ms="$(echo "${output}" | awk -F': ' '/Put p95 latency ms/ {print $2}')"
  get_avg_ms="$(echo "${output}" | awk -F': ' '/Get avg latency ms/ {print $2}')"
  get_p50_ms="$(echo "${output}" | awk -F': ' '/Get p50 latency ms/ {print $2}')"
  get_p95_ms="$(echo "${output}" | awk -F': ' '/Get p95 latency ms/ {print $2}')"
  group_distribution="$(echo "${output}" | awk -F': ' '/Target group distribution/ {print $2}')"

  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$(date '+%Y-%m-%d %H:%M:%S')" \
    "${SUITE_ID}" \
    "${repeat_index}" \
    "${topology}" \
    "${config_path}" \
    "${CONCURRENCY}" \
    "${OPS_PER_THREAD}" \
    "${total_ops}" \
    "${total_time}" \
    "${qps}" \
    "${put_avg_ms}" \
    "${put_p50_ms}" \
    "${put_p95_ms}" \
    "${get_avg_ms}" \
    "${get_p50_ms}" \
    "${get_p95_ms}" \
    "${group_distribution}" \
    "${raw_log}" >>"${RESULTS_CSV}"
}

for ((repeat_index = 1; repeat_index <= BENCH_REPEATS; ++repeat_index)); do
  run_case "single-group" "${SCRIPT_DIR}/run_single_group_compat.sh" "${SINGLE_CONFIG}" \
    "${TMP_ROOT}/single-r${repeat_index}" "${repeat_index}"
  run_case "two-group-static-shard" "${SCRIPT_DIR}/run_static_shard_2g.sh" "${DOUBLE_CONFIG}" \
    "${TMP_ROOT}/double-r${repeat_index}" "${repeat_index}"
done

if command -v "${PYTHON_BIN}" >/dev/null 2>&1; then
  "${PYTHON_BIN}" "${SCRIPT_DIR}/generate_performance_report.py" \
    --csv "${RESULTS_CSV}" \
    --output "${REPORT_MD}" \
    --latest-suite "${SUITE_ID}"
else
  echo "warning: ${PYTHON_BIN} not found, skipped Markdown report generation" >&2
fi
