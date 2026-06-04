#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "clerk.h"

namespace {
void ShowArgsHelp() {
  std::cout << "format: benchmark [-f configFile] [-c concurrency] [-n opsPerThread]" << std::endl;
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

  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> workers;
  std::atomic<int> totalGets{0};
  std::atomic<int> totalPuts{0};

  for (int tid = 0; tid < concurrency; ++tid) {
    workers.emplace_back([&, tid]() {
      Clerk client;
      client.Init(configFileName);

      for (int i = 0; i < opsPerThread; ++i) {
        const std::string key = "bench_t" + std::to_string(tid) + "_k" + std::to_string(i);
        const std::string value = "value_" + std::to_string(i);
        client.Put(key, value);
        totalPuts.fetch_add(1, std::memory_order_relaxed);

        const std::string readValue = client.Get(key);
        if (readValue != value) {
          std::cerr << "verification failed for key=" << key << ", expected=" << value << ", got=" << readValue
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
        totalGets.fetch_add(1, std::memory_order_relaxed);
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

  std::cout << "Benchmark finished." << std::endl;
  std::cout << "Total puts: " << totalPuts.load(std::memory_order_relaxed) << std::endl;
  std::cout << "Total gets: " << totalGets.load(std::memory_order_relaxed) << std::endl;
  std::cout << "Total operations: " << totalOps << std::endl;
  std::cout << "Total time: " << elapsed.count() << " seconds" << std::endl;
  std::cout << "QPS: " << qps << std::endl;

  return 0;
}
