#include "ibm.h"
#include "codegen.h"
#include "codegen_ir.h"
#include "codegen_ops.h"
#include "codegen_ops_arith.h"
#include "codegen_ops_logic.h"
#include "codegen_ops_mov.h"

RecompOpFn recomp_opcodes[512] =
{
        /*16-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/
/*00*/  ropADD_b_rmw,   ropADD_w_rmw,   ropADD_b_rm,    ropADD_w_rm,    ropADD_AL_imm,  ropADD_AX_imm,  NULL,           NULL,           ropOR_b_rmw,    ropOR_w_rmw,    ropOR_b_rm,     ropOR_w_rm,     ropOR_AL_imm,   ropOR_AX_imm,   NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropAND_b_rmw,   ropAND_w_rmw,   ropAND_b_rm,    ropAND_w_rm,    ropAND_AL_imm,  ropAND_AX_imm,  NULL,           NULL,           ropSUB_b_rmw,   ropSUB_w_rmw,   ropSUB_b_rm,    ropSUB_w_rm,    ropSUB_AL_imm,  ropSUB_AX_imm,  NULL,           NULL,
/*30*/  ropXOR_b_rmw,   ropXOR_w_rmw,   ropXOR_b_rm,    ropXOR_w_rm,    ropXOR_AL_imm,  ropXOR_AX_imm,  NULL,           NULL,           ropCMP_b_rmw,   ropCMP_w_rmw,   ropCMP_b_rm,    ropCMP_w_rm,    ropCMP_AL_imm,  ropCMP_AX_imm,  NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  rop80,          rop81_w,        rop80,          rop83_w,        NULL,           NULL,           NULL,           NULL,           ropMOV_b_r,     ropMOV_w_r,     ropMOV_r_b,     ropMOV_r_w,     NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropMOV_AL_abs,  ropMOV_AX_abs,  ropMOV_abs_AL,  ropMOV_abs_AX,  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,  ropMOV_rw_imm,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOV_b_imm,   ropMOV_w_imm,   NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

        /*32-bit data*/
/*      00              01              02              03              04              05              06              07              08              09              0a              0b              0c              0d              0e              0f*/
/*00*/  ropADD_b_rmw,   ropADD_l_rmw,   ropADD_b_rm,    ropADD_l_rm,    ropADD_AL_imm,  ropADD_EAX_imm, NULL,           NULL,           ropOR_b_rmw,    ropOR_l_rmw,    ropOR_b_rm,     ropOR_l_rm,     ropOR_AL_imm,   ropOR_EAX_imm,  NULL,           NULL,
/*10*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*20*/  ropAND_b_rmw,   ropAND_l_rmw,   ropAND_b_rm,    ropAND_l_rm,    ropAND_AL_imm,  ropAND_EAX_imm, NULL,           NULL,           ropSUB_b_rmw,   ropSUB_l_rmw,   ropSUB_b_rm,    ropSUB_l_rm,    ropSUB_AL_imm,  ropSUB_EAX_imm, NULL,           NULL,
/*30*/  ropXOR_b_rmw,   ropXOR_l_rmw,   ropXOR_b_rm,    ropXOR_l_rm,    ropXOR_AL_imm,  ropXOR_EAX_imm, NULL,           NULL,           ropCMP_b_rmw,   ropCMP_l_rmw,   ropCMP_b_rm,    ropCMP_l_rm,    ropCMP_AL_imm,  ropCMP_EAX_imm, NULL,           NULL,

/*40*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*50*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*60*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*70*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,

/*80*/  rop80,          rop81_l,        rop80,          rop83_l,        NULL,           NULL,           NULL,           NULL,           ropMOV_b_r,     ropMOV_l_r,     ropMOV_r_b,     ropMOV_r_l,     NULL,           NULL,           NULL,           NULL,
/*90*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*a0*/  ropMOV_AL_abs,  ropMOV_EAX_abs, ropMOV_abs_AL,  ropMOV_abs_EAX, NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*b0*/  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rb_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,  ropMOV_rl_imm,

/*c0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           ropMOV_b_imm,   ropMOV_l_imm,   NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*d0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*e0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,
/*f0*/  NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL,           NULL
};
