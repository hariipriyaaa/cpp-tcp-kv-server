#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main() {
    constexpr int NUM_CLIENTS = 16;
    constexpr int REQUESTS_PER_CLIENT = 1000;   // 16,000 total
    using clock = std::chrono::steady_clock;

    std::atomic<int> success_count{0};
    std::vector<std::vector<double>> latencies(NUM_CLIENTS);

    auto start = clock::now();
    std::vector<std::thread> clients;
    for (int c = 0; c < NUM_CLIENTS; ++c) {
        clients.emplace_back([&, c]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return;
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
                close(sock);
                return;
            }
            latencies[c].reserve(REQUESTS_PER_CLIENT);
            char buf[256];
            for (int i = 0; i < REQUESTS_PER_CLIENT; ++i) {
                std::string msg = (i % 2 == 0)
                    ? "SET key_" + std::to_string(c) + " val_" + std::to_string(i) + "\n"
                    : "GET key_" + std::to_string(c) + "\n";
                auto t0 = clock::now();
                send(sock, msg.c_str(), msg.size(), 0);
                ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
                auto t1 = clock::now();
                if (n > 0) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                    latencies[c].push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                }
            }
            close(sock);
        });
    }
    for (auto& t : clients) t.join();
    double elapsed = std::chrono::duration<double>(clock::now() - start).count();

    std::vector<double> all;
    for (auto& v : latencies) all.insert(all.end(), v.begin(), v.end());
    if (all.empty()) {
        std::cerr << "No successful requests. Is the server running on port 8080?\n";
        return 1;
    }
    std::sort(all.begin(), all.end());
    double sum = 0; for (double x : all) sum += x;

    std::cout << "Clients            : " << NUM_CLIENTS << "\n";
    std::cout << "Completed Requests : " << success_count.load() << "\n";
    std::cout << "Total Time         : " << elapsed << " sec\n";
    std::cout << "Throughput         : " << static_cast<int>(success_count.load() / elapsed) << " req/sec\n";
    std::cout << "Avg Latency        : " << sum / all.size() << " ms\n";
    std::cout << "p99 Latency        : " << all[static_cast<size_t>(all.size() * 0.99)] << " ms\n";
    return 0;
}
