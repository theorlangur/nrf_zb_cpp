cmake_minimum_required(VERSION 3.10)
project(NrfZBCpp VERSION 1.0.0 LANGUAGES CXX)

add_library(NrfZBCpp INTERFACE)

target_include_directories(NrfZBCpp INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


