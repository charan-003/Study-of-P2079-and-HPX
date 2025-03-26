#include <hpx/hpx_main.hpp>
#include <hpx/algorithm.hpp>
#include <hpx/execution.hpp>
#include <hpx/init.hpp>
#include <iostream>
#include <vector>
#include <cmath>

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

void multiply_matrices(const Matrix &A, const Matrix &B, Matrix &C) {
    int rowsA = A.size();
    int colsA = A[0].size();
    int colsB = B[0].size();

    C.resize(rowsA, std::vector<int>(colsB, 0));

    int num_threads = std::thread::hardware_concurrency();
    int block_size = rowsA / num_threads;

    hpx::experimental::for_loop(hpx::execution::par, 0, num_threads, [&](std::size_t t) {
        int start_row = t * block_size;
        int end_row = (t == num_threads - 1) ? rowsA : (t + 1) * block_size;
        for (std::size_t i = start_row; i < end_row; ++i) {
            hpx::experimental::for_loop(0, colsB, [&](std::size_t j) {
                double sum = 0.0; // Accumulate as double
                for (std::size_t k = 0; k < colsA; ++k) {
                    double term = static_cast<double>(A[i][k]) * B[k][j] * std::sin(A[i][k]);
                    sum += term;
                }
                C[i][j] = static_cast<int>(sum); // Cast after accumulation
            });
        }
    });
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

    multiply_matrices(A, B, C);
    print_matrix(C, "C"); // Print top-left 5x5 portion

    return 0;
}