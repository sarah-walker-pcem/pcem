#include "codegen_backend_x86-64_defs.h"

#define BLOCK_SIZE 0x4000
#define BLOCK_MASK 0x3fff
#define BLOCK_START 40

#define HASH_SIZE 0x20000
#define HASH_MASK 0x1ffff

#define HASH(l) ((l) & 0x1ffff)

#define BLOCK_EXIT_OFFSET 20
#define BLOCK_GPF_OFFSET 0

#define BLOCK_MAX 0x3c0
