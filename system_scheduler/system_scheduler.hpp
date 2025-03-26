#ifndef SYSTEM_SCHEDULER_HPP
#define SYSTEM_SCHEDULER_HPP

#include <memory>
#include <functional>
#include <iostream>
#include <exception>
#include <optional>
#include <thread>
#include <cstdint>
#include <vector>
#include <atomic>
#include <condition_variable>

#ifdef __linux__
#include <sched.h>
#include <numa.h>
#endif

namespace std::execution {

class lock_free_deque {
public:
    lock_free_deque() : capacity(DEFAULT_CAPACITY), top(0), bottom(0) {
        buffer = std::make_unique<std::vector<std::function<void()>>>();
        buffer->resize(capacity);
    }
    
    lock_free_deque(const lock_free_deque&) = delete;
    lock_free_deque& operator=(const lock_free_deque&) = delete;
    
    lock_free_deque(lock_free_deque&& other) noexcept 
        : capacity(other.capacity.load(std::memory_order_relaxed)), 
          top(other.top.load(std::memory_order_relaxed)), 
          bottom(other.bottom.load(std::memory_order_relaxed)), 
          buffer(std::move(other.buffer)) {
        other.capacity.store(0, std::memory_order_relaxed);
        other.top.store(0, std::memory_order_relaxed);
        other.bottom.store(0, std::memory_order_relaxed);
        other.buffer = nullptr;
    }
    
    lock_free_deque& operator=(lock_free_deque&& other) noexcept {
        if (this != &other) {
            capacity.store(other.capacity.load(std::memory_order_relaxed), std::memory_order_relaxed);
            top.store(other.top.load(std::memory_order_relaxed), std::memory_order_relaxed);
            bottom.store(other.bottom.load(std::memory_order_relaxed), std::memory_order_relaxed);
            buffer = std::move(other.buffer);
            other.capacity.store(0, std::memory_order_relaxed);
            other.top.store(0, std::memory_order_relaxed);
            other.bottom.store(0, std::memory_order_relaxed);
            other.buffer = nullptr;
        }
        return *this;
    }
    
    void push(std::function<void()> task) {
        int b = bottom.load(std::memory_order_relaxed);
        int t = top.load(std::memory_order_acquire);
        int size = b - t;
        int cap = capacity.load(std::memory_order_relaxed);
        
        if (size >= cap) {
            resize();
            b = bottom.load(std::memory_order_relaxed);
            cap = capacity.load(std::memory_order_relaxed);
        }
        
        (*buffer)[b % cap] = std::move(task);
        bottom.store(b + 1, std::memory_order_release);
    }
    
    bool pop(std::function<void()>& task) {
        int b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_seq_cst);
        int t = top.load(std::memory_order_seq_cst);
        int cap = capacity.load(std::memory_order_relaxed);
        
        if (t <= b) {
            task = std::move((*buffer)[b % cap]);
            if (t == b) {
                if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst)) {
                    task = nullptr;
                    bottom.store(b + 1, std::memory_order_relaxed);
                    return false;
                }
            }
            return true;
        } else {
            bottom.store(b + 1, std::memory_order_relaxed);
            return false;
        }
    }
    
    bool steal(std::function<void()>& task) {
        int t = top.load(std::memory_order_acquire);
        int b = bottom.load(std::memory_order_acquire);
        int cap = capacity.load(std::memory_order_relaxed);
        if (t < b) {
            task = std::move((*buffer)[t % cap]);
            if (top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst)) {
                return true;
            }
        }
        return false;
    }
    
    bool empty() const {
        int t = top.load(std::memory_order_acquire);
        int b = bottom.load(std::memory_order_acquire);
        return t >= b;
    }
    
    size_t size() const {
        int t = top.load(std::memory_order_acquire);
        int b = bottom.load(std::memory_order_acquire);
        return (b >= t) ? (b - t) : 0;
    }

private:
    static constexpr int DEFAULT_CAPACITY = 1024;
    std::atomic<int> capacity;
    std::unique_ptr<std::vector<std::function<void()>>> buffer;
    std::atomic<int> top;
    std::atomic<int> bottom;
    
    void resize() {
        int old_capacity = capacity.load(std::memory_order_acquire);
        int new_capacity = old_capacity * 2;
        
        auto new_buffer = std::make_unique<std::vector<std::function<void()>>>();
        new_buffer->resize(new_capacity);
        
        int t = top.load(std::memory_order_acquire);
        int b = bottom.load(std::memory_order_acquire);
        for (int i = t; i < b; ++i) {
            (*new_buffer)[i % new_capacity] = std::move((*buffer)[i % old_capacity]);
        }
        
        buffer = std::move(new_buffer);
        capacity.store(new_capacity, std::memory_order_release);
    }
};

