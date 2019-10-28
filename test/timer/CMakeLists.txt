include_directories(${CMAKE_SOURCE_DIR})
add_executable(timer timer_test.cc)
target_link_libraries(timer net)
