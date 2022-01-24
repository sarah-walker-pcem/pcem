set(PCEM_SRC_CODEGEN
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
        set(PCEM_SRC_CODEGEN ${PCEM_SRC_CODEGEN}
                codegen/x86-64/codegen_backend_x86-64.c
                codegen/x86-64/codegen_backend_x86-64_ops.c
                codegen/x86-64/codegen_backend_x86-64_ops_sse.c
                codegen/x86-64/codegen_backend_x86-64_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "i386")
        set(PCEM_SRC_CODEGEN ${PCEM_SRC_CODEGEN}
                codegen/x86/codegen_backend_x86.c
                codegen/x86/codegen_backend_x86_ops.c
                codegen/x86/codegen_backend_x86_ops_fpu.c
                codegen/x86/codegen_backend_x86_ops_sse.c
                codegen/x86/codegen_backend_x86_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm")
        set(PCEM_SRC_CODEGEN ${PCEM_SRC_CODEGEN}
                codegen/arm32/codegen_backend_arm.c
                codegen/arm32/codegen_backend_arm_ops.c
                codegen/arm32/codegen_backend_arm_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm64")
        set(PCEM_SRC_CODEGEN ${PCEM_SRC_CODEGEN}
                codegen/arm64/codegen_backend_arm64.c
                codegen/arm64/codegen_backend_arm64_imm.c
                codegen/arm64/codegen_backend_arm64_ops.c
                codegen/arm64/codegen_backend_arm64_uops.c
                )
endif()