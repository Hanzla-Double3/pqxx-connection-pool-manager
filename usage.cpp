#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include "ConnectionPoolManager.hpp"



using namespace std::chrono_literals;
using namespace std::chrono;

// g++ -std=c++17 usage.cpp -lpqxx -lpq
// this file is AI generated, had to do some modification
// not the ConnectionPoolManager.hpp and ConnectionPool.hpp

// Usage Example
void demonstrate_typical_usage() {
    const char* conn_str = "dbname=test user=postgres host=localhost port=5432 password=GenshinImpactIsForG*ys";
    std::vector<Statement> prepared_stmts = {
        {"create_table", "CREATE TABLE IF NOT EXISTS test (id SERIAL PRIMARY KEY, data TEXT)"},
        {"insert_data", "INSERT INTO test (data) VALUES ($1)"},
        {"select_data", "SELECT * FROM test WHERE data = $1"}
    };

    // Register connection pool
    ConnectionPoolManager::instance().registerPool(
        conn_str, 
        "main_db",
        5,      // Initial connections
        2s,     // Timeout
        prepared_stmts
    );

    auto& pool = ConnectionPoolManager::instance().get_pool("main_db");

    // Create table
    {
        auto conn = pool.get_connection();
        pqxx::work txn(*conn);
        txn.exec_prepared("create_table");
        txn.commit();
    }

    // Insert data
    for(int i = 0; i < 10; ++i) {
        auto conn = pool.get_connection();
        pqxx::work txn(*conn);
        txn.exec_prepared("insert_data", "test_data_" + std::to_string(i));
        txn.commit();
    }

    // Query data
    {
        auto conn = pool.get_connection();
        pqxx::work txn(*conn);
        auto result = txn.exec_prepared("select_data", "test_data_5");
        std::cout << "Found " << result.size() << " records\n";
        txn.commit();
    }
}

// Basic Test
void test_connection_acquisition() {
    ConnectionPool pool("dbname=test user=postgres host=localhost port=5432 password=03058246191AaBbCcDd55@#%_postgres", 3, 1s, {});
    
    auto c1 = pool.get_connection();
    auto c2 = pool.get_connection();
    auto c3 = pool.get_connection();
    
    try {
        auto c4 = pool.get_connection();
        std::cerr << "Test failed: Should throw on timeout\n";
    } catch(const std::exception& e) {
        std::cout << "Timeout test passed\n";
    }
}

// Concurrent Test
void concurrent_access_test() {
    constexpr int THREADS = 50;
    constexpr int OPS_PER_THREAD = 100;
    std::atomic<int> completed{0};
    
    auto& pool = ConnectionPoolManager::instance().get_pool("main_db");
    
    auto worker = [&] {
        try {
            for(int i = 0; i < OPS_PER_THREAD; ++i) {
                auto conn = pool.get_connection();
                pqxx::work txn(*conn);
                txn.exec("SELECT 1");
                txn.commit();
                ++completed;
            }
        } catch(const std::exception& e) {
            std::cerr << "Thread error: " << e.what() << "\n";
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < THREADS; ++i)
        threads.emplace_back(worker);

    for(auto& t : threads) t.join();

    std::cout << "Completed " << completed << " operations\n";
}

// Performance Test
void performance_benchmark() {
    auto& pool = ConnectionPoolManager::instance().get_pool("main_db");
    constexpr int TOTAL_OPS = 10000;
    constexpr int THREAD_COUNT = 100;

    std::atomic<int> ops{0};
    auto start = std::chrono::high_resolution_clock::now();

    auto worker = [&] {
        while(ops++ < TOTAL_OPS) {
            try {
                auto conn = pool.get_connection();
                pqxx::work txn(*conn);
                txn.exec("SELECT pg_sleep(0.001)");
                txn.commit();
            } catch(...) {}
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < THREAD_COUNT; ++i)
        threads.emplace_back(worker);

    for(auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    std::cout << "Completed " << TOTAL_OPS << " operations in " 
              << duration << "ms (" 
              << (TOTAL_OPS * 1000.0 / duration) << " ops/sec)\n";
}

// Stress Test
void extreme_stress_test() {
    constexpr int POOL_SIZE = 20;
    constexpr int THREADS = 500;
    constexpr int OPS_PER_THREAD = 1000;

    ConnectionPool pool("dbname=test user=postgres host=localhost port=5432 password=03058246191AaBbCcDd55@#%_postgres", POOL_SIZE, 5s, {});
    std::atomic<int> successes{0}, failures{0};

    auto worker = [&] {
        for(int i = 0; i < OPS_PER_THREAD; ++i) {
            try {
                auto conn = pool.get_connection();
                ++successes;
                std::this_thread::sleep_for(1ms);
            } catch(const std::exception&) {
                ++failures;
            }
        }
    };

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    
    for(int i = 0; i < THREADS; ++i)
        threads.emplace_back(worker);

    for(auto& t : threads) t.join();

    auto duration = duration_cast<seconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();

    std::cout << "Stress test results:\n"
              << "  Total operations: " << (successes + failures) << "\n"
              << "  Successes: " << successes << "\n"
              << "  Failures: " << failures << "\n"
              << "  Duration: " << duration << "s\n"
              << "  Throughput: " << (successes / duration) << " ops/sec\n";
}

int main() {
    try {
        demonstrate_typical_usage();
        test_connection_acquisition();
        
        concurrent_access_test();
        performance_benchmark();
        extreme_stress_test();
        
        ConnectionPoolManager::instance().shutdown();
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}