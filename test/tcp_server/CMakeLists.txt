include_directories(${CMAKE_SOURCE_DIR})
add_executable(server tcp_server.cc)
target_link_libraries(server net)

