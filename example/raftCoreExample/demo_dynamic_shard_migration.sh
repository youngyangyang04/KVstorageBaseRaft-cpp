#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BIN_ROOT="${REPO_ROOT}/bin"

LIVE_CONFIG="${LIVE_CONFIG:-${SCRIPT_DIR}/dynamic_migration_live.conf}"
V1_CONFIG="${V1_CONFIG:-${SCRIPT_DIR}/dynamic_migration_v1.conf}"
V2_CONFIG="${V2_CONFIG:-${SCRIPT_DIR}/dynamic_migration_v2.conf}"
RUN_ROOT="${RUN_ROOT:-/tmp/kv-dynamic-migration-demo}"
KEY="${KEY:-migration-demo-key}"
VALUE_BEFORE="${VALUE_BEFORE:-before-move}"
VALUE_AFTER="${VALUE_AFTER:-after-move}"

cleanup() {
  pkill -f "${BIN_ROOT}/raftCoreRun -g" || true
  pkill -f "${BIN_ROOT}/migrationClient" || true
}

trap cleanup EXIT

cleanup
cp "${V1_CONFIG}" "${LIVE_CONFIG}"

echo "=== Starting cluster with config version 1 ==="
bash "${SCRIPT_DIR}/run_static_shard_2g.sh" "${LIVE_CONFIG}" "${RUN_ROOT}" "${BIN_ROOT}"
sleep 12

echo "=== Writing initial value on version 1 ==="
"${BIN_ROOT}/migrationClient" -f "${LIVE_CONFIG}" -o put -k "${KEY}" -v "${VALUE_BEFORE}"
"${BIN_ROOT}/migrationClient" -f "${LIVE_CONFIG}" -o get -k "${KEY}"

echo "=== Switching to config version 2 and waiting for live migration ==="
cp "${V2_CONFIG}" "${LIVE_CONFIG}"
sleep 2

"${BIN_ROOT}/migrationClient" -f "${LIVE_CONFIG}" -o get -k "${KEY}"

echo "=== Verifying post-migration write/read on the new owner ==="
"${BIN_ROOT}/migrationClient" -f "${LIVE_CONFIG}" -o put -k "${KEY}" -v "${VALUE_AFTER}"
"${BIN_ROOT}/migrationClient" -f "${LIVE_CONFIG}" -o get -k "${KEY}"

echo "Dynamic shard migration demo finished successfully."
