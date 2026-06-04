#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "clerk.h"
#include "mprpcconfig.h"
#include "shardConfig.h"

namespace {
void ShowArgsHelp() {
  std::cout << "format: benchmark [-f configFile] [-c concurrency] [-n opsPerThread]" << std::endl;
}

struct LatencySummary {
  double avgMs = 0.0;
  double p50Ms = 0.0;
  double p95Ms = 0.0;
  double maxMs = 0.0;
};

LatencySummary SummarizeLatencies(std::vector<double> samples) {
  LatencySummary summary;
  if (samples.empty()) {
    return summary;
  }

  std::sort(samples.begin(), samples.end());
  summary.avgMs = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());

  auto percentile = [&](double ratio) {
    const size_t index = static_cast<size_t>(ratio * static_cast<double>(samples.size() - 1));
    return samples[index];
  };

  summary.p50Ms = percentile(0.50);
  summary.p95Ms = percentile(0.95);
  summary.maxMs = samples.back();
  return summary;
}

std::string FormatGroupDistribution(const std::unordered_map<int, int>& opCountByGroup) {
  if (opCountByGroup.empty()) {
    return "none";
  }

  std::map<int, int> orderedCounts(opCountByGroup.begin(), opCountByGroup.end());
  std::ostringstream oss;
  bool first = true;
  for (const auto& entry : orderedCounts) {
    if (!first) {
      oss << ';';
    }
    first = false;
    oss << "g" << entry.first << '=' << entry.second;
  }
  return oss.str();
}
}  // namespace

int main(int argc, char** argv) {
  std::string configFileName = "test.conf";
  int concurrency = 4;
  int opsPerThread = 200;

  int c = 0;
  while ((c = getopt(argc, argv, "f:c:n:h")) != -1) {
    switch (c) {
      case 'f':
        configFileName = optarg;
        break;
      case 'c':
        concurrency = std::max(1, atoi(optarg));
        break;
      case 'n':
        opsPerThread = std::max(1, atoi(optarg));
        break;
      case 'h':
      default:
        ShowArgsHelp();
        return 0;
    }
  }

  std::cout << "Starting shard-aware benchmark with concurrency=" << concurrency
            << ", opsPerThread=" << opsPerThread << ", config=" << configFileName << std::endl;

  MprpcConfig config;
  config.LoadConfigFile(configFileName.c_str());
  const StaticGroupTopology topology = LoadStaticGroupTopology(config);

  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> workers;
  std::atomic<int> totalGets{0};
  std::atomic<int> totalPuts{0};
  std::mutex statsMutex;
  std::vector<double> putLatenciesMs;
  std::vector<double> getLatenciesMs;
  std::unordered_map<int, int> opCountByGroup;
  putLatenciesMs.reserve(static_cast<size_t>(concurrency * opsPerThread));
  getLatenciesMs.reserve(static_cast<size_t>(concurrency * opsPerThread));

  for (int tid = 0; tid < concurrency; ++tid) {
    workers.emplace_back([&, tid, topology]() {
      Clerk client;
      client.Init(configFileName);
      std::vector<double> localPutLatenciesMs;
      std::vector<double> localGetLatenciesMs;
      std::unordered_map<int, int> localOpCountByGroup;
      localPutLatenciesMs.reserve(static_cast<size_t>(opsPerThread));
      localGetLatenciesMs.reserve(static_cast<size_t>(opsPerThread));

      for (int i = 0; i < opsPerThread; ++i) {
        const std::string key = "bench_t" + std::to_string(tid) + "_k" + std::to_string(i);
        const std::string value = "value_" + std::to_string(i);

        const int shardId = KeyToShard(key, topology.shardCount);
        int groupId = 0;
        if (shardId >= 0 && shardId < static_cast<int>(topology.shardOwners.size())) {
          groupId = topology.shardOwners[shardId];
        }

        const auto putStart = std::chrono::high_resolution_clock::now();
        client.Put(key, value);
        const std::chrono::duration<double, std::milli> putElapsed =
            std::chrono::high_resolution_clock::now() - putStart;
        localPutLatenciesMs.push_back(putElapsed.count());
        localOpCountByGroup[groupId] += 1;
        totalPuts.fetch_add(1, std::memory_order_relaxed);

        const auto getStart = std::chrono::high_resolution_clock::now();
        const std::string readValue = client.Get(key);
        const std::chrono::duration<double, std::milli> getElapsed =
            std::chrono::high_resolution_clock::now() - getStart;
        if (readValue != value) {
          std::cerr << "verification failed for key=" << key << ", expected=" << value << ", got=" << readValue
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
        localGetLatenciesMs.push_back(getElapsed.count());
        localOpCountByGroup[groupId] += 1;
        totalGets.fetch_add(1, std::memory_order_relaxed);
      }

      std::lock_guard<std::mutex> lock(statsMutex);
      putLatenciesMs.insert(putLatenciesMs.end(), localPutLatenciesMs.begin(), localPutLatenciesMs.end());
      getLatenciesMs.insert(getLatenciesMs.end(), localGetLatenciesMs.begin(), localGetLatenciesMs.end());
      for (const auto& entry : localOpCountByGroup) {
        opCountByGroup[entry.first] += entry.second;
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> elapsed = end - start;
  const double totalOps = totalGets.load(std::memory_order_relaxed) + totalPuts.load(std::memory_order_relaxed);
  const double qps = totalOps / elapsed.count();
  const LatencySummary putSummary = SummarizeLatencies(putLatenciesMs);
  const LatencySummary getSummary = SummarizeLatencies(getLatenciesMs);

  std::cout << "Benchmark finished." << std::endl;
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Total puts: " << totalPuts.load(std::memory_order_relaxed) << std::endl;
  std::cout << "Total gets: " << totalGets.load(std::memory_order_relaxed) << std::endl;
  std::cout << "Total operations: " << totalOps << std::endl;
  std::cout << "Total time: " << elapsed.count() << " seconds" << std::endl;
  std::cout << "QPS: " << qps << std::endl;
  std::cout << "Put avg latency ms: " << putSummary.avgMs << std::endl;
  std::cout << "Put p50 latency ms: " << putSummary.p50Ms << std::endl;
  std::cout << "Put p95 latency ms: " << putSummary.p95Ms << std::endl;
  std::cout << "Put max latency ms: " << putSummary.maxMs << std::endl;
  std::cout << "Get avg latency ms: " << getSummary.avgMs << std::endl;
  std::cout << "Get p50 latency ms: " << getSummary.p50Ms << std::endl;
  std::cout << "Get p95 latency ms: " << getSummary.p95Ms << std::endl;
  std::cout << "Get max latency ms: " << getSummary.maxMs << std::endl;
  std::cout << "Target group distribution: " << FormatGroupDistribution(opCountByGroup) << std::endl;

  return 0;
}
