set(CODE_FILES cndp.cpp config.cpp stats.cpp)

add_library(cndp ${CODE_FILES})
target_link_libraries(cndp PUBLIC ${CNDP_LIBRARIES} pthread jsoncpp)