enum class priority_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Updated work_queue_t to handle priorities with lock_free_deque
struct work_queue_t {
    std::vector<std::shared_ptr<lock_free_deque>> task_queues; // One deque per priority
    std::atomic<bool> active{true};
    
    work_queue_t() : task_queues(static_cast<size_t>(priority_t::CRITICAL) + 1) {
        for (auto& queue : task_queues) {
            queue = std::make_shared<lock_free_deque>();
        }
    }
    
    work_queue_t(work_queue_t&& other) noexcept 
        : task_queues(std::move(other.task_queues)), active(other.active.load()) {}
    
    work_queue_t& operator=(work_queue_t&& other) noexcept {
        if (this != &other) {
            task_queues = std::move(other.task_queues);
            active.store(other.active.load());
        }
        return *this;
    }
    
    void push_task(int prio, std::function<void()> task) {
        task_queues[prio]->push(std::move(task));
    }
    
    bool pop_task(std::function<void()>& task) {
        for (int p = static_cast<int>(priority_t::CRITICAL); p >= static_cast<int>(priority_t::LOW); --p) {
            if (task_queues[p]->pop(task)) return true;
        }
        return false;
    }
    
    bool steal_task(std::function<void()>& task) {
        for (int p = static_cast<int>(priority_t::CRITICAL); p >= static_cast<int>(priority_t::LOW); --p) {
            if (task_queues[p]->steal(task)) return true;
        }
        return false;
    }
    
    bool empty() const {
        for (const auto& dq : task_queues) {
            if (!dq->empty()) return false;
        }
        return true;
    }
    
    size_t size() const {
        size_t total = 0;
        for (const auto& dq : task_queues) {
            total += dq->size();
        }
        return total;
    }
};

class system_scheduler {
public:
    explicit system_scheduler(priority_t priority = priority_t::NORMAL, uint32_t thread_count = 0);
    virtual ~system_scheduler();
    
    system_scheduler(const system_scheduler&) = delete;
    system_scheduler(system_scheduler&&) = delete;
    system_scheduler& operator=(const system_scheduler&) = delete;
    system_scheduler& operator=(system_scheduler&&) = delete;
    
    bool operator==(const system_scheduler&) const noexcept;
    priority_t get_priority() const noexcept;
    void set_priority(priority_t priority) noexcept;
    
    virtual void schedule(std::function<void()> task, priority_t priority = priority_t::NORMAL) const noexcept;
    virtual void bulk_schedule(uint32_t n, std::function<void(uint32_t)> task, priority_t priority = priority_t::NORMAL) const noexcept;
    
    static std::shared_ptr<system_scheduler> query_system_context();
    
    template <class T>
    std::optional<T> try_query() const noexcept;
    
    virtual void set_error(std::exception_ptr error) noexcept;
    virtual void set_stopped() noexcept;
    
    uint32_t get_active_thread_count() const noexcept {
        return active_thread_count.load(std::memory_order_relaxed);
    }
    
private:
    priority_t priority_level;
    mutable std::vector<work_queue_t> work_queues;
    mutable std::condition_variable cv;
    mutable std::vector<std::thread> worker_threads;
    std::atomic<bool> stop_flag;
    
    mutable std::atomic<uint32_t> idle_count;
    mutable std::atomic<uint32_t> active_thread_count;
    uint32_t min_threads;
    uint32_t max_threads;
    
    mutable std::vector<int> worker_numa_nodes;
    mutable std::atomic<size_t> next_queue; // For round-robin scheduling
    mutable std::atomic<size_t> num_queues; // Store number of queues atomically
    
    void worker_loop(size_t thread_id);
};

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
class macos_system_scheduler : public system_scheduler {
public:
    using system_scheduler::system_scheduler;
    void schedule(std::function<void()> task, priority_t priority = priority_t::NORMAL) const noexcept override;
    ~macos_system_scheduler() override;
};
#endif

system_scheduler& get_system_scheduler(priority_t priority = priority_t::NORMAL);

} // namespace std::execution

#endif // SYSTEM_SCHEDULER_HPP