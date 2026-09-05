cmake_minimum_required(VERSION 3.14)
project(frontend LANGUAGES CXX)

set(BINARY_NAME "voip_translation_app")

cmake_policy(SET CMP0079 NEW)

# Flutter Windows configuration
set(FLUTTER_MANAGED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/flutter")
add_subdirectory(${FLUTTER_MANAGED_DIR} ${CMAKE_CURRENT_BINARY_DIR}/flutter)

add_executable(${BINARY_NAME} WIN32
  "runner/main.cpp"
  "runner/flutter_window.cpp"
  "runner/utils.cpp"
)

target_link_libraries(${BINARY_NAME} PRIVATE flutter flutter_wrapper_app)
