include_directories(${CMAKE_SOURCE_DIR})
include_directories(/opt/ffmpeg/include)
link_directories(/opt/ffmpeg/lib)

add_executable(rtsp_client 
    rtsp.cc
    decoder.cc
    main.cc
    )

target_link_libraries(rtsp_client 
    net
    avformat
    avcodec
    avfilter 
    avdevice 
    swresample 
    swscale 
    avutil
    )

