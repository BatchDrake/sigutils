cmake_minimum_required(VERSION 3.20.0)
project(testCmakeSigUtils)

find_package(sigutils REQUIRED)

add_executable(testCmakeSigUtils test.c)

target_link_libraries(testCmakeSigUtils sigutils::sigutils)