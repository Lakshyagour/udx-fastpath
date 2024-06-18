set(SRC_SOURCE_FILES main.cpp upf_gtp.cpp)

add_executable(${PROJECT_NAME} ${SRC_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC udx)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})
