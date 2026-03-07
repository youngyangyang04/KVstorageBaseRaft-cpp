#include <iostream>
#include <chrono>
#include <vector>
#include "clerk.h"
#include "util.h"

int main() {
    Clerk client;
    client.Init("test.conf");

    int count = 500;
    std::cout << "Starting LOCAL benchmark: 500 Put + 500 Get operations..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        client.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    for (int i = 0; i < count; ++i) {
        client.Get("key_" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    double total_ops = count * 2;
    double qps = total_ops / diff.count();

    std::cout << "Benchmark finished." << std::endl;
    std::cout << "Total operations: " << total_ops << std::endl;
    std::cout << "Total time: " << diff.count() << " seconds" << std::endl;
    std::cout << "QPS: " << qps << std::endl;

    return 0;
}
