typedef int (*OpFn)(uint32_t fetchdat);

#ifdef DYNAREC
void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f, OpFn *dynarec_opcodes, OpFn *dynarec_opcodes_0f);

extern OpFn *x86_dynarec_opcodes;
extern OpFn *x86_dynarec_opcodes_0f;

extern OpFn dynarec_ops_286[1024];
extern OpFn dynarec_ops_286_0f[1024];

extern OpFn dynarec_ops_386[1024];
extern OpFn dynarec_ops_386_0f[1024];

extern OpFn dynarec_ops_winchip_0f[1024];

#else
void x86_setopcodes(OpFn *opcodes, OpFn *opcodes_0f);
#endif

extern OpFn *x86_opcodes;
extern OpFn *x86_opcodes_0f;

extern OpFn ops_286[1024];
extern OpFn ops_286_0f[1024];

extern OpFn ops_386[1024];
extern OpFn ops_386_0f[1024];

extern OpFn ops_winchip_0f[1024];
