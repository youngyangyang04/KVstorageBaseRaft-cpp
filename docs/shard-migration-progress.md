# Shard Migration Progress

This document tracks where the horizontal-scaling work currently stands after the static-sharding phase.

## Current Status

We are no longer at "static routing only".

The project now has a first runnable online shard-migration path:

1. A config file carries `config_version`.
2. Servers poll the config and detect version changes.
3. Newly assigned shard owners mark those shards as `Pulling`.
4. The new owner pulls shard state from the previous owner over RPC.
5. The pulled shard is installed through the destination Raft group as an `InstallShard` command.
6. Clients see `ErrShardNotReady` during migration and retry until the shard becomes available.

This is already meaningfully beyond the earlier static-routing skeleton, because data can now move online between Raft groups without restarting the whole cluster.

## What Was Verified

The migration path was validated on the Linux server container with:

```bash
cd /workspace
bash example/raftCoreExample/demo_dynamic_shard_migration.sh
```

Observed behavior:

- Start with config version 1, where all shards belong to group 0.
- Write key `migration-demo-key=before-move`.
- Switch the live config to version 2, where all shards belong to group 1.
- Read succeeds on the new owner and still returns `before-move`.
- Write `after-move` on the new owner.
- Read again returns `after-move`.

That proves the current implementation supports:

- config-versioned ownership change
- online shard pull from the previous owner
- state installation on the destination group
- continued read/write correctness after migration

## What Is Done

- Static shard-aware routing for clients
- Server-side shard ownership checks
- Multi-group benchmark automation and reporting
- Config-version parsing
- `FetchShardData` RPC
- Pull-based shard migration loop
- Raft-replicated `InstallShard` command on the destination group
- Client retry support for `ErrShardNotReady`
- Demo script for live migration validation
- Fix for duplicate client id collisions across separate client processes

## What Is Not Done Yet

These are the biggest remaining gaps before the project feels closer to a production-style shardkv design:

- No dedicated shard-controller service yet
  - config changes still come from a shared config file rather than a replicated controller
- No migration garbage collection yet
  - the old owner keeps shard data for export safety and does not delete it after handoff
- No config-change barrier through Raft yet
  - ownership changes are detected locally by polling, not committed as replicated config log entries
- No cross-version fencing beyond retry behavior
  - this version is good for a first migration demo, but not yet hardened for many rapid config changes
- No migration performance benchmark yet
  - we proved correctness of online movement, but have not yet measured migration throughput or pause cost

## How To Explain The Progress

For interview discussion, the clean story is:

1. Phase 1 solved the single-group throughput bottleneck with static sharding.
2. Phase 2 introduced the first online reconfiguration mechanism.
3. The system can now change shard ownership at runtime and recover data on the new owner automatically.
4. The next milestone is to replace file-based config changes with a replicated shard controller and then add garbage collection plus migration benchmarking.

## Recommended Next Step

The highest-value next step is:

- build a lightweight `ShardController` service that owns versioned configs and exposes `Join`, `Leave`, and `Query`

After that, we should add:

- migration acknowledgment and old-owner cleanup
- migration latency / pause benchmark automation
- repeated config-change stress tests
