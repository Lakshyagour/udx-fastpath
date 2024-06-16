
add_subdirectory(cndp)
add_subdirectory(dpdk)
add_subdirectory(tap)

set(CODE_FILES core.cpp master_worker.cpp run_to_completion.cpp)
add_library(core ${CODE_FILES})
target_link_libraries(core PUBLIC cndp dpdk tap jsoncpp log)
