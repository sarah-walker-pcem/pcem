#ifdef DYNAREC

#include "ibm.h"
#include "x86.h"
#include "x86_ops.h"
#include "x86_flags.h"
#include "x87.h"
#include "386_common.h"
#include "cpu.h"
#include "codegen.h"
#include "codegen_ops.h"
#include "codegen_ops_x86.h"

#include "codegen_ops_arith.h"
#include "codegen_ops_fpu.h"
#include "codegen_ops_jump.h"
#include "codegen_ops_logic.h"
#include "codegen_ops_mov.h"
#include "codegen_ops_shift.h"
#include "codegen_ops_stack.h"
#include "codegen_ops_xchg.h"

RecompOpFn recomp_opcodes[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropADD_b_rmw,   ropADD_w_rmw,   ropADD_b_rm,    ropADD_w_rm,    ropADD_AL_imm,  ropADD_AX_imm,  NULL,           NULL,           ropOR_b_rmw,    ropOR_w_rmw,    ropOR_b_rm,     ropOR_w_rm,     ropOR_AL_imm,   ropOR_AX_imm,   NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropAND_b_rmw,   ropAND_w_rmw,   ropAND_b_rm,    ropAND_w_rm,    ropAND_AL_imm,  ropAND_AX_imm,  NULL,           NULL,           ropSUB_b_rmw,   ropSUB_w_rmw,   ropSUB_b_rm,    ropSUB_w_rm,    ropSUB_AL_imm,  ropSUB_AX_imm,  NULL,           NULL,
/*30*/  ropXOR_b_rmw,   ropXOR_w_rmw,   ropXOR_b_rm,    ropXOR_w_rm,    ropXOR_AL_imm,  ropXOR_AX_imm,  NULL,           NULL,           ropCMP_b_rmw,   ropCMP_w_rmw,   ropCMP_b_rm,    ropCMP_w_rm,    ropCMP_AL_imm,  ropCMP_AX_imm,  NULL,           NULL,

/*40*/  ropINC_rw,      ropINC_rw,      ropINC_rw,      ropINC_rw,      ropINC_rw,      ropINC_rw,      ropINC_rw,      ropINC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,      ropDEC_rw,
/*50*/  ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPUSH_16,     ropPOP_16,      ropPOP_16,      ropPOP_16,      ropPOP_16,      ropPOP_16,      ropPOP_16,      ropPOP_16,      ropPOP_16,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropPUSH_imm_16, NULL,           ropPUSH_imm_b16,NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  ropJO,          ropJNO,         ropJB,          ropJNB,         ropJE,          ropJNE,         ropJBE,         ropJNBE,        ropJS,          ropJNS,         ropJP,          ropJNP,         ropJL,          ropJNL,         ropJLE,         ropJNLE,

/*80*/  rop80,          rop81_w,        rop80,          rop83_w,        ropTEST_b_rm,   ropTEST_w_rm,   ropXCHG_b,      ropXCHG_w,      ropMOV_b_r,     ropMOV_w_r,     ropMOV_r_b,     ropMOV_r_w,     NULL,           ropLEA_w,       NULL,           NULL,
/*90*/  NULL,           ropXCHG_AX_CX,  ropXCHG_AX_DX,  ropXCHG_AX_BX,  ropXCHG_AX_SP,  ropXCHG_AX_BP,  ropXCHG_AX_SI,  ropXCHG_AX_DI,  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropMOV_AL_a,    ropMOV_AX_a,    ropMOV_a_AL,    ropMOV_a_AX,    NULL,           NULL,           NULL,           NULL,           ropTEST_AL_imm, ropTEST_AX_imm, NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,

/*c0*/  ropC0,          ropC1_w,        ropRET_imm_16,  ropRET_16,      NULL,           NULL,           ropMOV_b_imm,   ropMOV_w_imm,   NULL,           ropLEAVE_16,    NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  ropD0,          ropD1_w,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           ropLOOP,        ropJCXZ,        NULL,           NULL,           NULL,           NULL,           ropCALL_r16,    ropJMP_r16,     NULL,           ropJMP_r8,      NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropF6,          ropF7_w,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropADD_b_rmw,   ropADD_l_rmw,   ropADD_b_rm,    ropADD_l_rm,    ropADD_AL_imm,  ropADD_EAX_imm, NULL,           NULL,           ropOR_b_rmw,    ropOR_l_rmw,    ropOR_b_rm,     ropOR_l_rm,     ropOR_AL_imm,   ropOR_EAX_imm,  NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropAND_b_rmw,   ropAND_l_rmw,   ropAND_b_rm,    ropAND_l_rm,    ropAND_AL_imm,  ropAND_EAX_imm, NULL,           NULL,           ropSUB_b_rmw,   ropSUB_l_rmw,   ropSUB_b_rm,    ropSUB_l_rm,    ropSUB_AL_imm,  ropSUB_EAX_imm, NULL,           NULL,
/*30*/  ropXOR_b_rmw,   ropXOR_l_rmw,   ropXOR_b_rm,    ropXOR_l_rm,    ropXOR_AL_imm,  ropXOR_EAX_imm, NULL,           NULL,           ropCMP_b_rmw,   ropCMP_l_rmw,   ropCMP_b_rm,    ropCMP_l_rm,    ropCMP_AL_imm,  ropCMP_EAX_imm, NULL,           NULL,

/*40*/  ropINC_rl,      ropINC_rl,      ropINC_rl,      ropINC_rl,      ropINC_rl,      ropINC_rl,      ropINC_rl,      ropINC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,      ropDEC_rl,
/*50*/  ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPUSH_32,     ropPOP_32,      ropPOP_32,      ropPOP_32,      ropPOP_32,      ropPOP_32,      ropPOP_32,      ropPOP_32,      ropPOP_32,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropPUSH_imm_32, NULL,           ropPUSH_imm_b32,NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  ropJO,          ropJNO,         ropJB,          ropJNB,         ropJE,          ropJNE,         ropJBE,         ropJNBE,        ropJS,          ropJNS,         ropJP,          ropJNP,         ropJL,          ropJNL,         ropJLE,         ropJNLE,

/*80*/  rop80,          rop81_l,        rop80,          rop83_l,        ropTEST_b_rm,   ropTEST_l_rm,   ropXCHG_b,      ropXCHG_l,      ropMOV_b_r,     ropMOV_l_r,     ropMOV_r_b,     ropMOV_r_l,     NULL,           ropLEA_l,       NULL,           NULL,
/*90*/  NULL,           ropXCHG_EAX_ECX,ropXCHG_EAX_EDX,ropXCHG_EAX_EBX,ropXCHG_EAX_ESP,ropXCHG_EAX_EBP,ropXCHG_EAX_ESI,ropXCHG_EAX_EDI,NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropMOV_AL_a,    ropMOV_EAX_a,   ropMOV_a_AL,    ropMOV_a_EAX,   NULL,           NULL,           NULL,           NULL,           ropTEST_AL_imm, ropTEST_EAX_imm,NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,

/*c0*/  ropC0,          ropC1_l,        ropRET_imm_32,  ropRET_32,      NULL,           NULL,           ropMOV_b_imm,   ropMOV_l_imm,   NULL,           ropLEAVE_32,    NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  ropD0,          ropD1_l,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           ropLOOP,        ropJCXZ,        NULL,           NULL,           NULL,           NULL,           ropCALL_r32,    ropJMP_r32,     NULL,           ropJMP_r8,      NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropF6,          ropF7_l,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_0f[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropJO_w,        ropJNO_w,       ropJB_w,        ropJNB_w,       ropJE_w,        ropJNE_w,       ropJBE_w,       ropJNBE_w,      ropJS_w,        ropJNS_w,       ropJP_w,        ropJNP_w,       ropJL_w,        ropJNL_w,       ropJLE_w,       ropJNLE_w,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOVZX_w_b,   NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOVSX_w_b,   NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropJO_l,        ropJNO_l,       ropJB_l,        ropJNB_l,       ropJE_l,        ropJNE_l,       ropJBE_l,       ropJNBE_l,      ropJS_l,        ropJNS_l,       ropJP_l,        ropJNP_l,       ropJL_l,        ropJNL_l,       ropJLE_l,       ropJNLE_l,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOVZX_l_b,   ropMOVZX_l_w,   NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOVSX_l_b,   ropMOVSX_l_w,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};


RecompOpFn recomp_opcodes_d8[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFADDs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,       ropFMULs,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       ropFSUBs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       ropFDIVs,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFADD,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,        ropFMUL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        ropFSUB,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        ropFDIV,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_d9[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        ropFLDs,        NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTs,        ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,       ropFSTPs,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFLD,         ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,        ropFXCH,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_db[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*80*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       ropFILDl,       NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTl,       ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,      ropFISTPl,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_dc[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFADDr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,       ropFMULr,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_dd[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFST,         ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,        ropFSTP,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_de[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFADDP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,       ropFMULP,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

RecompOpFn recomp_opcodes_df[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  ropFSTSW_AX,    NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/        
/*00*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*30*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  ropFSTSW_AX,    NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
};

#endif
