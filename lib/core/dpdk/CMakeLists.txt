
set(SRC_SOURCE_FILES bpf_cmd.c  config.c  parameters.c  testpmd.h bpf_cmd.h dpdk.cpp  util.c)

add_library(dpdk ${SRC_SOURCE_FILES})
target_link_libraries(dpdk ${DPDK_LIBRARIES} ${DPDK_PMD} -lbsd)
target_compile_options(dpdk PRIVATE -Wno-deprecated-declarations -mssse3 -fpermissive)


