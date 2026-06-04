# KVstorageBaseRaft-cpp Performance Tracking Playbook

This document explains how we measure the current horizontal-scaling work, where the raw evidence is stored, and how to reproduce the numbers on the Linux server.

## What Is Automated

- `example/raftCoreExample/benchmark_static_sharding.sh`
  - Starts a `1 group x 3 nodes` baseline.
  - Starts a `2 groups x 3 nodes` static-shard topology.
  - Runs the same benchmark against both topologies.
  - Appends raw numbers into `docs/performance-history.csv`.
- `example/raftCoreExample/benchmark_static_sharding_matrix.sh`
  - Runs a configurable workload matrix such as `4x20` and `8x30`.
  - Assigns a unique suite id to every workload case.
  - Regenerates `docs/performance-report.md` after the suite finishes.
- `example/raftCoreExample/generate_performance_report.py`
  - Reads the CSV history.
  - Computes average and best QPS per topology.
  - Computes the uplift of `two-group-static-shard` over `single-group`.
  - Produces an interview-ready Markdown report.

## One-Command Workflow

Inside the Linux container:

```bash
cd /workspace
bash example/raftCoreExample/benchmark_static_sharding_matrix.sh
```

Useful overrides:

```bash
BENCH_CASES="4x20 8x30 12x40" \
BENCH_REPEATS=2 \
bash example/raftCoreExample/benchmark_static_sharding_matrix.sh
```

Outputs:

- Raw history: `docs/performance-history.csv`
- Latest summary: `docs/performance-report.md`
- Raw benchmark logs: `/tmp/kv-benchmark-suite/raw-logs/<suite-id>/`

## Current Measured Takeaways

The current phase is not full dynamic resharding yet. It proves a narrower but still valuable claim:

- The client can route requests by shard.
- The server rejects requests for shards it does not own with `ErrWrongGroup`.
- Load can be split across two independent Raft groups instead of one leader becoming the only write path.

Measured throughput uplift on the Alibaba Cloud server:

- `4 clients x 20 ops/thread`
  - first recorded run: `32.4309 -> 33.4931 QPS` (`+3.27%`)
  - automation rerun: `32.4607 -> 32.7697 QPS` (`+0.95%`)
- `8 clients x 30 ops/thread`
  - first recorded run: `32.1976 -> 47.5597 QPS` (`+47.71%`)
  - automation rerun: `32.2128 -> 46.9394 QPS` (`+45.72%`)

The second case is the more important signal. At higher concurrency, the single-group setup starts to saturate while the two-group layout keeps scaling better. The low-load case shows only a small gain, but the high-load case repeatedly shows roughly `+46%` to `+48%` uplift. That is the core interview story for this phase.

## How To Explain This In An Interview

Use this sequence:

1. Start with the bottleneck.
   - Originally all keys went through one Raft replication group, so write throughput was bounded by one leader pipeline.
2. Explain the first scaling step.
   - We introduced shard ownership metadata plus shard-aware routing, then split the keyspace across two static groups.
3. Show the evidence.
   - On the same host and the same benchmark shape, the two-group topology improved QPS by `+47.71%` at `8x30`.
4. Be honest about scope.
   - This is static sharding, not full online resharding yet.
5. Show the next step.
   - Add a shard controller, versioned configs, migration RPCs, and background shard handoff.

## Why The Automation Matters

Without automation, benchmark numbers are hard to trust and harder to reuse. This workflow gives us:

- repeatable startup and cleanup
- comparable single-group vs multi-group measurements
- persistent CSV history
- automatically regenerated Markdown summaries
- raw logs for later debugging

That makes every later optimization measurable instead of anecdotal.

## Next Gaps To Close

- Add repeated runs with average, min, and max latency.
- Record p50 and p95 latency instead of QPS only.
- Add shard-distribution checks so we can prove both groups are receiving traffic.
- Move from static shard config to dynamic configuration changes and online shard migration.
