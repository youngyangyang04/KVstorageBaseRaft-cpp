#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

CONFIG_PATH="${1:-${SCRIPT_DIR}/static_shard_2g.conf}"
RUN_ROOT="${2:-/tmp/kv-static-shard-2g}"
BIN_ROOT="${3:-${REPO_ROOT}/bin}"

mkdir -p "${RUN_ROOT}"
pkill -f "${BIN_ROOT}/raftCoreRun -g" || true

start_node() {
  local group_id="$1"
  local node_id="$2"
  local node_dir="${RUN_ROOT}/g${group_id}n${node_id}"
  mkdir -p "${node_dir}"
  pushd "${node_dir}" >/dev/null
  nohup "${BIN_ROOT}/raftCoreRun" -g "${group_id}" -n "${node_id}" -f "${CONFIG_PATH}" >"server.log" 2>&1 &
  echo $! >pid
  popd >/dev/null
}

for node_id in 0 1 2; do
  start_node 0 "${node_id}"
  start_node 1 "${node_id}"
done

echo "started static 2-group shard cluster under ${RUN_ROOT}"
echo "stop with: pkill -f '${BIN_ROOT}/raftCoreRun -g'"
