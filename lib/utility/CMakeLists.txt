# add_subdirectory(shm)
add_subdirectory(log)

add_library(utility INTERFACE)
target_link_libraries(utility INTERFACE log)
