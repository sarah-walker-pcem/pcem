#define BLOCK_SIZE 0x4000
#define BLOCK_MASK 0x3fff
#define BLOCK_START 0

#define HASH_SIZE 0x20000
#define HASH_MASK 0x1ffff

#define HASH(l) ((l) & 0x1ffff)

enum
{
        OP_RET = 0xc3
};
