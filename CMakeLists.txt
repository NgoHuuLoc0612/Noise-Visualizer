cmake_minimum_required(VERSION 3.18)
project(noise_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Release flags ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
  -O3 -march=native -ffast-math -funroll-loops
  -Wall -Wextra -Wno-unused-parameter
)

# ── pybind11 ──────────────────────────────────────────────────────────────────
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Try system pybind11 first, then fetch
find_package(pybind11 QUIET)
if(NOT pybind11_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.12.0
  )
  FetchContent_MakeAvailable(pybind11)
endif()

# ── Extension ─────────────────────────────────────────────────────────────────
pybind11_add_module(noise_engine bindings.cpp)
target_include_directories(noise_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Install next to server.py ─────────────────────────────────────────────────
install(TARGETS noise_engine DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
