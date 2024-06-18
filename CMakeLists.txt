
cmake_minimum_required(VERSION 3.10)

project(UDX_APP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_LINK_JOBS_MAX 4) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
find_package (cndp REQUIRED)
find_package (dpdk REQUIRED)
find_package (dpdk_pmd REQUIRED)

# Set Enviroment Variables
set(CONFIG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/config)
set(CORE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/core)
set(UNET_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/unet)
set(UTILITY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/utility)

include_directories(${CONFIG_INCLUDE_DIR})
include_directories(${CORE_INCLUDE_DIR})
include_directories(${UNET_INCLUDE_DIR})
include_directories(${UTILITY_INCLUDE_DIR}/log ${UTILITY_INCLUDE_DIR}/shm)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_SOURCE_DIR}/app)
