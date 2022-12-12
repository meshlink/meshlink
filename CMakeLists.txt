project(meshlink)
cmake_minimum_required(VERSION 3.12)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(CheckSymbolExists)
include(CheckIncludeFile)
include(CTest)

#add_definitions(-D_GNU_SOURCE)
#add_definitions(-D_POSIX_C_SOURCE=200112L)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
#list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
#add_compile_definitions(_GNU_SOURCE)
#string(APPEND CMAKE_C_FLAGS "-D_GNU_SOURCE")

check_symbol_exists(setns "sched.h" HAVE_SETNS)
check_symbol_exists(strsignal "string.h" HAVE_STRSIGNAL)
check_symbol_exists(asprintf "stdio.h" HAVE_ASPRINTF)
check_symbol_exists(pselect "sys/select.h" HAVE_PSELECT)
check_include_file(stdatomic.h HAVE_STDATOMIC_H)
configure_file(cmake_config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	enable_testing()
	add_subdirectory(test)
endif()
