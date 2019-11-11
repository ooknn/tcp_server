cmake_minimum_required(VERSION 3.4)

project(server)

include(ProcessorCount) 
ProcessorCount(N) 
if(NOT N EQUAL 0) 
    set(JOBS_IN_PARALLEL -j${N}) 
endif(NOT N EQUAL 0)

set(CMAKE_CXX_STANDARD 17 )
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS " -g -ggdb -O0 -Wall -Werror")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON )
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)

set(SOURCE
  io_context.cc
  acceptor.cc
  connection.cc
  timer.cc
  socket.cc
  log.cc
  server.cc
  error.cc
  connector.cc
  client.cc
  )

add_library(net STATIC ${SOURCE})
add_subdirectory(${PROJECT_SOURCE_DIR}/test)
