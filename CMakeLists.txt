cmake_minimum_required(VERSION 3.17)
project(PackageTest)

find_package(stdnet REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)

add_executable(simple_http_server main.cpp)
target_link_libraries(simple_http_server stdnet::stdnet fmt::fmt)
