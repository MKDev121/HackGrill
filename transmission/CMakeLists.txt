cmake_minimum_required(VERSION 3.16)
project(TransmissionEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(TRANSMISSION_SOURCES
    src/main.cpp
    src/ipc/shm_ring_buffer.cpp
    src/ipc/control_socket.cpp
    src/audio/codec.cpp
    src/network/connection.cpp
    src/network/receiver.cpp
    src/network/sender.cpp
)

set(TRANSMISSION_HEADERS
    include/transmission/common.h
    src/ipc/shm_ring_buffer.h
    src/ipc/control_socket.h
    src/audio/codec.h
    src/network/connection.h
    src/network/receiver.h
    src/network/sender.h
)

add_executable(transmission_app ${TRANSMISSION_SOURCES} ${TRANSMISSION_HEADERS})

find_package(Threads REQUIRED)
target_link_libraries(transmission_app PRIVATE Threads::Threads)

if (WIN32)
    target_link_libraries(transmission_app PRIVATE ws2_32)
else()
    target_link_libraries(transmission_app PRIVATE rt)
endif()

enable_testing()
add_subdirectory(tests)
