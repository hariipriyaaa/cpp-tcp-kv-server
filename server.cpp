#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "ThreadPool.hpp"

std::atomic<bool> running{true};
int server_fd = -1;

// shutdown() reliably wakes a blocked accept() on Linux, whichever thread gets the signal
void signal_handler(int) {
    running = false;
    if (server_fd != -1) shutdown(server_fd, SHUT_RDWR);
}

class ConcurrentKVStore {
private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::shared_mutex rw_mutex_;

public:
    void set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);   // exclusive write
        store_[key] = value;
    }
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);   // concurrent reads
        auto it = store_.find(key);
        return (it != store_.end()) ? it->second : "(nil)";
    }
    bool del(const std::string& key) {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        return store_.erase(key) > 0;
    }
};

std::string process_command(const std::string& line, ConcurrentKVStore& kv) {
    std::istringstream iss(line);
    std::string cmd, key, val;
    iss >> cmd;

    if (cmd == "PING") return "PONG\n";
    if (cmd == "SET") {
        iss >> key;
        std::getline(iss >> std::ws, val);
        if (key.empty() || val.empty()) return "ERR usage: SET <key> <value>\n";
        kv.set(key, val);
        return "OK\n";
    }
    if (cmd == "GET") {
        iss >> key;
        if (key.empty()) return "ERR usage: GET <key>\n";
        return kv.get(key) + "\n";
    }
    if (cmd == "DEL") {
        iss >> key;
        if (key.empty()) return "ERR usage: DEL <key>\n";
        return kv.del(key) ? "1\n" : "0\n";
    }
    return "ERR unknown command\n";
}

void handle_client(int client_fd, ConcurrentKVStore& kv) {
    // 1s receive timeout so idle connections notice server shutdown
    timeval tv{1, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buffer[1024];
    std::string pending;

    while (true) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n == 0) break;                       // client closed
        if (n < 0) {
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && running) continue;
            break;                               // error or shutting down
        }
        pending.append(buffer, static_cast<size_t>(n));

        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            pending.erase(0, pos + 1);
            if (!line.empty()) {
                std::string resp = process_command(line, kv);
                send(client_fd, resp.c_str(), resp.size(), MSG_NOSIGNAL);  // no SIGPIPE crash
            }
        }
    }
    close(client_fd);
}

int main() {
    constexpr int PORT = 8080;
    constexpr size_t NUM_WORKERS = 16;   // must be >= concurrent clients (one worker per connection)
    constexpr size_t MAX_QUEUE = 256;

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                     // no SA_RESTART
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind failed"); close(server_fd); return 1;
    }
    if (listen(server_fd, 128) < 0) {
        perror("listen failed"); close(server_fd); return 1;
    }

    ConcurrentKVStore kv_store;
    {
        ThreadPool pool(NUM_WORKERS, MAX_QUEUE);
        std::cout << "[Server] Listening on port " << PORT << " with " << NUM_WORKERS
                  << " worker threads. Ctrl+C to stop.\n";

        while (running) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
            if (client_fd < 0) {
                if (!running) break;
                continue;
            }
            if (!pool.submit([client_fd, &kv_store]() { handle_client(client_fd, kv_store); })) {
                close(client_fd);
            }
        }
        std::cout << "\n[Server] Shutting down cleanly...\n";
    }   // ~ThreadPool joins workers here (RAII)
    close(server_fd);
    return 0;
}
