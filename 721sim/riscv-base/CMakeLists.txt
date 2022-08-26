# RISCV
add_subdirectory(softfloat)
add_subdirectory(fesvr)

include(./generating_rules.cmake)

set(
        riscv_hdrs
        htif.h
        common.h
        decode.h
        mmu.h
        processor.h
        trap.h
        encoding.h
        cachesim.h
        memtracer.h
        extension.h
        rocc.h
        insn_template.h
        mulhi.h
        bbtracker.h
        gzstream.h
        ${riscv_gen_hdrs}
)

set(
        riscv_srcs
        htif.cc
        processor.cc
        interactive.cc
        trap.cc
        cachesim.cc
        mmu.cc
        spike_main/disasm.cc
        extension.cc
        spike_main/extensions.cc
        rocc.cc
        regnames.cc
        bbtracker.cc
        gzstream.cc
        ${riscv_gen_srcs}
)

add_library(
        riscv STATIC
        ${riscv_srcs}
        ${riscv_hdrs}
)

target_compile_definitions(
        riscv
        PRIVATE
        RISCV_MICRO_CHECKER
        PREFIX="${AC_CONFIGURE_PREFIX}"
)

target_include_directories(riscv PUBLIC .)
target_include_directories(riscv PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(riscv PRIVATE ../uarchsim)

target_link_libraries(riscv ${CMAKE_DL_LIBS})
target_link_libraries(riscv fesvr-static)
target_link_libraries(riscv softfloat)

if ("${CMAKE_VERSION}" VERSION_GREATER "3.0.0")
    find_package(ZLIB)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(riscv ZLIB::ZLIB)
    target_link_libraries(riscv Threads::Threads)
else ()
    target_link_libraries(riscv z)
    target_link_libraries(riscv pthread)
endif ()

set_target_properties(riscv PROPERTIES RISCV_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
