add_subdirectory(alu_ops)

file(GLOB uarchsim_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
file(GLOB uarchsim_hdrs ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(
        721sim
        ${uarchsim_srcs}
        ${uarchsim_hdrs}
)

target_include_directories(721sim PRIVATE .)

target_link_libraries(
        721sim
        fesvr-static
        softfloat
        riscv
        uarchsim-alu-ops
)

target_compile_definitions(
        721sim
        PRIVATE
        RISCV_MICRO_CHECKER
        PREFIX="${AC_CONFIGURE_PREFIX}"
)

target_compile_options(
        721sim PRIVATE
        -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
)
