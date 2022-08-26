cmake_minimum_required(VERSION 2.8)

project(721sim_cmake_proj)

if ("${CMAKE_VERSION}" VERSION_GREATER "3.0.0")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_EXTENSIONS OFF)
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

set(AC_CONFIGURE_PREFIX .)

add_compile_options(-Wno-sign-compare)
set(CMAKE_CXX_FLAGS_RELO3 "-O3 -g")
set(CMAKE_C_FLAGS_RELO3 "-O3 -g")

add_subdirectory(riscv-base)
add_subdirectory(uarchsim)

