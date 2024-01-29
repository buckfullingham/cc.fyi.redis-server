find_package(unordered_dense REQUIRED)
add_subdirectory(main)

find_package(Catch2 REQUIRED)
find_package(benchmark REQUIRED)
include(CTest)
include(Catch)

add_subdirectory(test)
add_subdirectory(benchmark)


