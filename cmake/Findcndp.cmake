find_package(PkgConfig REQUIRED)
pkg_check_modules(CNDP REQUIRED libcndp)


find_package(PkgConfig REQUIRED)
pkg_check_modules(XDP REQUIRED libxdp>=1.2.0)
pkg_check_modules(BPF REQUIRED libbpf)

# Check for the availability of linux/if_xdp.h
include(CheckIncludeFile)
check_include_file(linux/if_xdp.h HAVE_LINUX_IF_XDP_H)

if (XDP_FOUND AND BPF_FOUND)
    include_directories(${XDP_INCLUDE_DIRS})
    include_directories(${BPF_INCLUDE_DIRS})
    # Check for additional headers
    include(CheckIncludeFile)
    check_include_file(xdp/xsk.h HAVE_XDP_XSK_H)
    check_include_file(bpf/bpf.h HAVE_BPF_BPF_H)

    if (HAVE_XDP_XSK_H AND HAVE_BPF_BPF_H)
        # Set configuration flags
        add_definitions(-DUSE_LIBXDP)
        add_definitions(-DHAS_XSK_UMEM_SHARED)

        # Link with libxdp and libbpf
        link_libraries(${XDP_LIBRARIES})
        link_libraries(${BPF_LIBRARIES})
    endif ()
endif ()
