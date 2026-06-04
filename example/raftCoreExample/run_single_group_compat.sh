#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

CONFIG_PATH="${1:-${REPO_ROOT}/bin/test.conf}"
RUN_ROOT="${2:-/tmp/kv-single-group}"
BIN_ROOT="${3:-${REPO_ROOT}/bin}"

mkdir -p "${RUN_ROOT}"
pkill -f "${BIN_ROOT}/raftCoreRun -g" || true

start_node() {
  local node_id="$1"
  local node_dir="${RUN_ROOT}/n${node_id}"
  mkdir -p "${node_dir}"
  pushd "${node_dir}" >/dev/null
  nohup "${BIN_ROOT}/raftCoreRun" -g 0 -n "${node_id}" -f "${CONFIG_PATH}" >"server.log" 2>&1 &
  echo $! >pid
  popd >/dev/null
}

for node_id in 0 1 2; do
  start_node "${node_id}"
done

echo "started single-group compatibility cluster under ${RUN_ROOT}"
echo "stop with: pkill -f '${BIN_ROOT}/raftCoreRun -g'"
