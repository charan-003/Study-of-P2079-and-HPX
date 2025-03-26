#include "system_scheduler.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <atomic>

using Matrix = std::vector<std::vector<int>>;

void print_matrix(const Matrix &M, const std::string &name, int max_rows = 5, int max_cols = 5) {
    std::cout << "Matrix " << name << " (top-left " << max_rows << "x" << max_cols << " portion):\n";
    for (int i = 0; i < std::min(max_rows, static_cast<int>(M.size())); ++i) {
        for (int j = 0; j < std::min(max_cols, static_cast<int>(M[i].size())); ++j) {
            std::cout << M[i][j] << "\t";
        }
        std::cout << "\n";
    }
}

void multiply_matrices(const Matrix &A, const Matrix &B, Matrix &C, std::execution::system_scheduler& scheduler, std::atomic<int>& tasks_remaining) {
    int rowsA = A.size();
    int colsA = A[0].size();
    int colsB = B[0].size();

    C.resize(rowsA, std::vector<int>(colsB, 0));

    int num_threads = std::thread::hardware_concurrency();
    int block_size = rowsA / num_threads;
    tasks_remaining = num_threads;

    for (int t = 0; t < num_threads; ++t) {
        int start_row = t * block_size;
        int end_row = (t + 1) * block_size;
        if (t == num_threads - 1) end_row = rowsA;

        scheduler.schedule([start_row, end_row, colsA, colsB, &A, &B, &C, &tasks_remaining]() {
            for (int i = start_row; i < end_row; ++i) {
                for (int j = 0; j < colsB; ++j) {
                    double sum = 0.0;
                    for (int k = 0; k < colsA; ++k) {
                        sum += static_cast<double>(A[i][k]) * B[k][j] * std::sin(A[i][k]);
                    }
                    C[i][j] = static_cast<int>(sum);
                }
            }
            tasks_remaining.fetch_sub(1, std::memory_order_relaxed);
        }, std::execution::priority_t::NORMAL);
    }
}

int main(int argc, char* argv[]) {
    int size = 500;
    if (argc >= 2) {
        size = std::stoi(argv[1]);
        if (size <= 0) return 1;
    }
    Matrix A(size, std::vector<int>(size, 1));
    Matrix B(size, std::vector<int>(size, 1));
    Matrix C;
    std::atomic<int> tasks_remaining(0);

    std::execution::system_scheduler scheduler(std::execution::priority_t::NORMAL, std::thread::hardware_concurrency());
    multiply_matrices(A, B, C, scheduler, tasks_remaining);

    while (tasks_remaining.load(std::memory_order_relaxed) > 0) {
        std::this_thread::yield();
    }

    print_matrix(C, "C", 5, 5);

    return 0;
}




