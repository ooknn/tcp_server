include_directories(${CMAKE_SOURCE_DIR})
add_executable(client tcp_client.cc)
target_link_libraries(client net)

