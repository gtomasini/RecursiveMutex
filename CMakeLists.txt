cmake_minimum_required(VERSION 3.1)

# Project
project(recursive-lock
   VERSION 0.1.0
   DESCRIPTION "C++ recursive lock"
)

# Completion
set( CMAKE_EXPORT_COMPILE_COMMANDS ON ) 
# Platform Threads support (e.g. Posix, Win32, etc)
find_package(Threads)

# This flag is recommended and only used if posix thread support is present
# It is used in conjuntion with CMAKE_THREAD_LIBS_INIT flag
set(THREADS_PREFER_PTHREAD_FLAG true)

if(MSVC)
	add_compile_options(/W3)
else()
	set(MY_FLAGS -Wall -pedantic -fnon-call-exceptions)
	add_compile_options(${MY_FLAGS})
	add_compile_options("$<$<CONFIG:DEBUG>:-g>")
endif()

set(CMAKE_CXX_STANDARD 17)

add_executable(recursiveLock RecursiveLock.cpp)
target_link_libraries(recursiveLock ${CMAKE_THREAD_LIBS_INIT} )
