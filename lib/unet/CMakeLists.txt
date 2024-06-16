
add_subdirectory(common)

add_library(unet INTERFACE)
target_link_libraries(unet INTERFACE common)
