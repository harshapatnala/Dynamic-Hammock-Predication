set(THREADS_PREFER_PTHREAD_FLAG ON)

if (NOT DEFINED AC_CONFIGURE_PREFIX)
    message(FATAL_ERROR "Variable AC_CONFIGURE_PREFIX is not set, give it a value so the macro PREFIX can be determined")
endif ()

set(
        fesvr_hdrs
        elf.h
        elfloader.h
        htif.h
        memif.h
        syscall.h
        packet.h
        context.h
        htif_pthread.h
        htif_rs232.h
        htif_hexwriter.h
        htif_eth.h
        htif_zedboard.h
        option_parser.h
        term.h
        device.h
        rfb.h
        base64.h
        strace.h
        target_cwd.h
        syscall_bypass.h
)

set(
        fesvr_srcs
        elfloader.cc
        htif.cc
        packet.cc
        memif.cc
        syscall.cc
        device.cc
        rfb.cc
        context.cc
        htif_pthread.cc
        htif_rs232.cc
        htif_hexwriter.cc
        htif_eth.cc
        htif_zedboard.cc
        dummy.cc
        option_parser.cc
        term.cc
        base64.cc
        target_cwd.cc
        syscall_bypass.cc
)

add_library(
        fesvr-static STATIC
        ${fesvr_srcs}
        ${fesvr_hdrs}
)
target_include_directories(fesvr-static INTERFACE ..)
target_include_directories(fesvr-static PUBLIC .)
target_compile_definitions(
        fesvr-static PRIVATE
        PREFIX="${AC_CONFIGURE_PREFIX}"
)

if ("${CMAKE_VERSION}" VERSION_GREATER "3.0.0")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(fesvr-static Threads::Threads)
else ()
    target_link_libraries(fesvr-static pthread)
endif ()
