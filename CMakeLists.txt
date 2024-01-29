cmake_minimum_required(VERSION 3.20)
project(redis_server)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_CXX_FLAGS_UBSAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
set(CMAKE_EXE_LINKER_FLAGS_UBSAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=undefined")
set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXEC_LINKER_FLAGS_DEBUG} -fsanitize=address")

#execute_process(COMMAND sleep 9999)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

enable_testing()

add_subdirectory(src)
add_subdirectory(scripts)
