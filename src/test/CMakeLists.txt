include_directories(
        ../main
)

add_executable(tests
        commands.cpp
        database.cpp
        io.cpp
        resp.cpp
        util.cpp
)

target_link_libraries(tests PRIVATE
        catch2::catch2_with_main
        redis_server_objects
)

catch_discover_tests(tests)
