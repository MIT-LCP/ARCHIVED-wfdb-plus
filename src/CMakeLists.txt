cmake_minimum_required(VERSION 3.10)

project(WFDB VERSION 20.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# External dependencies.
# Example instructions: https://github.com/google/googletest/blob/main/googletest/README.md#incorporating-into-an-existing-cmake-project
include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.11.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20211102.0
)
# Usage instructions: https://github.com/abseil/abseil-cpp/blob/master/CMake/README.md
FetchContent_MakeAvailable(abseil)

add_subdirectory(lib)
