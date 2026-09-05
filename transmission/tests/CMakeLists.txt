cmake_minimum_required(VERSION 3.16)

add_executable(test_shm_buffer
    test_shm_buffer.cpp
    ../src/ipc/shm_ring_buffer.cpp
)
target_include_directories(test_shm_buffer PRIVATE ../include ../src)
target_link_libraries(test_shm_buffer PRIVATE Threads::Threads)
if (WIN32)
    target_link_libraries(test_shm_buffer PRIVATE ws2_32)
else()
    target_link_libraries(test_shm_buffer PRIVATE rt)
endif()
add_test(NAME test_shm_buffer COMMAND test_shm_buffer)

add_executable(test_network
    test_network.cpp
    ../src/network/connection.cpp
    ../src/audio/codec.cpp
)
target_include_directories(test_network PRIVATE ../include ../src)
target_link_libraries(test_network PRIVATE Threads::Threads)
if (WIN32)
    target_link_libraries(test_network PRIVATE ws2_32)
endif()
add_test(NAME test_network COMMAND test_network)
