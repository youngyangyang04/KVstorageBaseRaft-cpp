#!/usr/bin/env python3
import argparse
import csv
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from statistics import mean
from typing import Dict, List, Optional


CSV_COLUMNS = [
    "timestamp",
    "suite",
    "repeat",
    "topology",
    "config",
    "concurrency",
    "ops_per_thread",
    "total_ops",
    "total_time_sec",
    "qps",
    "raw_log",
]


@dataclass
class Row:
    timestamp: str
    suite: str
    repeat: int
    topology: str
    config: str
    concurrency: int
    ops_per_thread: int
    total_ops: int
    total_time_sec: float
    qps: float
    raw_log: str

    @property
    def parsed_time(self) -> datetime:
        return datetime.strptime(self.timestamp, "%Y-%m-%d %H:%M:%S")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a Markdown performance report from benchmark history.")
    parser.add_argument("--csv", required=True, help="Path to docs/performance-history.csv")
    parser.add_argument("--output", required=True, help="Path to generated Markdown report")
    parser.add_argument("--latest-suite", default="", help="Suite id to highlight at the top of the report")
    return parser.parse_args()


def load_rows(csv_path: Path) -> List[Row]:
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != CSV_COLUMNS:
            raise ValueError(f"unexpected CSV header in {csv_path}: {reader.fieldnames}")

        rows: List[Row] = []
        for raw in reader:
            if not raw["timestamp"]:
                continue
            rows.append(
                Row(
                    timestamp=raw["timestamp"],
                    suite=raw["suite"] or "unspecified-suite",
                    repeat=int(raw["repeat"] or "1"),
                    topology=raw["topology"],
                    config=raw["config"],
                    concurrency=int(raw["concurrency"]),
                    ops_per_thread=int(raw["ops_per_thread"]),
                    total_ops=int(float(raw["total_ops"])),
                    total_time_sec=float(raw["total_time_sec"]),
                    qps=float(raw["qps"]),
                    raw_log=raw["raw_log"],
                )
            )
        return rows


def summarize_topology(rows: List[Row]) -> Dict[str, object]:
    return {
        "repeats": len(rows),
        "avg_qps": mean(row.qps for row in rows),
        "best_qps": max(row.qps for row in rows),
        "avg_time_sec": mean(row.total_time_sec for row in rows),
        "config": rows[0].config,
        "raw_logs": [row.raw_log for row in rows if row.raw_log],
    }


def summarize_suite(rows: List[Row]) -> Dict[str, object]:
    rows = sorted(rows, key=lambda row: row.parsed_time)
    by_topology: Dict[str, List[Row]] = defaultdict(list)
    for row in rows:
        by_topology[row.topology].append(row)

    topology_summary = {name: summarize_topology(group) for name, group in by_topology.items()}
    single = topology_summary.get("single-group")
    multi = topology_summary.get("two-group-static-shard")
    uplift = None
    if single and multi and single["avg_qps"] > 0:
        uplift = ((multi["avg_qps"] - single["avg_qps"]) / single["avg_qps"]) * 100.0

    return {
        "suite": rows[0].suite,
        "load": f"{rows[0].concurrency}x{rows[0].ops_per_thread}",
        "concurrency": rows[0].concurrency,
        "ops_per_thread": rows[0].ops_per_thread,
        "first_timestamp": rows[0].timestamp,
        "last_timestamp": rows[-1].timestamp,
        "topology_summary": topology_summary,
        "uplift_pct": uplift,
    }


def fmt_number(value: Optional[float], digits: int = 2) -> str:
    if value is None:
        return "n/a"
    return f"{value:.{digits}f}"


