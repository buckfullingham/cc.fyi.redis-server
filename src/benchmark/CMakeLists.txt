include_directories(
        ../main
)

add_executable(benchmarks
        resp.cpp
        util.cpp
)

target_link_libraries(benchmarks PRIVATE
        redis_server_objects
        benchmark::benchmark_main
)
