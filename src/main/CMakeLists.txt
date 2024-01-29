add_library(redis_server_objects OBJECT
        command_handler.cpp
        commands.cpp
        database.cpp
        io.cpp
        resp.cpp
)

add_executable(redis_server
        redis_server.cpp
)

target_include_directories(redis_server_objects PUBLIC
        ${unordered_dense_INCLUDE_DIRS}
)

target_link_libraries(redis_server PRIVATE
        redis_server_objects
        ${unordered_dense_LIBRARIES}
)
