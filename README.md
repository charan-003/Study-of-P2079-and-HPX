## Exploring High Performance Scheduling: A Comparative Study of P2079 and HPX

[Exploring High Performance Scheduling: A Comparative Study of P2079 and HPX]([url](https://docs.google.com/document/d/1cfYmVl4atqdxcjnk6Bj4lyvdwWbBtQmf1nDdFg2e4xM/edit?tab=t.0))

### Overview
This project presents a comparative study of **P2079 (system_scheduler)** and **HPX**, focusing on high-performance task scheduling for parallel workloads. P2079 is a lightweight scheduler optimized for memory efficiency and execution speed, while HPX is a scalable parallel computing framework. 

The study evaluates:
- **Execution time**
- **Memory usage**
- **Thread utilization**
- **CPU usage**

The benchmarking task is **matrix multiplication** on an **8-core system**, comparing different scheduling strategies.

---

## Features
- **P2079 Scheduler**
  - Lock-free task submission
  - Efficient work-stealing
  - Optimized task distribution
  - Low-memory footprint
  - Faster execution compared to HPX

- **Benchmarking**
  - Matrix multiplication for problem sizes **10 to 1000**
  - Execution time, memory, and CPU utilization comparison

- **Performance Highlights**
  - P2079 is **27.4% faster** than HPX (0.77s vs. 1.06s).
  - P2079 uses **63.7% less memory** (5.62 MB vs. 15.47 MB).
  - P2079 uses **5 threads**, whereas HPX uses **6 threads**.

---

## Setup Instructions

### Prerequisites
Ensure you have the following dependencies installed:

- **C++ Compiler** (GCC/Clang with C++20 support)
- **CMake** (for build configuration)
- **HPX Library** (if running HPX benchmarks)
- **Boost** (if required by HPX)

### Build P2079
```sh
cd system_scheduler
mkdir build && cd build
cmake ..
make 
```

### Run the Benchmark
```sh
python3 benchmark.py
```

---

## Results

| Metric         | P2079   | HPX    | Difference |
|---------------|--------|--------|------------|
| Execution Time | **0.77s** | 1.06s  | 27.4% faster |
| Memory Usage   | **5.62MB** | 15.47MB | 63.7% less |
| Thread Usage   | 5 Threads | 6 Threads | - |
| CPU Usage (%)  | 390.14%  | 390.38% | - |

### Key Takeaways
- **P2079 prioritizes memory efficiency and execution speed**, making it ideal for **resource-constrained environments**.
- **HPX focuses on scalability and thread utilization**, which is beneficial for workloads requiring maximum parallelism.
- **Lock-free programming** and **optimized work-stealing** significantly improve performance in P2079.
- P2079 currently has **race condition issues**, requiring further optimizations.

---

## Future Improvements
- **Race Condition Fixes**: Improve synchronization mechanisms.
- **Dynamic Scaling**: Implement adaptive thread scaling for better performance across workloads.
- **NUMA Awareness**: Optimize thread affinity to maximize performance on NUMA architectures.

---

## References
- **HPX Documentation**: [https://stellar-group.github.io/hpx-docs/](https://stellar-group.github.io/hpx-docs/)
- **P2079 Proposal**: [https://wg21.link/P2079](https://wg21.link/P2079)
- **C++ Sender/Receiver Model**: [https://wg21.link/P2300](https://wg21.link/P2300)

---

## License
This project is licensed under the **MIT License**.
