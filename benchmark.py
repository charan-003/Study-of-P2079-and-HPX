import subprocess
import time
import psutil
import matplotlib.pyplot as plt
import numpy as np
import os

output_dir = "/Users/saicharan/Desktop/final/github"
benchmark_file = os.path.join(output_dir, "benchmark_results.txt")

def get_system_info():
    cpu_count = psutil.cpu_count(logical=False)
    thread_count = psutil.cpu_count(logical=True)
    cpu_freq = psutil.cpu_freq().max if psutil.cpu_freq() else 0
    return cpu_count, thread_count, cpu_freq

def measure_performance_with_problem_size(executable_path, problem_sizes, runs=3):
    avg_times = []
    avg_cpu_usages = []
    max_thread_counts = []
    avg_memory_usages = []
    
    system_threads = psutil.cpu_count(logical=True)
    
    for problem_size in problem_sizes:
        times = []
        cpu_usages = []
        all_threads = []
        memory_usages = []
        
        for _ in range(runs):
            start_time = time.time()
            process = subprocess.Popen([executable_path, str(problem_size)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
            temp_cpu = []
            temp_threads = []
            temp_memory = []
            
            try:
                p = psutil.Process(process.pid)
            except psutil.NoSuchProcess:
                continue
            
            try:
                thread_count = min(system_threads, max(0, p.num_threads() - 1))
                temp_threads.append(thread_count)
            except psutil.NoSuchProcess:
                pass
            
            while process.poll() is None:
                try:
                    cpu_val = psutil.cpu_percent(interval=0.1)  # System-wide CPU usage
                    temp_cpu.append(cpu_val)
                    thread_count = min(system_threads, max(0, p.num_threads() - 1))
                    temp_threads.append(thread_count)
                    temp_memory.append(p.memory_info().rss / (1024 ** 2))
                except psutil.NoSuchProcess:
                    break
                time.sleep(0.1)
            
            process.wait()
            end_time = time.time()
            times.append(end_time - start_time)
            
            avg_cpu = np.mean(temp_cpu) if temp_cpu else 0
            avg_memory = np.mean(temp_memory) if temp_memory else 0
            
            cpu_usages.append(avg_cpu)
            memory_usages.append(avg_memory)
            all_threads.extend(temp_threads)
        
        avg_times.append(np.mean(times))
        avg_cpu_usages.append(np.mean(cpu_usages))
        max_threads = np.max(all_threads) if all_threads else 0
        max_thread_counts.append(max_threads)
        avg_memory_usages.append(np.mean(memory_usages))

        
    
    return avg_times, avg_cpu_usages, max_thread_counts, avg_memory_usages

cpu_cores, cpu_threads, cpu_freq = get_system_info()

problem_sizes = [10, 100, 250,500,750,1000]


hpx = "/Users/saicharan/Desktop/final/github/hpx/build/my_hpx_program"
scheduler = "/Users/saicharan/Desktop/final/github/system_scheduler/build/scheduler"

hpx_times, hpx_cpu, hpx_threads, hpx_memory = measure_performance_with_problem_size(hpx, problem_sizes)
scheduler_times, scheduler_cpu, scheduler_threads, scheduler_memory = measure_performance_with_problem_size(scheduler, problem_sizes)

avg_exec_time_hpx = np.mean(hpx_times)
avg_exec_time_scheduler = np.mean(scheduler_times)
avg_cpu_hpx = np.mean(hpx_cpu)
avg_cpu_scheduler = np.mean(scheduler_cpu)
avg_threads_hpx = np.mean(hpx_threads)
avg_threads_scheduler = np.mean(scheduler_threads)
avg_memory_hpx = np.mean(hpx_memory)
avg_memory_scheduler = np.mean(scheduler_memory)

with open(benchmark_file, "w") as f:
    f.write(f"Benchmarking on a system with {cpu_cores} CPU cores, {cpu_threads} threads, and a max frequency of {cpu_freq:.2f} MHz.\n\n")
    f.write("Performance Comparison:\n")
    f.write("| Metric               | HPX Execution | P2079-based Execution |\n")
    f.write("|----------------------|-------------------|-----------------------|\n")
    f.write(f"| Execution Time (s)   | {avg_exec_time_hpx:.2f}         | {avg_exec_time_scheduler:.2f}         |\n")
    f.write(f"| CPU Utilization (%)  | {avg_cpu_hpx:.2f}%        | {avg_cpu_scheduler:.2f}%        |\n")
    f.write(f"| Max Threads Used     | {avg_threads_hpx:.0f}          | {avg_threads_scheduler:.0f}          |\n")
    f.write(f"| Memory Usage (MB)    | {avg_memory_hpx:.2f} MB      | {avg_memory_scheduler:.2f} MB      |\n")

print(f"\nBenchmark results saved to: {benchmark_file}")
print(f"\nBenchmark Summary:\n{open(benchmark_file).read()}")

plt.figure(figsize=(18, 6))
plt.suptitle(f"System Specs: {cpu_cores} Cores, {cpu_threads} Threads, {cpu_freq:.2f} MHz\n"
             f"Avg Exec Time: HPX={avg_exec_time_hpx:.2f}s, P2079={avg_exec_time_scheduler:.2f}s | "
             f"CPU Usage: HPX={avg_cpu_hpx:.2f}%, P2079={avg_cpu_scheduler:.2f}%", fontsize=12)

plt.subplot(2, 2, 1)
plt.plot(problem_sizes, hpx_times, label='HPX', color='blue', marker='o', linestyle='-')
plt.plot(problem_sizes, scheduler_times, label='Scheduler', color='orange', marker='s', linestyle='-')
plt.ylabel('Execution Time (s)')
plt.xlabel('Problem Size')
plt.title('Execution Time vs Problem Size')
plt.legend()
plt.grid(True)

plt.subplot(2, 2, 2)
plt.plot(problem_sizes, hpx_cpu, label='HPX', color='blue', marker='o', linestyle='-')
plt.plot(problem_sizes, scheduler_cpu, label='Scheduler', color='orange', marker='s', linestyle='-')
plt.ylabel('CPU Usage (%)')
plt.xlabel('Problem Size')
plt.title('CPU Usage vs Problem Size')
plt.legend()
plt.grid(True)

plt.subplot(2, 2, 3)
plt.plot(problem_sizes, hpx_memory, label='HPX', color='blue', marker='o', linestyle='-')
plt.plot(problem_sizes, scheduler_memory, label='Scheduler', color='orange', marker='s', linestyle='-')
plt.ylabel('Memory Usage (MB)')
plt.xlabel('Problem Size')
plt.title('Memory Usage vs Problem Size')
plt.legend()
plt.grid(True)

plt.subplot(2, 2, 4)
plt.plot(problem_sizes, hpx_threads, label='HPX', color='blue', marker='o', linestyle='-')
plt.plot(problem_sizes, scheduler_threads, label='Scheduler', color='orange', marker='s', linestyle='-')
plt.ylabel('Max Thread Count')
plt.xlabel('Problem Size')
plt.title('Max Thread Count vs Problem Size')
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.subplots_adjust(top=0.85)

plot_file = os.path.join(output_dir, "benchmark_scaling_plot.png")
plt.savefig(plot_file)
plt.show()

print(f"\nScaling benchmark plot saved to: {plot_file}")