#include "kvstore.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>

void populateDatabase(
    KVStore &db,
    int count,
    const std::string &prefix)
{
    for (int i = 0; i < count; i++)
    {
        db.set(
            prefix + std::to_string(i),
            "value_" + std::to_string(i));
    }
}

void benchmarkBasicOperations()
{

    const int operations = 100000;

    std::cout << "\n=== Basic Operations ===\n";

    // SET
    {
        KVStore db(false);

        auto start =
            std::chrono::high_resolution_clock::now();

        for (int i = 0; i < operations; i++)
        {
            db.set(
                "key_" + std::to_string(i),
                "value_" + std::to_string(i));
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start)
                .count();

        std::cout << "SET:    "
                  << operations / seconds
                  << " ops/sec\n";
    }

    // GET
    {
        KVStore db(false);

        populateDatabase(
            db,
            operations,
            "key_");

        auto start =
            std::chrono::high_resolution_clock::now();

        for (int i = 0; i < operations; i++)
        {
            db.get(
                "key_" + std::to_string(i));
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start)
                .count();

        std::cout << "GET:    "
                  << operations / seconds
                  << " ops/sec\n";
    }

    // DELETE
    {
        KVStore db(false);

        populateDatabase(
            db,
            operations,
            "key_");

        auto start =
            std::chrono::high_resolution_clock::now();

        for (int i = 0; i < operations; i++)
        {
            db.remove(
                "key_" + std::to_string(i));
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start)
                .count();

        std::cout << "DELETE: "
                  << operations / seconds
                  << " ops/sec\n";
    }

    // RENAME
    {
        KVStore db(false);

        populateDatabase(
            db,
            operations,
            "old_key_");

        auto start =
            std::chrono::high_resolution_clock::now();

        for (int i = 0; i < operations; i++)
        {
            db.rename(
                "old_key_" + std::to_string(i),
                "new_key_" + std::to_string(i));
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start)
                .count();

        std::cout << "RENAME: "
                  << operations / seconds
                  << " ops/sec\n";
    }
}

void benchmarkMixedWorkload()
{

    const int operations = 100000;

    KVStore db(false);

    populateDatabase(
        db,
        operations,
        "key_");

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> operationDist(1, 100);

    int gets = 0;
    int sets = 0;
    int deletes = 0;
    int renames = 0;

    auto start =
        std::chrono::high_resolution_clock::now();

    for (int i = 0; i < operations; i++)
    {

        int operation = operationDist(rng);

        if (operation <= 70)
        {

            db.get(
                "key_" + std::to_string(i));

            gets++;
        }
        else if (operation <= 90)
        {

            db.set(
                "key_" + std::to_string(i),
                "updated_" + std::to_string(i));

            sets++;
        }
        else if (operation <= 95)
        {

            db.remove(
                "key_" + std::to_string(i));

            deletes++;
        }
        else
        {

            db.rename(
                "key_" + std::to_string(i),
                "renamed_" + std::to_string(i));

            renames++;
        }
    }

    auto end =
        std::chrono::high_resolution_clock::now();

    double seconds =
        std::chrono::duration<double>(
            end - start)
            .count();

    double throughput =
        operations / seconds;

    std::cout << "\n=== Mixed Workload ===\n";

    std::cout << "Operations: "
              << operations << '\n';

    std::cout << "GET:    "
              << gets << '\n';

    std::cout << "SET:    "
              << sets << '\n';

    std::cout << "DELETE: "
              << deletes << '\n';

    std::cout << "RENAME: "
              << renames << '\n';

    std::cout << "Throughput: "
              << throughput
              << " ops/sec\n";
}

void benchmarkConcurrentReads()
{

    const int operationsPerThread = 100000;

    std::cout << "\n=== Concurrent GET ===\n";

    for (int threadCount : {1, 2, 4, 8})
    {

        KVStore db(false);

        int totalOperations =
            operationsPerThread * threadCount;

        populateDatabase(
            db,
            totalOperations,
            "key_");

        std::vector<std::thread> threads;

        auto start =
            std::chrono::high_resolution_clock::now();

        for (int t = 0; t < threadCount; t++)
        {

            threads.emplace_back([&, t]()
                                 {

                int startIndex =
                    t * operationsPerThread;

                int endIndex =
                    startIndex + operationsPerThread;

                for (int i = startIndex; i < endIndex; i++) {

                    db.get(
                        "key_" + std::to_string(i)
                    );
                } });
        }

        for (auto &thread : threads)
        {
            thread.join();
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start)
                .count();

        double throughput =
            totalOperations / seconds;

        std::cout
            << threadCount
            << " threads: "
            << throughput
            << " ops/sec\n";
    }
}

