#define BLOCK_SIZE 0x4000
#define BLOCK_MASK 0x3fff
#define BLOCK_START 0

#define HASH_SIZE 0x20000
#define HASH_MASK 0x1ffff

#define HASH(l) ((l) & 0x1ffff)

/*Hack until better memory management written*/
#define BLOCK_EXIT_OFFSET 0xffe0
/*#define BLOCK_EXIT_OFFSET 0x7f0*/
#define BLOCK_GPF_OFFSET (BLOCK_EXIT_OFFSET - 20)

/*#define BLOCK_MAX 1720*/
#define BLOCK_MAX 65208
