#include "system_scheduler.hpp"
#include <random>
#include <chrono>
#include <iostream>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>

#ifdef __linux__
#include <numa.h>
#endif

namespace {
    thread_local bool is_worker_thread = false;
    thread_local size_t local_worker_index = 0;
#ifdef __linux__
    thread_local int local_numa_node = 0;
#endif
}

namespace std::execution {

system_scheduler::system_scheduler(priority_t priority, uint32_t thread_count) 
    : priority_level(priority), stop_flag(false), next_queue(0) {
    uint32_t init_threads = thread_count > 0 ? thread_count : std::thread::hardware_concurrency();
    min_threads = init_threads;
    max_threads = init_threads;
    idle_count.store(0, std::memory_order_relaxed);
    active_thread_count.store(init_threads, std::memory_order_relaxed);
    
    worker_threads.reserve(max_threads);
    worker_numa_nodes.resize(max_threads, 0);
    work_queues.resize(max_threads);
    num_queues.store(max_threads, std::memory_order_relaxed);
    
#ifdef __linux__
    int num_nodes = (numa_available() != -1) ? numa_max_node() + 1 : 1;
    for (uint32_t i = 0; i < init_threads; ++i) {
        worker_numa_nodes[i] = i % num_nodes;
    }
#else
    // macOS: all on node 0
#endif

    for (uint32_t i = 0; i < init_threads; ++i) {
        worker_threads.emplace_back(&system_scheduler::worker_loop, this, i);
    }
}

system_scheduler::~system_scheduler() {
    stop_flag.store(true, std::memory_order_relaxed);
    
    for (auto& thread : worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool system_scheduler::operator==(const system_scheduler&) const noexcept {
    return true;
}

priority_t system_scheduler::get_priority() const noexcept {
    return priority_level;
}

void system_scheduler::set_priority(priority_t priority) noexcept {
    priority_level = priority;
}

void system_scheduler::schedule(std::function<void()> task, priority_t priority) const noexcept {
    if (stop_flag.load(std::memory_order_relaxed)) return;
    
    size_t num = num_queues.load(std::memory_order_relaxed);
    size_t chosen = next_queue.fetch_add(1, std::memory_order_relaxed) % num;
    
    while (!work_queues[chosen].active.load(std::memory_order_relaxed)) {
        chosen = (chosen + 1) % num;
    }
    work_queues[chosen].push_task(static_cast<int>(priority), std::move(task));
}

void system_scheduler::bulk_schedule(uint32_t n, std::function<void(uint32_t)> task, priority_t priority) const noexcept {
    uint32_t active_threads = active_thread_count.load(std::memory_order_relaxed);
    uint32_t num_chunks = std::max(active_threads * 8, n);
    if (num_chunks == 0) num_chunks = 1;
    uint32_t chunk_size = n / num_chunks;
    uint32_t remainder = n % num_chunks;
    
    for (uint32_t chunk = 0; chunk < num_chunks; ++chunk) {
        uint32_t start = chunk * chunk_size + std::min(chunk, remainder);
        uint32_t end = start + chunk_size + (chunk < remainder ? 1 : 0);
        if (start < end) {
            schedule([=]() {
                for (uint32_t i = start; i < end; ++i) {
                    task(i);
                }
            }, priority);
        }
    }
}

void system_scheduler::worker_loop(size_t thread_id) {
    is_worker_thread = true;
    local_worker_index = thread_id;
#ifdef __linux__
    int node = worker_numa_nodes[thread_id];
    local_numa_node = node;
    if (numa_available() != -1) numa_run_on_node(node);
#endif
    
    std::vector<size_t> steal_indices;
    steal_indices.reserve(work_queues.size() - 1);
    for (size_t i = 0; i < work_queues.size(); ++i) {
        if (i != thread_id) {
            steal_indices.push_back(i);
        }
    }
    
    std::mt19937 rng(std::random_device{}());
    
    while (true) {
        std::function<void()> task;
        bool found_task = false;
        
        if (thread_id < work_queues.size() && work_queues[thread_id].pop_task(task)) {
            found_task = true;
        }
        
        if (!found_task) {
            std::shuffle(steal_indices.begin(), steal_indices.end(), rng);
            for (size_t steal_id : steal_indices) {
                if (work_queues[steal_id].active.load(std::memory_order_relaxed) && work_queues[steal_id].steal_task(task)) {
                    found_task = true;
                    break;
                }
            }
        }
        
        if (found_task) {
            task();
        } else {
            idle_count.fetch_add(1, std::memory_order_relaxed);
            
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            
            idle_count.fetch_sub(1, std::memory_order_relaxed);
            
            if (stop_flag.load(std::memory_order_relaxed) && 
                std::all_of(work_queues.begin(), work_queues.end(), 
                            [](const work_queue_t& q) { return q.empty(); })) {
                return;
            }
        }
    }
}

std::shared_ptr<system_scheduler> system_scheduler::query_system_context() {
    static std::shared_ptr<system_scheduler> instance = std::make_shared<system_scheduler>();
    return instance;
}

template <class T>
std::optional<T> system_scheduler::try_query() const noexcept {
    if constexpr (std::is_same_v<T, std::atomic<bool>>) {
        static std::atomic<bool> stop_token{false};
        return stop_token;
    }
    return std::nullopt;
}

void system_scheduler::set_error(std::exception_ptr error) noexcept {
    try {
        if (error) std::rethrow_exception(error);
    } catch (const std::exception& e) {
        std::cerr << "System Scheduler Error: " << e.what() << std::endl;
    }
}

void system_scheduler::set_stopped() noexcept {
    stop_flag.store(true, std::memory_order_relaxed);
    std::cerr << "System Scheduler: Execution Stopped." << std::endl;
}

#if defined(__APPLE__)
void macos_system_scheduler::schedule(std::function<void()> task, priority_t priority) const noexcept {
    long dispatch_priority;
    switch (priority) {
        case priority_t::LOW: dispatch_priority = DISPATCH_QUEUE_PRIORITY_LOW; break;
        case priority_t::NORMAL: dispatch_priority = DISPATCH_QUEUE_PRIORITY_DEFAULT; break;
        case priority_t::HIGH: dispatch_priority = DISPATCH_QUEUE_PRIORITY_HIGH; break;
        case priority_t::CRITICAL: dispatch_priority = DISPATCH_QUEUE_PRIORITY_HIGH; break;
        default: dispatch_priority = DISPATCH_QUEUE_PRIORITY_DEFAULT;
    }
    dispatch_async(dispatch_get_global_queue(dispatch_priority, 0), ^{
        task();
    });
}

macos_system_scheduler::~macos_system_scheduler() = default;
#endif

std::mutex scheduler_mutex;
std::shared_ptr<system_scheduler> current_scheduler = nullptr;

void set_system_scheduler(std::shared_ptr<system_scheduler> scheduler) {
    std::scoped_lock lock(scheduler_mutex);
    current_scheduler = scheduler;
}

system_scheduler& get_system_scheduler(priority_t priority) {
    std::scoped_lock lock(scheduler_mutex);
    if (current_scheduler) {
        return *current_scheduler;
    }
    static macos_system_scheduler scheduler(priority);
    return scheduler;
}

}  // namespace std::execution