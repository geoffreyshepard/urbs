cmake_minimum_required(VERSION 3.25)
project(urbs VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(URBS_SANITIZE "Build with ASan + UBSan" OFF)
option(URBS_TRACY "Enable Tracy profiling" OFF)
option(URBS_BUILD_GAME "Build SDL game executable (off = headless only)" ON)

add_compile_definitions(URBS_VERSION="${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CPM)
include(Dependencies)
include(Warnings)

if(URBS_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests tests_build)
add_subdirectory(bench bench_build)