def build_latest_section(summary: Dict[str, object]) -> List[str]:
    lines = [
        "## Latest Suite",
        "",
        f"- Suite: `{summary['suite']}`",
        f"- Load: `{summary['concurrency']} clients x {summary['ops_per_thread']} ops/thread`",
        f"- Measured uplift: `{fmt_number(summary['uplift_pct'])}%`",
        "",
        "| Topology | Repeats | Avg QPS | Best QPS | Avg total time (s) | Config |",
        "| --- | ---: | ---: | ---: | ---: | --- |",
    ]

    topology_summary = summary["topology_summary"]
    for topology in ("single-group", "two-group-static-shard"):
        stats = topology_summary.get(topology)
        if not stats:
            continue
        lines.append(
            f"| {topology} | {stats['repeats']} | {fmt_number(stats['avg_qps'], 4)} | "
            f"{fmt_number(stats['best_qps'], 4)} | {fmt_number(stats['avg_time_sec'], 4)} | `{stats['config']}` |"
        )

    lines.extend([""])
    return lines


def build_history_table(summaries: List[Dict[str, object]]) -> List[str]:
    lines = [
        "## Historical Suites",
        "",
        "| Suite | Load | Single-group avg QPS | Two-group avg QPS | Uplift | Last timestamp |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]

    for summary in summaries:
        topology_summary = summary["topology_summary"]
        single = topology_summary.get("single-group")
        multi = topology_summary.get("two-group-static-shard")
        lines.append(
            f"| `{summary['suite']}` | `{summary['load']}` | "
            f"{fmt_number(single['avg_qps'], 4) if single else 'n/a'} | "
            f"{fmt_number(multi['avg_qps'], 4) if multi else 'n/a'} | "
            f"{fmt_number(summary['uplift_pct'])}% | {summary['last_timestamp']} |"
        )

    lines.extend([""])
    return lines


def build_interpretation(summaries: List[Dict[str, object]]) -> List[str]:
    lines = [
        "## Interpretation",
        "",
        "- This phase validates static shard-aware routing, not dynamic shard migration.",
        "- The important comparison is same host, same benchmark shape, different topology width.",
    ]

    by_load: Dict[str, List[float]] = defaultdict(list)
    for summary in summaries:
        uplift = summary["uplift_pct"]
        if uplift is not None:
            by_load[summary["load"]].append(uplift)

    for load in sorted(by_load.keys()):
        uplifts = by_load[load]
        if len(uplifts) == 1:
            lines.append(f"- At `{load}`, the two-group topology improved throughput by `{fmt_number(uplifts[0])}%`.")
            continue

        lines.append(
            f"- Across `{len(uplifts)}` suite(s) at `{load}`, uplift ranged from "
            f"`{fmt_number(min(uplifts))}%` to `{fmt_number(max(uplifts))}%`, "
            f"with an average of `{fmt_number(mean(uplifts))}%`."
        )

    lines.extend(
        [
            "- Larger uplift at higher concurrency is a sign that the original single-group path is the bottleneck.",
            "",
        ]
    )
    return lines


def build_repro_section() -> List[str]:
    return [
        "## Reproduce",
        "",
        "```bash",
        "cd /workspace",
        'BENCH_CASES="4x20 8x30" BENCH_REPEATS=1 bash example/raftCoreExample/benchmark_static_sharding_matrix.sh',
        "```",
        "",
        "The raw data source is `docs/performance-history.csv`.",
        "",
    ]


def generate_report(rows: List[Row], latest_suite: str) -> str:
    suites: Dict[str, List[Row]] = defaultdict(list)
    for row in rows:
        suites[row.suite].append(row)

    summaries = [summarize_suite(group) for group in suites.values()]
    summaries.sort(key=lambda item: item["last_timestamp"], reverse=True)

    highlighted = None
    if latest_suite:
        for summary in summaries:
            if summary["suite"] == latest_suite:
                highlighted = summary
                break
    if highlighted is None and summaries:
        highlighted = summaries[0]

    lines = [
        "# KVstorageBaseRaft-cpp Performance Report",
        "",
        "This file is auto-generated by `example/raftCoreExample/generate_performance_report.py`.",
        "",
    ]

    if highlighted is not None:
        lines.extend(build_latest_section(highlighted))

    lines.extend(build_history_table(summaries))
    lines.extend(build_interpretation(summaries))
    lines.extend(build_repro_section())
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    csv_path = Path(args.csv)
    output_path = Path(args.output)
    rows = load_rows(csv_path)
    report = generate_report(rows, args.latest_suite)
    output_path.write_text(report, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
