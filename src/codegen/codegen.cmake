set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_accumulate.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_allocator.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm64_defs.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm64.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm64_ops.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm_defs.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_arm_ops.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86-64_defs.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86-64.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86-64_ops.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86-64_ops_helpers.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86-64_ops_sse.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86_defs.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86_ops_fpu.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86_ops.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86_ops_helpers.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_backend_x86_ops_sse.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ir_defs.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ir.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_3dnow.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_arith.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_branch.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_fpu_arith.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_fpu_constant.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_fpu_loadstore.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_fpu_misc.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_helpers.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_jump.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_logic.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_misc.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_arith.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_cmp.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_loadstore.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_logic.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_pack.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mmx_shift.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_mov.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_shift.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_ops_stack.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_reg.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_timing_common.h
        ${CMAKE_SOURCE_DIR}/includes/private/codegen/codegen_x86-64.h
        )

set(PCEM_SRC ${PCEM_SRC}
        codegen/codegen.c
        codegen/codegen_accumulate.c
        codegen/codegen_allocator.c
        codegen/codegen_block.c
        codegen/codegen_ir.c
        codegen/codegen_ops.c
        codegen/codegen_ops_3dnow.c
        codegen/codegen_ops_arith.c
        codegen/codegen_ops_branch.c
        codegen/codegen_ops_fpu_arith.c
        codegen/codegen_ops_fpu_constant.c
        codegen/codegen_ops_fpu_loadstore.c
        codegen/codegen_ops_fpu_misc.c
        codegen/codegen_ops_helpers.c
        codegen/codegen_ops_jump.c
        codegen/codegen_ops_logic.c
        codegen/codegen_ops_misc.c
        codegen/codegen_ops_mmx_arith.c
        codegen/codegen_ops_mmx_cmp.c
        codegen/codegen_ops_mmx_loadstore.c
        codegen/codegen_ops_mmx_logic.c
        codegen/codegen_ops_mmx_pack.c
        codegen/codegen_ops_mmx_shift.c
        codegen/codegen_ops_mov.c
        codegen/codegen_ops_shift.c
        codegen/codegen_ops_stack.c
        codegen/codegen_reg.c
        codegen/codegen_timing_486.c
        codegen/codegen_timing_686.c
        codegen/codegen_timing_common.c
        codegen/codegen_timing_cyrixiii.c
        codegen/codegen_timing_k6.c
        codegen/codegen_timing_p6.c
        codegen/codegen_timing_pentium.c
        codegen/codegen_timing_winchip.c
        codegen/codegen_timing_winchip2.c
        )

if(${PCEM_CPU_TYPE} STREQUAL "x86_64")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/x86-64/codegen_backend_x86-64.c
                codegen/x86-64/codegen_backend_x86-64_ops.c
                codegen/x86-64/codegen_backend_x86-64_ops_sse.c
                codegen/x86-64/codegen_backend_x86-64_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "i386")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/x86/codegen_backend_x86.c
                codegen/x86/codegen_backend_x86_ops.c
                codegen/x86/codegen_backend_x86_ops_fpu.c
                codegen/x86/codegen_backend_x86_ops_sse.c
                codegen/x86/codegen_backend_x86_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/arm32/codegen_backend_arm.c
                codegen/arm32/codegen_backend_arm_ops.c
                codegen/arm32/codegen_backend_arm_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm64")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/arm64/codegen_backend_arm64.c
                codegen/arm64/codegen_backend_arm64_imm.c
                codegen/arm64/codegen_backend_arm64_ops.c
                codegen/arm64/codegen_backend_arm64_uops.c
                )
endif()