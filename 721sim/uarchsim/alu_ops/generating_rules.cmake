# Obtaining a list of instructions that should be processed by the ALU
file(READ alu_insn.list riscv_alu_insn_list_raw)
string(REPLACE "\n" ";" riscv_alu_insn_list ${riscv_alu_insn_list_raw})

# Print the registered ALU instructions
string(REPLACE ";" " " space_sep_insn_list "${riscv_alu_insn_list}")
message("alu-ops will handle: ${space_sep_insn_list}")

# Generate .cc for each ALU operation
macro(riscv_alu_op_srcs_generator outfiles)
    foreach (i_basename ${ARGN})
        set(aop_out ${CMAKE_CURRENT_BINARY_DIR}/alu_op_${i_basename}.cc)
        add_custom_command(
                DEPENDS ${riscv_base_dir}/encoding.h ${CMAKE_CURRENT_SOURCE_DIR}/alu_op_template.cc
                OUTPUT ${aop_out}
                VERBATIM
                COMMAND bash -c "sed 's/NAME/${i_basename}/' ${CMAKE_CURRENT_SOURCE_DIR}/alu_op_template.cc | sed \"s/OPCODE/$(grep '^DECLARE_INSN.*\\<${i_basename}\\>' ${riscv_base_dir}/encoding.h | sed 's/DECLARE_INSN(.*,\\(.*\\),.*)/\\1/' )/\" > ${aop_out}"
        )
        set(${outfiles} ${${outfiles}} ${aop_out})
    endforeach (i_basename)
endmacro(riscv_alu_op_srcs_generator)
riscv_alu_op_srcs_generator(alu_ops_gen_src_list ${riscv_alu_insn_list})


# Generate the a header containing only the macros declaring ALU related ops
set(alu_ops_declare_h ${CMAKE_CURRENT_BINARY_DIR}/alu_ops_declare.h)

add_custom_command(
        DEPENDS ${riscv_base_dir}/encoding.h ${CMAKE_CURRENT_SOURCE_DIR}/alu_insn.list
        OUTPUT ${alu_ops_declare_h}
        VERBATIM
        COMMAND bash -c "echo '#ifdef DECLARE_INSN' > ${alu_ops_declare_h}; while read -r i; do grep \"^DECLARE_INSN.*\\<\${i}\\>\" ${riscv_base_dir}/encoding.h >> ${alu_ops_declare_h} ; done < ${CMAKE_CURRENT_SOURCE_DIR}/alu_insn.list ; echo '#endif' >> ${alu_ops_declare_h} ;"
)

# Passing the manifest of generated sources with lists alu_ops_gen_srcs & alu_ops_gen_hdrs
set(
        alu_ops_gen_srcs
        ${alu_ops_gen_src_list}
)

set(
        alu_ops_gen_hdrs
        ${alu_ops_declare_h}
)
