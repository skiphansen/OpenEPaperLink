cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set project name and language
project(add_ota_hdr C)
set(CMAKE_BUILD_TYPE "Debug")

# Compilation options
set(CMAKE_C_FLAGS "-std=gnu99 -Wall")

set(COMMON ../common CACHE STRING "Path to shared code")

# Sources for this executable
set (SOURCES 
   add_ota_hdr.c
   crc32.c
)

add_executable(add_ota_hdr ${SOURCES})

include_directories(${COMMON})



