include_directories(../)
add_executable(server tcp_server.cc)
target_link_libraries(server net)

add_executable(client tcp_client.cc)
target_link_libraries(client net)

add_executable(timer timer_test.cc)
target_link_libraries(timer net)
