extern int firstline_draw, lastline_draw;
extern int displine;
extern int sc;

extern uint32_t ma, ca;
extern int con, cursoron, cgablink;

extern int scrollcache;

extern uint8_t edatlookup[4][4];

void svga_render_blank();
void svga_render_text_40();
void svga_render_text_80();

void svga_render_2bpp_lowres();
void svga_render_4bpp_lowres();
void svga_render_4bpp_highres();
void svga_render_8bpp_lowres();
void svga_render_8bpp_highres();
void svga_render_15bpp_lowres();
void svga_render_15bpp_highres();
void svga_render_16bpp_lowres();
void svga_render_16bpp_highres();
void svga_render_24bpp_lowres();
void svga_render_24bpp_highres();
void svga_render_32bpp_lowres();
void svga_render_32bpp_highres();

extern void (*svga_render)();
