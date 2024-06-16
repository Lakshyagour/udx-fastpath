include_directories(${UNET_INCLUDE_DIR}/common)
include_directories(${CORE_INCLUDE_DIR}/dpdk)
include_directories(${CORE_INCLUDE_DIR}/cndp)
include_directories(${CORE_INCLUDE_DIR}/tap)

add_subdirectory(unet)
add_subdirectory(core)
add_subdirectory(utility)

add_library(udx INTERFACE)
target_link_libraries(udx INTERFACE utility core unet jsoncpp)
