cmake_minimum_required(VERSION 3.31)
project(PackageTest)

option(ENABLE_STATIC_ANALYZER "Enable clang-tidy static analyzer" FALSE)
message(STATUS "clang-tidy ${ENABLE_STATIC_ANALYZER}")

if(ENABLE_STATIC_ANALYZER)
  set(CMAKE_CXX_CLANG_TIDY
    clang-tidy
    --config-file ${CMAKE_CURRENT_LIST_DIR}/.clang-tidy
  )

endif()

find_package(stdnet REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)

add_executable(simple_http_server main.cpp)
target_link_libraries(simple_http_server stdnet::stdnet fmt::fmt)
