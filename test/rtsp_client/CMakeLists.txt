include_directories(${CMAKE_SOURCE_DIR})
add_executable(rtsp_client 
    rtsp.cc
    main.cc
    )
target_link_libraries(rtsp_client net)

