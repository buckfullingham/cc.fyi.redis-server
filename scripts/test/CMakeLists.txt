find_program(BASH_PROGRAM bash REQUIRED)
enable_testing()
add_test(
        integration_test
        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/integration_test.sh ${CMAKE_BINARY_DIR}/src/main/redis_server
)
