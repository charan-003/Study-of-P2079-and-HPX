cmake_minimum_required(VERSION 3.19)
project(my_hpx_project CXX)

find_package(HPX REQUIRED)

add_executable(my_hpx_program matrix_multiplication.cpp)
target_link_libraries(my_hpx_program HPX::hpx HPX::wrap_main HPX::iostreams_component)

# Set the runtime search path to find HPX libraries at runtime.
set_target_properties(my_hpx_program PROPERTIES
    BUILD_RPATH "/Users/saicharan/Desktop/hpx/build/lib"
)
