pcem_private_api += files(
        '../../includes/private/codegen/codegen_accumulate.h',
        '../../includes/private/codegen/codegen_allocator.h',
        '../../includes/private/codegen/codegen_backend_arm64_defs.h',
        '../../includes/private/codegen/codegen_backend_arm64.h',
        '../../includes/private/codegen/codegen_backend_arm64_ops.h',
        '../../includes/private/codegen/codegen_backend_arm_defs.h',
        '../../includes/private/codegen/codegen_backend_arm.h',
        '../../includes/private/codegen/codegen_backend_arm_ops.h',
        '../../includes/private/codegen/codegen_backend.h',
        '../../includes/private/codegen/codegen_backend_x86-64_defs.h',
        '../../includes/private/codegen/codegen_backend_x86-64.h',
        '../../includes/private/codegen/codegen_backend_x86-64_ops.h',
        '../../includes/private/codegen/codegen_backend_x86-64_ops_helpers.h',
        '../../includes/private/codegen/codegen_backend_x86-64_ops_sse.h',
        '../../includes/private/codegen/codegen_backend_x86_defs.h',
        '../../includes/private/codegen/codegen_backend_x86.h',
        '../../includes/private/codegen/codegen_backend_x86_ops_fpu.h',
        '../../includes/private/codegen/codegen_backend_x86_ops.h',
        '../../includes/private/codegen/codegen_backend_x86_ops_helpers.h',
        '../../includes/private/codegen/codegen_backend_x86_ops_sse.h',
        '../../includes/private/codegen/codegen.h',
        '../../includes/private/codegen/codegen_ir_defs.h',
        '../../includes/private/codegen/codegen_ir.h',
        '../../includes/private/codegen/codegen_ops_3dnow.h',
        '../../includes/private/codegen/codegen_ops_arith.h',
        '../../includes/private/codegen/codegen_ops_branch.h',
        '../../includes/private/codegen/codegen_ops_fpu_arith.h',
        '../../includes/private/codegen/codegen_ops_fpu_constant.h',
        '../../includes/private/codegen/codegen_ops_fpu_loadstore.h',
        '../../includes/private/codegen/codegen_ops_fpu_misc.h',
        '../../includes/private/codegen/codegen_ops.h',
        '../../includes/private/codegen/codegen_ops_helpers.h',
        '../../includes/private/codegen/codegen_ops_jump.h',
        '../../includes/private/codegen/codegen_ops_logic.h',
        '../../includes/private/codegen/codegen_ops_misc.h',
        '../../includes/private/codegen/codegen_ops_mmx_arith.h',
        '../../includes/private/codegen/codegen_ops_mmx_cmp.h',
        '../../includes/private/codegen/codegen_ops_mmx_loadstore.h',
        '../../includes/private/codegen/codegen_ops_mmx_logic.h',
        '../../includes/private/codegen/codegen_ops_mmx_pack.h',
        '../../includes/private/codegen/codegen_ops_mmx_shift.h',
        '../../includes/private/codegen/codegen_ops_mov.h',
        '../../includes/private/codegen/codegen_ops_shift.h',
        '../../includes/private/codegen/codegen_ops_stack.h',
        '../../includes/private/codegen/codegen_reg.h',
        '../../includes/private/codegen/codegen_timing_common.h',
        '../../includes/private/codegen/codegen_x86-64.h',
        )

pcem_src += files(
        'codegen.c',
        'codegen_accumulate.c',
        'codegen_allocator.c',
        'codegen_block.c',
        'codegen_ir.c',
        'codegen_ops.c',
        'codegen_ops_3dnow.c',
        'codegen_ops_arith.c',
        'codegen_ops_branch.c',
        'codegen_ops_fpu_arith.c',
        'codegen_ops_fpu_constant.c',
        'codegen_ops_fpu_loadstore.c',
        'codegen_ops_fpu_misc.c',
        'codegen_ops_helpers.c',
        'codegen_ops_jump.c',
        'codegen_ops_logic.c',
        'codegen_ops_misc.c',
        'codegen_ops_mmx_arith.c',
        'codegen_ops_mmx_cmp.c',
        'codegen_ops_mmx_loadstore.c',
        'codegen_ops_mmx_logic.c',
        'codegen_ops_mmx_pack.c',
        'codegen_ops_mmx_shift.c',
        'codegen_ops_mov.c',
        'codegen_ops_shift.c',
        'codegen_ops_stack.c',
        'codegen_reg.c',
        'codegen_timing_486.c',
        'codegen_timing_686.c',
        'codegen_timing_common.c',
        'codegen_timing_cyrixiii.c',
        'codegen_timing_k6.c',
        'codegen_timing_p6.c',
        'codegen_timing_pentium.c',
        'codegen_timing_winchip.c',
        'codegen_timing_winchip2.c'
        )

if target_machine.cpu_family() == 'x86_64'
        pcem_src += files(
                'x86-64/codegen_backend_x86-64.c',
                'x86-64/codegen_backend_x86-64_ops.c',
                'x86-64/codegen_backend_x86-64_ops_sse.c',
                'x86-64/codegen_backend_x86-64_uops.c'
                )
endif

if target_machine.cpu_family() == 'x86'
        pcem_src += files(
                'x86/codegen_backend_x86.c',
                'x86/codegen_backend_x86_ops.c',
                'x86/codegen_backend_x86_ops_fpu.c',
                'x86/codegen_backend_x86_ops_sse.c',
                'x86/codegen_backend_x86_uops.c'
                )
endif

if target_machine.cpu_family() == 'arm'
        pcem_src += files(
                'arm32/codegen_backend_arm.c',
                'arm32/codegen_backend_arm_ops.c',
                'arm32/codegen_backend_arm_uops.c',
                )
endif

if target_machine.cpu_family() == 'aarch64'
        pcem_src += files(
                'arm64/codegen_backend_arm64.c',
                'arm64/codegen_backend_arm64_imm.c',
                'arm64/codegen_backend_arm64_ops.c',
                'arm64/codegen_backend_arm64_uops.c',
                )
endif