void benchmarkConcurrentMixed()
{

    const int threadCount = 4;
    const int operationsPerThread = 100000;

    const int totalOperations =
        threadCount * operationsPerThread;

    KVStore db(false);

    populateDatabase(
        db,
        totalOperations,
        "key_");

    std::vector<std::thread> threads;

    auto start =
        std::chrono::high_resolution_clock::now();

    for (int t = 0; t < threadCount; t++)
    {

        threads.emplace_back([&, t]()
                             {

            std::mt19937 rng(42 + t);

            std::uniform_int_distribution<int>
                operationDist(1, 100);

            int startIndex =
                t * operationsPerThread;

            int endIndex =
                startIndex + operationsPerThread;

            for (int i = startIndex; i < endIndex; i++) {

                int operation =
                    operationDist(rng);

                if (operation <= 70) {

                    // GET
                    db.get(
                        "key_" + std::to_string(i)
                    );
                }
                else if (operation <= 90) {

                    // SET
                    db.set(
                        "key_" + std::to_string(i),
                        "updated_" + std::to_string(i)
                    );
                }
                else if (operation <= 95) {

                    // DELETE
                    db.remove(
                        "key_" + std::to_string(i)
                    );
                }
                else {

                    // RENAME
                    db.rename(
                        "key_" + std::to_string(i),
                        "renamed_" + std::to_string(i)
                    );
                }
            } });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    auto end =
        std::chrono::high_resolution_clock::now();

    double seconds =
        std::chrono::duration<double>(
            end - start)
            .count();

    double throughput =
        totalOperations / seconds;

    std::cout
        << "\n=== Concurrent Mixed Workload ===\n";

    std::cout
        << "Threads: "
        << threadCount << '\n';

    std::cout
        << "Operations: "
        << totalOperations << '\n';

    std::cout
        << "Throughput: "
        << throughput
        << " ops/sec\n";
}


void benchmarkLatency() {

    const int operations = 100000;

    KVStore db(false);

    populateDatabase(
        db,
        operations,
        "key_"
    );

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> operationDist(1, 100);

    std::vector<long long> latencies;
    latencies.reserve(operations);

    int gets = 0;
    int sets = 0;
    int deletes = 0;
    int renames = 0;

    auto totalStart =
        std::chrono::high_resolution_clock::now();

    for (int i = 0; i < operations; i++) {

        int operation = operationDist(rng);

        auto start =
            std::chrono::high_resolution_clock::now();

        if (operation <= 70) {

            db.get(
                "key_" + std::to_string(i)
            );

            gets++;
        }
        else if (operation <= 90) {

            db.set(
                "key_" + std::to_string(i),
                "updated_" + std::to_string(i)
            );

            sets++;
        }
        else if (operation <= 95) {

            db.remove(
                "key_" + std::to_string(i)
            );

            deletes++;
        }
        else {

            db.rename(
                "key_" + std::to_string(i),
                "renamed_" + std::to_string(i)
            );

            renames++;
        }

        auto end =
            std::chrono::high_resolution_clock::now();

        latencies.push_back(
            std::chrono::duration_cast<
                std::chrono::nanoseconds
            >(end - start).count()
        );
    }

    auto totalEnd =
        std::chrono::high_resolution_clock::now();

    double totalSeconds =
        std::chrono::duration<double>(
            totalEnd - totalStart
        ).count();

    double throughput =
        operations / totalSeconds;

    std::sort(
        latencies.begin(),
        latencies.end()
    );

    long long totalLatency = 0;

    for (long long latency : latencies) {
        totalLatency += latency;
    }

    double averageLatency =
        static_cast<double>(totalLatency) /
        operations;

    auto percentile =
        [&](double p) {

            size_t index =
                static_cast<size_t>(
                    p * (latencies.size() - 1)
                );

            return latencies[index];
        };

    std::cout
        << "\n=== Latency ===\n";

    std::cout
        << "Operations: "
        << operations << '\n';

    std::cout
        << "Throughput: "
        << throughput
        << " ops/sec\n";

    std::cout
        << "Average: "
        << averageLatency
        << " ns\n";

    std::cout
        << "P50: "
        << percentile(0.50)
        << " ns\n";

    std::cout
        << "P95: "
        << percentile(0.95)
        << " ns\n";

    std::cout
        << "P99: "
        << percentile(0.99)
        << " ns\n";

    std::cout
        << "Max: "
        << latencies.back()
        << " ns\n";
}

void printThroughput(double throughput) {
    std::cout
        << std::fixed
        << std::setprecision(0)
        << throughput
        << " ops/sec\n";
}

int main() {

    benchmarkBasicOperations();
    benchmarkMixedWorkload();
    benchmarkConcurrentReads();
    benchmarkConcurrentMixed();
    benchmarkLatency();

    return 0;
}