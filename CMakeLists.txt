cmake_minimum_required(VERSION 3.11)
project(application_inlined_nic_driver C)
# At past, I was used printf in some parts to prove the logic works as expected.
option(DEBUG_MODE "Enable debug logging" OFF)

if(DEBUG_MODE)
    add_definitions(-DDEBUG_MODE)
    message(STATUS "Debug Mode is enabled, may cause high overhead.")
else()
    message(STATUS "Debug Mode is disabled.")
    message(STATUS "application codes may be removed from compiler, known as dead code elimination.")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -fomit-frame-pointer -Wall -Wextra")

file(GLOB_RECURSE ALL_SOURCES "src/*.c" "selftests/*.c")

include(FetchContent)
set(HDR_LOG_REQUIRED OFF CACHE BOOL "" FORCE)
fetchcontent_declare(
  hdr_histogram
  GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
  GIT_TAG 0.11.5
)
fetchcontent_makeavailable(hdr_histogram)

add_executable(driver ${ALL_SOURCES})
target_link_libraries(driver PRIVATE hdr_histogram_static)
target_include_directories(driver PRIVATE src/ selftests/)