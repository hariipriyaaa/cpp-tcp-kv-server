# Multithreaded C++ TCP Key-Value Server

A high-concurrency, Redis-inspired in-memory key-value server built using **C++17, POSIX TCP sockets, and multithreading**. Designed around a fixed-size thread pool, bounded task queue, and thread-safe storage to handle concurrent client requests efficiently.

---

## Project Overview

| Attribute | Details |
|:---|:---|
| **Project** | Multithreaded TCP Key-Value Server |
| **Language** | C++17 |
| **Architecture** | Client-Server |
| **Networking** | TCP / POSIX Sockets |
| **Concurrency** | Fixed Thread Pool |
| **Synchronization** | Mutex, Condition Variables, Shared Mutex |
| **Data Storage** | In-memory Key-Value Store |
| **Platform** | Linux |
| **Default Port** | 8080 |

## Key Features

| Feature | Description |
|:---|:---|
| Concurrent Clients | Handles multiple simultaneous TCP client connections |
| Thread Pool | Uses 16 worker threads to process client connections |
| Bounded Task Queue | Limits queued tasks to 256 and applies backpressure |
| Thread-Safe Storage | Uses `std::shared_mutex` for synchronized access |
| Concurrent Reads | Allows multiple clients to execute read operations simultaneously |
| Exclusive Writes | Protects write and delete operations using exclusive locking |
| Graceful Shutdown | Closes the task queue and joins worker threads |
| Benchmarking | Measures throughput, average latency, and p99 latency |

## System Architecture

```text
                  TCP CLIENTS
              ┌────────┬────────┐
              │Client 1│Client 2│ ... Client N
              └────┬───┴────┬───┘
                   │        │
                   ▼        ▼
             ┌──────────────────┐
             │   TCP SERVER     │
             │   accept()      │
             └────────┬─────────┘
                      │
                      ▼
             ┌──────────────────┐
             │  BOUNDED QUEUE   │
             │  Capacity: 256   │
             └────────┬─────────┘
                      │
                      ▼
             ┌──────────────────┐
             │   THREAD POOL    │
             │  16 Workers      │
             └────────┬─────────┘
                      │
                      ▼
             ┌──────────────────┐
             │    KV STORE      │
             │  shared_mutex    │
             └──────────────────┘
```

### Request Processing Workflow

1. The main server thread listens for incoming TCP connections using `accept()`.
2. Each accepted connection is wrapped into a task and submitted to the bounded queue.
3. If the queue reaches capacity, task submission blocks, applying backpressure.
4. An available worker thread retrieves and processes the client connection.
5. The worker reads newline-delimited commands and executes the corresponding operation.
6. The key-value store uses shared locking for reads and exclusive locking for modifications.

## Supported Commands

The server supports four basic Redis-inspired operations.

| Command | Description | Example |
|:---|:---|:---|
| `PING` | Checks server availability | `PING` |
| `SET` | Stores a value against a key | `SET name Haripriya` |
| `GET` | Retrieves a value using its key | `GET name` |
| `DEL` | Deletes an existing key | `DEL name` |

### Example Client Session

```text
PING
PONG

SET language C++17
OK

GET language
C++17

DEL language
1

GET language
(nil)
```

## Project Structure

| File | Responsibility |
|:---|:---|
| `BoundedBuffer.hpp` | Implements a thread-safe bounded queue using mutexes and condition variables |
| `ThreadPool.hpp` | Manages worker threads, task execution, and graceful shutdown |
| `server.cpp` | Implements the TCP server, command parsing, and in-memory key-value storage |
| `benchmark.cpp` | Implements a multithreaded client load generator |
| `.gitignore` | Excludes compiled binaries and log files |

## Getting Started

### Prerequisites

- Linux operating system
- GCC / G++ compiler supporting C++17
- POSIX threads (`pthread`)
- Netcat (`nc`) for testing client connections

### 1. Clone the Repository

```bash
git clone https://github.com/YOUR-USERNAME/cpp-tcp-kv-server.git
cd cpp-tcp-kv-server
```

### 2. Compile the Project

Compile the server:

```bash
g++ -std=c++17 -O2 -pthread server.cpp -o tcp_server
```

Compile the benchmark:

```bash
g++ -std=c++17 -O2 -pthread benchmark.cpp -o benchmark
```

### 3. Start the Server

```bash
./tcp_server
```

### 4. Connect Using Netcat

Open another terminal:

```bash
nc 127.0.0.1 8080
```

You can now execute `PING`, `SET`, `GET`, and `DEL` commands.

## Performance Benchmark

The project includes a multithreaded benchmark to evaluate request throughput and response latency.

### Benchmark Configuration

| Parameter | Value |
|:---|:---|
| Concurrent Clients | 16 |
| Requests per Client | 20,000 |
| Total Requests | 320,000 |
| Request Distribution | 50% SET / 50% GET |
| Network | TCP Loopback |
| Execution | Single Machine |

### Benchmark Results

| Run | Throughput (req/sec) | Average Latency (ms) | p99 Latency (ms) |
|:---:|---:|---:|---:|
| 1 | 163,270 | 0.059 | 0.308 |
| 2 | 175,152 | 0.054 | 0.321 |
| 3 | 160,422 | 0.062 | 0.507 |
| 4 | 148,017 | 0.065 | 0.498 |
| 5 | 138,969 | 0.066 | 0.527 |

**Median Throughput: ~160,422 requests/second**

The benchmark uses a closed-loop workload, where each client waits for a response before sending its next request. Results may vary depending on CPU scheduling and system load.

## Engineering Concepts Demonstrated

| Concept | Implementation |
|:---|:---|
| Producer-Consumer Pattern | Bounded task queue |
| Concurrency | Fixed-size worker thread pool |
| Synchronization | Mutexes and condition variables |
| Reader-Writer Locking | `std::shared_mutex` |
| Resource Management | RAII and automatic thread cleanup |
| Network Programming | POSIX TCP sockets |
| TCP Framing | Newline-delimited request processing |
| Backpressure | Blocking task submission when the queue is full |
| Performance Analysis | Throughput and latency benchmarking |
| Graceful Shutdown | Queue closure and worker thread joining |

## Current Limitations & Future Improvements

| Current Limitation | Potential Improvement |
|:---|:---|
| Data is lost when the server shuts down | Implement persistent storage using an append-only log |
| One worker handles one client connection at a time | Introduce an `epoll`-based event-driven networking model |
| No maximum request-line length | Add request-size validation and limits |
| ThreadSanitizer validation is pending | Perform race-condition testing on a compatible Linux environment |
| Fixed worker pool size | Make worker count configurable |

## Learning Outcomes

Through this project, I explored practical systems programming concepts, including multithreaded server design, TCP client-server communication, synchronization, resource management, concurrent data access, backpressure, and performance benchmarking.

---

**Built with C++17 | Linux | POSIX Sockets | Multithreading**
