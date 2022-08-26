# Get the location of riscv-base dir
get_target_property(riscv_base_dir riscv RISCV_BASE_DIR)

include(generating_rules.cmake)


add_library(
        uarchsim-alu-ops STATIC
        alu_ops.cc
        alu_ops.h
        ${alu_ops_gen_srcs}
        ${alu_ops_gen_hdrs}
)

target_link_libraries(uarchsim-alu-ops softfloat)

target_include_directories(uarchsim-alu-ops PUBLIC .)
target_include_directories(uarchsim-alu-ops PRIVATE ..)
target_include_directories(uarchsim-alu-ops PRIVATE ${riscv_base_dir})
target_include_directories(uarchsim-alu-ops PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
