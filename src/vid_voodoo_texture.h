static const uint32_t texture_offset[LOD_MAX+3] =
{
        0,
        256*256,
        256*256 + 128*128,
        256*256 + 128*128 + 64*64,
        256*256 + 128*128 + 64*64 + 32*32,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2 + 1*1,
        256*256 + 128*128 + 64*64 + 32*32 + 16*16 + 8*8 + 4*4 + 2*2 + 1*1 + 1
};

void voodoo_recalc_tex(voodoo_t *voodoo, int tmu);
void voodoo_use_texture(voodoo_t *voodoo, voodoo_params_t *params, int tmu);
void voodoo_tex_writel(uint32_t addr, uint32_t val, void *p);
void flush_texture_cache(voodoo_t *voodoo, uint32_t dirty_addr, int tmu);
