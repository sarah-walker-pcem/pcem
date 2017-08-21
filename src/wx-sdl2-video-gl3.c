#include <SDL2/SDL.h>
#define BITMAP WINDOWS_BITMAP
#include <SDL2/SDL_opengl.h>
#if SDL_VERSION_ATLEAST(2, 0, 4)
#include <SDL2/SDL_opengl_glext.h>
#endif
#undef BITMAP
#include "wx-sdl2-glw.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "video.h"
#include "wx-sdl2-video.h"
#include "wx-sdl2-video-renderer.h"
#include "ibm.h"
#include "wx-glslp-parser.h"
#include "wx-utils.h"
#include "config.h"
#include "wx-glsl.h"

extern char* get_filename(char*);

extern int take_screenshot;
extern void screenshot_taken(unsigned char* rgb, int width, int height);

#define SCALE_SOURCE 0
#define SCALE_VIEWPORT 1
#define SCALE_ABSOLUTE 2

float gl3_shader_refresh_rate = 0;
float gl3_input_scale = 1.0f;
int gl3_input_stretch = FULLSCR_SCALE_FULL;
char gl3_shader_file[MAX_USER_SHADERS][512];

static int max_texture_size;

static SDL_GLContext context = NULL;
static struct shader_texture scene_texture;

static glsl_t* active_shader;

static glw_t* glw;

static GLfloat matrix[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
};


extern int video_scale_mode;
extern int video_vsync;
extern int video_focus_dim;
extern int video_refresh_rate;

static int glsl_version[2];

const char* vertex_shader_default_tex_src =
        "#version 130\n"
        "\n"
        "in vec4 VertexCoord;\n"
        "in vec2 TexCoord;\n"
        "\n"
        "out vec2 texCoord;\n"
        "\n"
        "void main()\n"
        "{\n"
        "       gl_Position = VertexCoord;\n"
        "       texCoord = TexCoord;\n"
        "}\n";

const char* fragment_shader_default_tex_src =
        "#version 130\n"
        "\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D Texture;\n"
        "\n"
        "out vec4 color;"
        "\n"
        "void main()\n"
        "{\n"
        "       color = texture(Texture, texCoord);\n"
        "}\n";

const char* vertex_shader_default_color_src =
        "#version 130\n"
        "\n"
        "in vec4 VertexCoord;\n"
        "in vec4 Color;\n"
        "\n"
        "out vec4 color;\n"
        "\n"
        "void main()\n"
        "{\n"
        "       gl_Position = VertexCoord;\n"
        "       color = Color;\n"
        "}\n";

const char* fragment_shader_default_color_src =
        "#version 130\n"
        "\n"
        "in vec4 color;\n"
        "\n"
        "out vec4 outColor;"
        "\n"
        "void main()\n"
        "{\n"
        "       outColor = color;\n"
        "}\n";

static int next_pow2(unsigned int n)
{
        n--;
        n |= n >> 1;   // Divide by 2^k for consecutive doublings of k up to 32,
        n |= n >> 2;   // and then or the results.
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n++;

        return n;
}

static int compile_shader(GLenum shader_type, const char* prepend, const char* program, int* dst)
{
        const char* source[3];
        char version[50];
        int ver = 0;
        char *version_loc = strstr(program, "#version");
        if (version_loc)
                ver = (int)strtol(version_loc + 8, (char**)&program, 10);
        else
        {
                ver = glsl_version[0] * 100 + glsl_version[1] * 10;
                if (ver == 300)
                        ver = 130;
                else if (ver == 310)
                        ver = 140;
                else if (ver == 320)
                        ver = 150;
        }
        sprintf(version, "#version %d\n", ver);
        source[0] = version;
        source[1] = prepend ? prepend : "";
        source[2] = program;

        pclog("GLSL version %d\n", ver);

        GLuint shader = glw->glCreateShader(shader_type);
        glw->glShaderSource(shader, 3, source, NULL);
        glw->glCompileShader(shader);

        GLint status = 0;
        glw->glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status)
        {
                GLint length;
                glw->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                char* log = malloc(length);
                glw->glGetShaderInfoLog(shader, length, &length, log);
                wx_simple_messagebox("GLSL Error", "Could not compile shader:\n%s", log);
                pclog("Could not compile shader: %s\n", log);
//                pclog("Shader: %s\n", program);

                free(log);
                return 0;
        }

        *dst = shader;

        return 1;
}

static int create_program(struct shader_program* program)
{
        GLint status;
        program->id = glw->glCreateProgram();
        glw->glAttachShader(program->id, program->vertex_shader);
        glw->glAttachShader(program->id, program->fragment_shader);

        glw->glLinkProgram(program->id);

        glw->glDeleteShader(program->vertex_shader);
        glw->glDeleteShader(program->fragment_shader);

        program->vertex_shader = program->fragment_shader = 0;

        glw->glGetProgramiv(program->id, GL_LINK_STATUS, &status);

        if (!status)
        {
                int maxLength;
                int length;
                glw->glGetProgramiv(program->id, GL_INFO_LOG_LENGTH, &maxLength);
                char* log = malloc(maxLength);
                glw->glGetProgramInfoLog(program->id, maxLength, &length, log);
                wx_simple_messagebox("GLSL Error", "Program not linked:\n%s", log);
                free(log);
                return 0;
        }

        return 1;
}

static GLuint get_uniform(GLuint program, const char* name)
{
        return glw->glGetUniformLocation(program, name);
}

static GLuint get_attrib(GLuint program, const char* name)
{
        return glw->glGetAttribLocation(program, name);
}

static void find_uniforms(struct glsl_shader* glsl, int num_pass)
{
        int i;
        char s[50];
        struct shader_pass* pass = &glsl->passes[num_pass];
        int p = pass->program.id;
        glw->glUseProgram(p);

        struct shader_uniforms* u = &pass->uniforms;

        u->mvp_matrix = get_uniform(p, "MVPMatrix");
        u->vertex_coord = get_attrib(p, "VertexCoord");
        u->tex_coord = get_attrib(p, "TexCoord");
        u->color = get_attrib(p, "Color");

        u->frame_count = get_uniform(p, "FrameCount");
        u->frame_direction = get_uniform(p, "FrameDirection");

        u->texture = get_uniform(p, "Texture");
        u->input_size = get_uniform(p, "InputSize");
        u->texture_size = get_uniform(p, "TextureSize");
        u->output_size = get_uniform(p, "OutputSize");

        u->orig.texture = get_uniform(p, "OrigTexture");
        u->orig.input_size = get_uniform(p, "OrigInputSize");
        u->orig.texture_size = get_uniform(p, "OrigTextureSize");

        for (i = 0; i < glsl->num_passes; ++i)
        {
                sprintf(s, "Pass%dTexture", (i+1));
                u->pass[i].texture = get_uniform(p, s);
                sprintf(s, "Pass%dInputSize", (i+1));
                u->pass[i].input_size = get_uniform(p, s);
                sprintf(s, "Pass%dTextureSize", (i+1));
                u->pass[i].texture_size = get_uniform(p, s);

                sprintf(s, "PassPrev%dTexture", num_pass-i);
                u->prev_pass[i].texture = get_uniform(p, s);
                sprintf(s, "PassPrev%dInputSize", num_pass-i);
                u->prev_pass[i].input_size = get_uniform(p, s);
                sprintf(s, "PassPrev%dTextureSize", num_pass-i);
                u->prev_pass[i].texture_size = get_uniform(p, s);
        }

        u->prev[0].texture = get_uniform(p, "PrevTexture");
        u->prev[0].tex_coord = get_attrib(p, "PrevTexCoord");
        for (i = 1; i < MAX_PREV; ++i)
        {
                sprintf(s, "Prev%dTexture", i);
                u->prev[i].texture = get_uniform(p, s);
                sprintf(s, "Prev%dTexCoord", i);
                u->prev[i].tex_coord = get_attrib(p, s);
        }
        for (i = 0; i < MAX_PREV; ++i)
                if (u->prev[i].texture >= 0) glsl->has_prev = 1;

        for (i = 0; i < glsl->num_lut_textures; ++i)
                u->lut_textures[i] = get_uniform(p, glsl->lut_textures[i].name);

        for (i = 0; i < glsl->num_parameters; ++i)
                u->parameters[i] = get_uniform(p, glsl->parameters[i].id);

        glw->glUseProgram(0);
}

static void set_scale_mode(char* scale, int* dst)
{
        if (!strcmp(scale, "viewport"))
                *dst = SCALE_VIEWPORT;
        else if (!strcmp(scale, "absolute"))
                *dst = SCALE_ABSOLUTE;
        else
                *dst = SCALE_SOURCE;
}

static void setup_scale(struct shader* shader, struct shader_pass* pass)
{
        set_scale_mode(shader->scale_type_x, &pass->scale.mode[0]);
        set_scale_mode(shader->scale_type_y, &pass->scale.mode[1]);
        pass->scale.value[0] = shader->scale_x;
        pass->scale.value[1] = shader->scale_y;
}

static void create_texture(struct shader_texture* tex)
{
        if (tex->width > max_texture_size)
                tex->width = max_texture_size;
        if (tex->height > max_texture_size)
                tex->height = max_texture_size;
        pclog("Create texture with size %dx%d\n", tex->width, tex->height);
        glGenTextures(1, (GLuint *)&tex->id);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex->wrap_mode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex->wrap_mode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex->min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex->mag_filter);
        glTexImage2D(GL_TEXTURE_2D, 0, tex->internal_format, tex->width, tex->height, 0, tex->format, tex->type, tex->data);
        if (tex->mipmap)
                glw->glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
}

static void delete_texture(struct shader_texture* tex)
{
        if (tex->id > 0)
                glDeleteTextures(1, (GLuint *)&tex->id);
        tex->id = 0;
}

static void delete_fbo(struct shader_fbo* fbo)
{
        if (fbo->id >= 0)
        {
                glw->glDeleteFramebuffers(1, (GLuint *)&fbo->id);
                delete_texture(&fbo->texture);
        }
}

static void delete_program(struct shader_program* program)
{
        if (program->vertex_shader) glw->glDeleteShader(program->vertex_shader);
        if (program->fragment_shader) glw->glDeleteShader(program->fragment_shader);
        glw->glDeleteProgram(program->id);
}

static void delete_vbo(struct shader_vbo* vbo)
{
        if (vbo->color >= 0) glw->glDeleteBuffers(1, (GLuint *)&vbo->color);
        glw->glDeleteBuffers(1, (GLuint *)&vbo->vertex_coord);
        glw->glDeleteBuffers(1, (GLuint *)&vbo->tex_coord);
}

static void delete_pass(struct shader_pass* pass)
{
        delete_fbo(&pass->fbo);
        delete_vbo(&pass->vbo);
        delete_program(&pass->program);
        glw->glDeleteVertexArrays(1, (GLuint *)&pass->vertex_array);
}

static void delete_prev(struct shader_prev* prev)
{
        delete_fbo(&prev->fbo);
        delete_vbo(&prev->vbo);
}

static void delete_shader(struct glsl_shader* glsl)
{
        int i;
        for (i = 0; i < glsl->num_passes; ++i)
                delete_pass(&glsl->passes[i]);
        if (glsl->has_prev)
        {
                delete_pass(&glsl->prev_scene);
                for (i = 0; i < MAX_PREV; ++i)
                        delete_prev(&glsl->prev[i]);
        }
        for (i = 0; i < glsl->num_lut_textures; ++i)
                delete_texture(&glsl->lut_textures[i].texture);
}

static void delete_glsl(glsl_t* glsl)
{
        int i;
        for (i = 0; i < glsl->num_shaders; ++i)
                delete_shader(&glsl->shaders[i]);
        delete_pass(&glsl->scene);
        delete_pass(&glsl->fs_color);
        delete_pass(&glsl->final_pass);
#ifdef SDL2_SHADER_DEBUG
        delete_pass(&glsl->debug);
#endif
}

static void create_fbo(struct shader_fbo* fbo)
{
        create_texture(&fbo->texture);

        glw->glGenFramebuffers(1, (GLuint *)&fbo->id);
        glw->glBindFramebuffer(GL_FRAMEBUFFER, fbo->id);
        glw->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->texture.id, 0);

        if (glw->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                pclog("Could not create framebuffer!\n");

        glw->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void setup_fbo(struct shader* shader, struct shader_fbo* fbo)
{
        fbo->texture.internal_format = GL_RGBA8;
        fbo->texture.format = GL_RGBA;
        fbo->texture.min_filter = fbo->texture.mag_filter = shader->filter_linear ? GL_LINEAR : GL_NEAREST;
        fbo->texture.width = 1024;
        fbo->texture.height = 1024;
        fbo->texture.type = GL_UNSIGNED_BYTE;
        if (!strcmp(shader->wrap_mode, "repeat"))
                fbo->texture.wrap_mode = GL_REPEAT;
        else if (!strcmp(shader->wrap_mode, "mirrored_repeat"))
                fbo->texture.wrap_mode = GL_MIRRORED_REPEAT;
        else if (!strcmp(shader->wrap_mode, "clamp_to_edge"))
                fbo->texture.wrap_mode = GL_CLAMP_TO_EDGE;
        else
                fbo->texture.wrap_mode = GL_CLAMP_TO_BORDER;
        fbo->srgb = 0;
        if (shader->srgb_framebuffer)
        {
                fbo->texture.internal_format = GL_SRGB8_ALPHA8;
                fbo->srgb = 1;
        }
        else if (shader->float_framebuffer)
        {
                fbo->texture.internal_format = GL_RGBA32F;
                fbo->texture.type = GL_FLOAT;
        }

        if (fbo->texture.mipmap)
                fbo->texture.min_filter = shader->filter_linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

        create_fbo(fbo);
}

static void recreate_fbo(struct shader_fbo* fbo, int width, int height)
{
        if (width != fbo->texture.width || height != fbo->texture.height)
        {
                glw->glDeleteFramebuffers(1, (GLuint *)&fbo->id);
                glDeleteTextures(1, (GLuint *)&fbo->texture.id);
                fbo->texture.width = width;
                fbo->texture.height = height;
                create_fbo(fbo);
        }
}

static int create_default_shader_tex(struct shader_pass* pass)
{
        if (
                        !compile_shader(GL_VERTEX_SHADER, 0, vertex_shader_default_tex_src, &pass->program.vertex_shader) ||
                        !compile_shader(GL_FRAGMENT_SHADER, 0, fragment_shader_default_tex_src, &pass->program.fragment_shader) ||
                        !create_program(&pass->program))
                return 0;
        glw->glGenVertexArrays(1, (GLuint *)&pass->vertex_array);

        struct shader_uniforms* u = &pass->uniforms;
        int p = pass->program.id;
        memset(u, -1, sizeof(struct shader_uniforms));
        u->vertex_coord = get_attrib(p, "VertexCoord");
        u->tex_coord = get_attrib(p, "TexCoord");
        u->texture = get_uniform(p, "Texture");
        pass->scale.mode[0] = pass->scale.mode[1] = SCALE_SOURCE;
        pass->scale.value[0] = pass->scale.value[1] = 1.0f;
        pass->fbo.id = -1;
        pass->active = 1;
        return 1;
}

static int create_default_shader_color(struct shader_pass* pass)
{
        if (
                        !compile_shader(GL_VERTEX_SHADER, 0, vertex_shader_default_color_src, &pass->program.vertex_shader) ||
                        !compile_shader(GL_FRAGMENT_SHADER, 0, fragment_shader_default_color_src, &pass->program.fragment_shader) ||
                        !create_program(&pass->program))
                return 0;
        glw->glGenVertexArrays(1, (GLuint *)&pass->vertex_array);

        struct shader_uniforms* u = &pass->uniforms;
        int p = pass->program.id;
        memset(u, -1, sizeof(struct shader_uniforms));
        u->vertex_coord = get_attrib(p, "VertexCoord");
        u->color = get_attrib(p, "Color");
        pass->scale.mode[0] = pass->scale.mode[1] = SCALE_SOURCE;
        pass->scale.value[0] = pass->scale.value[1] = 1.0f;
        pass->fbo.id = -1;
        pass->active = 1;
        return 1;
}

/* create the default scene shader */
static void create_scene_shader()
{
        struct shader scene_shader_conf;
        memset(&scene_shader_conf, 0, sizeof(struct shader));
        create_default_shader_tex(&active_shader->scene);
        scene_shader_conf.filter_linear = video_scale_mode;
        if (active_shader->num_shaders > 0 && active_shader->shaders[0].input_filter_linear >= 0)
                scene_shader_conf.filter_linear = active_shader->shaders[0].input_filter_linear;
        setup_fbo(&scene_shader_conf, &active_shader->scene.fbo);

        memset(&scene_shader_conf, 0, sizeof(struct shader));
        create_default_shader_color(&active_shader->fs_color);
        setup_fbo(&scene_shader_conf, &active_shader->fs_color.fbo);
}

static int load_texture(const char* f, struct shader_texture* tex)
{
        void* img = wx_image_load(f);
        if (!img)
                return 0;
        int width, height;
        wx_image_get_size(img, &width, &height);

        if (width != next_pow2(width) || height != next_pow2(height))
                wx_image_rescale(img, next_pow2(width), next_pow2(height));

        wx_image_get_size(img, &width, &height);

        GLubyte* rgb = wx_image_get_data(img);
        GLubyte* alpha = wx_image_get_alpha(img);

        int bpp = alpha ?  4 : 3;

        GLubyte* data = malloc(width*height*bpp);

        int x, y, Y;
        for (y = 0; y < height; ++y)
        {
                Y = height-y-1;
                for (x = 0; x < width; x++)
                {
                        data[(y*width+x)*bpp+0] = rgb[(Y*width+x)*3+0];
                        data[(y*width+x)*bpp+1] = rgb[(Y*width+x)*3+1];
                        data[(y*width+x)*bpp+2] = rgb[(Y*width+x)*3+2];

                        if (alpha)
                                data[(y*width+x)*bpp+3] = alpha[Y*width+x];
                }
        }
        wx_image_free(img);

        tex->width = width;
        tex->height = height;
        tex->internal_format = alpha ? GL_RGBA8 : GL_RGB8;
        tex->format = alpha ? GL_RGBA : GL_RGB;
        tex->type = GL_UNSIGNED_BYTE;
        tex->data = data;
        return 1;
}

static glsl_t* load_glslp(glsl_t* glsl, int num_shader, const char* f)
{
        int i, j;
        glslp_t* p = glslp_parse(f);

        if (p)
        {
                char path[512];
                char file[512];
                int failed = 0;
                strcpy(path, f);
                char* filename = get_filename(path);

                struct glsl_shader* gshader = &glsl->shaders[num_shader];

                strcpy(gshader->name, p->name);
                *filename = 0;

                gshader->num_lut_textures = p->num_textures;

                for (i = 0; i < p->num_textures; ++i)
                {
                        struct texture* texture = &p->textures[i];

                        sprintf(file, "%s%s", path, texture->path);

                        struct shader_lut_texture* tex = &gshader->lut_textures[i];
                        strcpy(tex->name, texture->name);

                        pclog("Load texture %s...\n", file);

                        if (!load_texture(file, &tex->texture))
                        {
                                wx_simple_messagebox("GLSL Error", "Could not load texture: %s", file);
                                pclog("Could not load texture %s!\n", file);
                                failed = 1;
                                break;
                        }

                        if (!strcmp(texture->wrap_mode, "repeat"))
                                tex->texture.wrap_mode = GL_REPEAT;
                        else if (!strcmp(texture->wrap_mode, "mirrored_repeat"))
                                tex->texture.wrap_mode = GL_MIRRORED_REPEAT;
                        else if (!strcmp(texture->wrap_mode, "clamp_to_edge"))
                                tex->texture.wrap_mode = GL_CLAMP_TO_EDGE;
                        else
                                tex->texture.wrap_mode = GL_CLAMP_TO_BORDER;

                        tex->texture.mipmap = texture->mipmap;

                        tex->texture.min_filter = tex->texture.mag_filter = texture->linear ? GL_LINEAR : GL_NEAREST;
                        if (tex->texture.mipmap)
                                tex->texture.min_filter = texture->linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

                        create_texture(&tex->texture);
                        free(tex->texture.data);
                        tex->texture.data = 0;
                }

                if (!failed)
                {
                        gshader->input_filter_linear = p->input_filter_linear;

                        gshader->num_parameters = p->num_parameters;
                        for (j = 0; j < gshader->num_parameters; ++j)
                                memcpy(&gshader->parameters[j], &p->parameters[j], sizeof(struct shader_parameter));

                        gshader->num_passes = p->num_shaders;

                        for (i = 0; i < p->num_shaders; ++i)
                        {
                                struct shader* shader = &p->shaders[i];
                                struct shader_pass* pass = &gshader->passes[i];

                                strcpy(pass->alias, shader->alias);
                                if (!strlen(pass->alias))
                                        sprintf(pass->alias, "Pass %u", (i+1));

                                pclog("Creating pass %u (%s)\n", (i+1), pass->alias);
                                pclog("Loading shader %s...\n", shader->shader_fn);
                                if (!shader->shader_program)
                                {
                                        wx_simple_messagebox("GLSL Error", "Could not load shader: %s", shader->shader_fn);
                                        pclog("Could not load shader %s\n", shader->shader_fn);
                                        failed = 1;
                                        break;
                                }
                                else
                                        pclog("Shader %s loaded\n", shader->shader_fn);
                                failed =
                                                !compile_shader(GL_VERTEX_SHADER, "#define VERTEX\n#define PARAMETER_UNIFORM\n", shader->shader_program, &pass->program.vertex_shader) ||
                                                !compile_shader(GL_FRAGMENT_SHADER, "#define FRAGMENT\n#define PARAMETER_UNIFORM\n", shader->shader_program, &pass->program.fragment_shader);
                                if (failed)
                                        break;

                                if (!create_program(&pass->program))
                                {
                                        failed = 1;
                                        break;
                                }
                                pass->frame_count_mod = shader->frame_count_mod;
                                pass->fbo.mipmap_input = shader->mipmap_input;

                                glw->glGenVertexArrays(1, (GLuint *)&pass->vertex_array);
                                find_uniforms(gshader, i);
                                setup_scale(shader, pass);
                                if (i == p->num_shaders-1) /* last pass may or may not be an fbo depending on scale */
                                {
                                        if (num_shader == glsl->num_shaders-1)
                                        {
                                                pass->fbo.id = -1;

                                                for (j = 0; j < 2; ++j)
                                                {
                                                        if (pass->scale.mode[j] != SCALE_SOURCE || pass->scale.value[j] != 1)
                                                        {
                                                                setup_fbo(shader, &pass->fbo);
                                                                break;
                                                        }
                                                }
                                        }
                                        else
                                        {
                                                /* check if next shaders' first pass wants the input mipmapped (will this ever happen?) */
                                                pass->fbo.texture.mipmap = glsl->shaders[num_shader+1].num_passes > 0 && glsl->shaders[num_shader+1].passes[0].fbo.mipmap_input;
                                                /* check if next shader wants the output of this pass to be filtered */
                                                if (glsl->shaders[num_shader+1].num_passes > 0 && glsl->shaders[num_shader+1].input_filter_linear >= 0)
                                                        shader->filter_linear = glsl->shaders[num_shader+1].input_filter_linear;
                                                setup_fbo(shader, &pass->fbo);
                                        }
                                }
                                else
                                {
                                        /* check if next pass wants the input mipmapped, if so we need to generate mipmaps of this pass */
                                        pass->fbo.texture.mipmap = (i+1) < p->num_shaders && p->shaders[i+1].mipmap_input;
                                        setup_fbo(shader, &pass->fbo);
                                }
                                if (pass->fbo.srgb) glsl->srgb = 1;
                                pass->active = 1;
                        }
                        if (!failed)
                        {
                                if (gshader->has_prev)
                                {
                                        struct shader scene_shader_conf;
                                        memset(&scene_shader_conf, 0, sizeof(struct shader));
                                        for (i = 0; i < MAX_PREV; ++i)
                                        {
                                                setup_fbo(&scene_shader_conf, &gshader->prev[i].fbo);
                                        }

                                }
                        }
                }

                glslp_free(p);

                return glsl;
        }
        return 0;
}

static glsl_t* load_shaders(int num, char shaders[MAX_USER_SHADERS][512])
{
        int i;
        glsl_t* glsl;

        glsl = malloc(sizeof(glsl_t));
        memset(glsl, 0, sizeof(glsl_t));

        glsl->num_shaders = num;
        int failed = 0;
        for (i = num-1; i >= 0; --i)
        {
                const char* f = shaders[i];
                if (f && strlen(f))
                {
                        if (!load_glslp(glsl, i, f))
                        {
                                failed = 1;
                                break;
                        }
                }

        }
        if (failed)
        {
                delete_glsl(glsl);
                memset(glsl, 0, sizeof(glsl_t));
        }
        return glsl;
}

static void read_shader_config()
{
        char s[512];
        int i, j;
        for (i = 0; i < active_shader->num_shaders; ++i)
        {
                struct glsl_shader* shader = &active_shader->shaders[i];
                char* name = shader->name;
                sprintf(s, "GL3 Shaders - %s", name);
//                shader->shader_refresh_rate = config_get_float(CFG_MACHINE, s, "shader_refresh_rate", -1);
                for (j = 0; j < shader->num_parameters; ++j)
                {
                        struct shader_parameter* param = &shader->parameters[j];
                        param->value = config_get_float(CFG_MACHINE, s, param->id, param->default_value);
                }
        }
}

int gl3_init(SDL_Window* window, sdl_render_driver requested_render_driver, SDL_Rect screen)
{
        int i, j;

        strcpy(current_render_driver_name, requested_render_driver.name);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        context = SDL_GL_CreateContext(window);

        if (!context)
        {
                pclog("Could not create GL context.\n");
                return FALSE;
        }

        SDL_GL_SetSwapInterval(video_vsync ? 1 : 0);

        pclog("OpenGL information: [%s] %s (%s)\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
        glsl_version[0] = glsl_version[1] = -1;
        glGetIntegerv(GL_MAJOR_VERSION, &glsl_version[0]);
        glGetIntegerv(GL_MINOR_VERSION, &glsl_version[1]);
        if (glsl_version[0] < 3)
        {
                pclog("OpenGL 3.0 is not available.");
                return SDL_FALSE;
        }
        pclog("Using OpenGL %s\n", glGetString(GL_VERSION));
        pclog("Using Shading Language %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        pclog("Max texture size: %dx%d\n", max_texture_size, max_texture_size);

        //SDL_GL_MakeCurrent(window, context);

        glw = glw_init();

        glEnable(GL_TEXTURE_2D);

        scene_texture.data = NULL;
        scene_texture.width = screen.w;
        scene_texture.height = screen.h;
        scene_texture.internal_format = GL_RGBA8;
        scene_texture.format = GL_RGBA;
        scene_texture.type = GL_UNSIGNED_INT_8_8_8_8_REV;
        scene_texture.wrap_mode = GL_CLAMP_TO_BORDER;
        scene_texture.min_filter = scene_texture.mag_filter = video_scale_mode ? GL_LINEAR : GL_NEAREST;
        scene_texture.mipmap = 0;

        create_texture(&scene_texture);

        /* load shader */
//        const char* shaders[1];
//        shaders[0] = gl3_shader_file;
//
//        active_shader = load_shaders(1, shaders);

//        const char* shaders[3];
//        shaders[0] = "/home/phantasy/git/glsl-shaders/ntsc/ntsc-320px.glslp";
//        shaders[1] = "/home/phantasy/git/glsl-shaders/motionblur/motionblur-simple.glslp";
//        shaders[2] = "/home/phantasy/git/glsl-shaders/crt/crt-lottes-multipass.glslp";
//
//        active_shader = load_shaders(3, shaders);
        int num_shaders = 0;
        for (i = 0; i < MAX_USER_SHADERS; ++i)
        {
                if (strlen(gl3_shader_file[i]))
                        ++num_shaders;
                else
                        break;
        }
        active_shader = load_shaders(num_shaders, gl3_shader_file);

        create_scene_shader();

        /* read config */
        read_shader_config();

        /* buffers */

        GLfloat vertex[] = {
                -1.0f, -1.0f,
                -1.0f,  1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f
        };

        GLfloat inv_vertex[] = {
                -1.0f,  1.0f,
                -1.0f, -1.0f,
                 1.0f,  1.0f,
                 1.0f, -1.0f
        };

        GLfloat tex_coords[] = {
                0.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 0.0f,
                1.0f, 1.0f
        };


        GLfloat colors[] = {
                1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f
        };


        /* set the scene shader buffers */
        {
                glw->glBindVertexArray(active_shader->scene.vertex_array);

                struct shader_vbo* vbo = &active_shader->scene.vbo;

                glw->glGenBuffers(1, (GLuint *)&vbo->vertex_coord);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(inv_vertex), inv_vertex, GL_STATIC_DRAW);
                glw->glVertexAttribPointer(active_shader->scene.uniforms.vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

                glw->glGenBuffers(1, (GLuint *)&vbo->tex_coord);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->tex_coord);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
                glw->glVertexAttribPointer(active_shader->scene.uniforms.tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);
        }

        /* set buffers for all passes */
        for (j = 0; j < active_shader->num_shaders; ++j)
        {
                struct glsl_shader* shader = &active_shader->shaders[j];
                for (i = 0; i < shader->num_passes; ++i)
                {
                        struct shader_uniforms* u = &shader->passes[i].uniforms;

                        glw->glBindVertexArray(shader->passes[i].vertex_array);

                        struct shader_vbo* vbo = &shader->passes[i].vbo;

                        glw->glGenBuffers(1, (GLuint *)&vbo->vertex_coord);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
                        glw->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

                        glw->glVertexAttribPointer(u->vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

                        glw->glGenBuffers(1, (GLuint *)&vbo->tex_coord);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->tex_coord);
                        glw->glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
                        glw->glVertexAttribPointer(u->tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);

                        if (u->color)
                        {
                                glw->glGenBuffers(1, (GLuint *)&vbo->color);
                                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->color);
                                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
                                glw->glVertexAttribPointer(u->color, 4, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), (GLvoid*)0);
                        }
                }
        }

        for (i = 0; i < active_shader->num_shaders; ++i)
        {
                struct glsl_shader* shader = &active_shader->shaders[i];
                if (shader->has_prev)
                {
                        struct shader_pass* prev_pass = &shader->prev_scene;
                        create_default_shader_tex(prev_pass);

                        struct shader_vbo* vbo = &prev_pass->vbo;

                        glw->glBindVertexArray(prev_pass->vertex_array);

                        glw->glGenBuffers(1, (GLuint *)&vbo->vertex_coord);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
                        glw->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
                        glw->glVertexAttribPointer(prev_pass->uniforms.vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

                        glw->glGenBuffers(1, (GLuint *)&vbo->tex_coord);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->tex_coord);
                        glw->glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
                        glw->glVertexAttribPointer(prev_pass->uniforms.tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);

                        for (j = 0; j < MAX_PREV; ++j)
                        {
                                struct shader_prev* prev = &shader->prev[j];
                                struct shader_vbo* prev_vbo = &prev->vbo;

                                glw->glGenBuffers(1, (GLuint *)&prev_vbo->vertex_coord);
                                glw->glBindBuffer(GL_ARRAY_BUFFER, prev_vbo->vertex_coord);
                                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

                                glw->glGenBuffers(1, (GLuint *)&prev_vbo->tex_coord);
                                glw->glBindBuffer(GL_ARRAY_BUFFER, prev_vbo->tex_coord);
                                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
                        }
                }
        }

        /* create final pass */
        if (active_shader->num_shaders == 0 || active_shader->shaders[active_shader->num_shaders-1].passes[active_shader->shaders[active_shader->num_shaders-1].num_passes-1].fbo.id >= 0)
        {
                struct shader_pass* final_pass = &active_shader->final_pass;
                create_default_shader_tex(final_pass);

                glw->glBindVertexArray(final_pass->vertex_array);

                struct shader_vbo* vbo = &final_pass->vbo;

                glw->glGenBuffers(1, (GLuint *)&vbo->vertex_coord);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
                glw->glVertexAttribPointer(final_pass->uniforms.vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

                glw->glGenBuffers(1, (GLuint *)&vbo->tex_coord);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->tex_coord);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
                glw->glVertexAttribPointer(final_pass->uniforms.tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);
        }

        {
                struct shader_pass* color_pass = &active_shader->fs_color;
                create_default_shader_color(color_pass);

                glw->glBindVertexArray(color_pass->vertex_array);

                struct shader_vbo* vbo = &color_pass->vbo;

                glw->glGenBuffers(1, (GLuint *)&vbo->vertex_coord);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
                glw->glVertexAttribPointer(color_pass->uniforms.vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

                glw->glGenBuffers(1, (GLuint *)&vbo->color);
                glw->glBindBuffer(GL_ARRAY_BUFFER, vbo->color);
                glw->glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_DYNAMIC_DRAW);
                glw->glVertexAttribPointer(color_pass->uniforms.color, 4, GL_FLOAT, GL_TRUE, 4*sizeof(GLfloat), (GLvoid*)0);

        }
#ifdef SDL2_SHADER_DEBUG
        struct shader_pass* debug_pass = &active_shader->debug;
        create_default_shader(debug_pass);

        glBindVertexArray(debug_pass->vertex_array);

        struct shader_vbo* vbo = &debug_pass->vbo;

        glGenBuffers(1, &vbo->vertex_coord);
        glBindBuffer(GL_ARRAY_BUFFER, vbo->vertex_coord);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
        glVertexAttribPointer(debug_pass->uniforms.vertex_coord, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), (GLvoid*)0);

        glGenBuffers(1, &vbo->tex_coord);
        glBindBuffer(GL_ARRAY_BUFFER, vbo->tex_coord);
        glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coords), tex_coords, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(debug_pass->uniforms.tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);
#endif

        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glw->glBindVertexArray(0);

        return SDL_TRUE;
}

void gl3_close()
{
        if (context)
        {
                delete_texture(&scene_texture);

                if (active_shader)
                {
                        delete_glsl(active_shader);
                        free(active_shader);
                }
                active_shader = NULL;

                SDL_GL_DeleteContext(context);
                context = NULL;
        }
        glw_free(glw);
}

void gl3_update(SDL_Window* window, SDL_Rect updated_rect, BITMAP* screen)
{
        if (!context)
                return;
        glBindTexture(GL_TEXTURE_2D, scene_texture.id);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, screen->w);
        glTexSubImage2D(GL_TEXTURE_2D, 0, updated_rect.x, updated_rect.y, updated_rect.w, updated_rect.h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &((uint32_t*) screen->dat)[updated_rect.y * screen->w + updated_rect.x]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
}

struct render_data {
        int pass;
        struct glsl_shader* shader;
        struct shader_pass* shader_pass;
        GLfloat* output_size;
        struct shader_pass* orig_pass;
        GLint texture;
        int frame_count;
};

static void render_pass(struct render_data* data)
{
        int i;
        GLuint texture_unit = 0;

//        pclog("pass %d: %gx%g, %gx%g -> %gx%g, %gx%g, %gx%g\n", num_pass, pass->state.input_size[0], pass->state.input_size[1], pass->state.input_texture_size[0], pass->state.input_texture_size[1], pass->state.output_size[0], pass->state.output_size[1], pass->state.output_texture_size[0], pass->state.output_texture_size[1], output_size[0], output_size[1]);

        glw->glBindVertexArray(data->shader_pass->vertex_array);

        GLint p = data->shader_pass->program.id;
        struct shader_uniforms* u = &data->shader_pass->uniforms;

        glw->glUseProgram(p);

        if (data->texture)
        {
                glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                glBindTexture(GL_TEXTURE_2D, data->texture);
                glw->glUniform1i(u->texture, texture_unit);
                texture_unit++;
        }

        if (u->color >= 0)              glw->glEnableVertexAttribArray(u->color);

        if (u->mvp_matrix >= 0)         glw->glUniformMatrix4fv(u->mvp_matrix, 1, 0, matrix);
        if (u->frame_direction >= 0)    glw->glUniform1i(u->frame_direction, 1);

        int framecnt = data->frame_count;
        if (data->shader_pass->frame_count_mod > 0)
                framecnt = framecnt%data->shader_pass->frame_count_mod;
        if (u->frame_count >= 0)        glw->glUniform1i(u->frame_count, framecnt);

        if (u->input_size >= 0)         glw->glUniform2fv(u->input_size, 1, data->shader_pass->state.input_size);
        if (u->texture_size >= 0)       glw->glUniform2fv(u->texture_size, 1, data->shader_pass->state.input_texture_size);
        if (u->output_size >= 0)        glw->glUniform2fv(u->output_size, 1, data->output_size);

        if (data->shader)
        {
                /* parameters */
                for (i = 0; i < data->shader->num_parameters; ++i)
                        if (u->parameters[i] >= 0) glw->glUniform1f(u->parameters[i], data->shader->parameters[i].value);

                if (data->pass > 0)
                {
                        struct shader_pass* passes = data->shader->passes;
                        struct shader_pass* orig = data->orig_pass;
                        if (u->orig.texture >= 0)
                        {
                                glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                                glBindTexture(GL_TEXTURE_2D, orig->fbo.texture.id);
                                glw->glUniform1i(u->orig.texture, texture_unit);
                                texture_unit++;
                        }
                        if (u->orig.input_size >= 0)         glw->glUniform2fv(u->orig.input_size, 1, orig->state.input_size);
                        if (u->orig.texture_size >= 0)       glw->glUniform2fv(u->orig.texture_size, 1, orig->state.input_texture_size);

                        for (i = 0; i < data->pass; ++i)
                        {
                                if (u->pass[i].texture >= 0)
                                {
                                        glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                                        glBindTexture(GL_TEXTURE_2D, passes[i].fbo.texture.id);
                                        glw->glUniform1i(u->pass[i].texture, texture_unit);
                                        texture_unit++;
                                }
                                if (u->pass[i].texture_size >= 0)       glw->glUniform2fv(u->pass[i].texture_size, 1, passes[i].state.input_texture_size);
                                if (u->pass[i].input_size >= 0)         glw->glUniform2fv(u->pass[i].input_size, 1, passes[i].state.input_size);

                                if (u->prev_pass[i].texture >= 0)
                                {
                                        glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                                        glBindTexture(GL_TEXTURE_2D, passes[i].fbo.texture.id);
                                        glw->glUniform1i(u->prev_pass[i].texture, texture_unit);
                                        texture_unit++;
                                }
                                if (u->prev_pass[i].texture_size >= 0)  glw->glUniform2fv(u->prev_pass[i].texture_size, 1, passes[i].state.input_texture_size);
                                if (u->prev_pass[i].input_size >= 0)    glw->glUniform2fv(u->prev_pass[i].input_size, 1, passes[i].state.input_size);

                        }
                }

                if (data->shader->has_prev)
                {
                        /* loop through each previous frame */
                        for (i = 0; i < MAX_PREV; ++i)
                        {
                                if (u->prev[i].texture >= 0)
                                {
                                        glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                                        glBindTexture(GL_TEXTURE_2D, data->shader->prev[i].fbo.texture.id);
                                        glw->glUniform1i(u->prev[i].texture, texture_unit);
                                        texture_unit++;
                                }
                                if (u->prev[i].tex_coord >= 0)
                                {
                                        glw->glBindBuffer(GL_ARRAY_BUFFER, data->shader->prev[i].vbo.tex_coord);
                                        glw->glVertexAttribPointer(u->prev[i].tex_coord, 2, GL_FLOAT, GL_TRUE, 2*sizeof(GLfloat), (GLvoid*)0);
                                        glw->glEnableVertexAttribArray(u->prev[i].tex_coord);
                                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);
                                }
                        }
                }

                for (i = 0; i < data->shader->num_lut_textures; ++i)
                {
                        if (u->lut_textures[i] >= 0)
                        {
                                glw->glActiveTexture(GL_TEXTURE0 + texture_unit);
                                glBindTexture(GL_TEXTURE_2D, data->shader->lut_textures[i].texture.id);
                                glw->glUniform1i(u->lut_textures[i], texture_unit);
                                texture_unit++;
                        }

                }
        }

        glw->glEnableVertexAttribArray(u->vertex_coord);
        glw->glEnableVertexAttribArray(u->tex_coord);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glw->glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glw->glDisableVertexAttribArray(data->shader_pass->uniforms.vertex_coord);
        glw->glDisableVertexAttribArray(data->shader_pass->uniforms.tex_coord);
        if (data->shader_pass->uniforms.color >= 0)  glw->glDisableVertexAttribArray(data->shader_pass->uniforms.color);

        if (data->shader && data->shader->has_prev)
        {
                for (i = 0; i < MAX_PREV; ++i)
                {
                        if (u->prev[i].tex_coord >= 0)
                                glw->glDisableVertexAttribArray(u->prev[i].tex_coord);

                }
        }

        glw->glBindVertexArray(0);


        glw->glUseProgram(0);
}

void gl3_present(SDL_Window* window, SDL_Rect video_rect, SDL_Rect window_rect, SDL_Rect screen)
{
        if (!context)
                return;

        int s, i, j;

        Uint32 ticks = SDL_GetTicks();

        GLfloat orig_output_size[] = {
                window_rect.w, window_rect.h
        };

        if (active_shader->srgb) glEnable(GL_FRAMEBUFFER_SRGB);

        struct render_data data;

        /* render scene to texture */
        {
                struct shader_pass* pass = &active_shader->scene;

                SDL_Rect rect;
                rect.x = rect.y = 0;
                rect.w = video_rect.w*gl3_input_scale;
                rect.h = video_rect.h*gl3_input_scale;
                sdl_scale(gl3_input_stretch, rect, &rect, video_rect.w, video_rect.h);

                pass->state.input_size[0] = pass->state.output_size[0] = rect.w;
                pass->state.input_size[1] = pass->state.output_size[1] = rect.h;

                pass->state.input_texture_size[0] = pass->state.output_texture_size[0] = next_pow2(pass->state.output_size[0]);
                pass->state.input_texture_size[1] = pass->state.output_texture_size[1] = next_pow2(pass->state.output_size[1]);

                recreate_fbo(&active_shader->scene.fbo, pass->state.output_texture_size[0], pass->state.output_texture_size[1]);

                glw->glBindFramebuffer(GL_FRAMEBUFFER, active_shader->scene.fbo.id);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                glViewport(0, 0, pass->state.output_size[0], pass->state.output_size[1]);

                GLfloat minx = 0;
                GLfloat miny = 0;
                GLfloat maxx = pass->state.output_size[0]/(GLfloat)pass->state.output_texture_size[0];
                GLfloat maxy = pass->state.output_size[1]/(GLfloat)pass->state.output_texture_size[1];

                pass->state.tex_coords[0] = minx;
                pass->state.tex_coords[1] = miny;
                pass->state.tex_coords[2] = minx;
                pass->state.tex_coords[3] = maxy;
                pass->state.tex_coords[4] = maxx;
                pass->state.tex_coords[5] = miny;
                pass->state.tex_coords[6] = maxx;
                pass->state.tex_coords[7] = maxy;

                // create input tex coords
                minx = video_rect.x/(float)screen.w;
                miny = video_rect.y/(float)screen.h;
                maxx = (video_rect.x+video_rect.w)/(float)screen.w;
                maxy = (video_rect.y+video_rect.h)/(float)screen.h;

                GLfloat tex_coords[] = {
                        minx, miny,
                        minx, maxy,
                        maxx, miny,
                        maxx, maxy
                };

                glw->glBindVertexArray(pass->vertex_array);

                glw->glBindBuffer(GL_ARRAY_BUFFER, pass->vbo.tex_coord);
                glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 8*sizeof(GLfloat), tex_coords);
                glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                memset(&data, 0, sizeof(struct render_data));
                data.pass = -1;
                data.shader_pass = &active_shader->scene;
                data.texture = scene_texture.id;
                data.output_size = orig_output_size;
                render_pass(&data);

                glw->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        struct shader_pass* orig = &active_shader->scene;
        struct shader_pass* input = &active_shader->scene;

        for (s = 0; s < active_shader->num_shaders; ++s)
        {
                struct glsl_shader* shader = &active_shader->shaders[s];

//                float refresh_rate = shader->shader_refresh_rate;
//                if (refresh_rate < 0)
//                        refresh_rate = gl3_shader_refresh_rate;
                float refresh_rate = gl3_shader_refresh_rate;
                if (refresh_rate == 0)
                        refresh_rate = video_refresh_rate;
                int frame_count = ticks/(1000.0f/refresh_rate);

                /* loop through each pass */
                for (i = 0; i < shader->num_passes; ++i)
                {
                        struct shader_pass* pass = &shader->passes[i];

                        memcpy(pass->state.input_size, input->state.output_size, 2*sizeof(GLfloat));
                        memcpy(pass->state.input_texture_size, input->state.output_texture_size, 2*sizeof(GLfloat));

                        for (j = 0; j < 2; ++j)
                        {
                                if (pass->scale.mode[j] == SCALE_VIEWPORT)
                                        pass->state.output_size[j] = orig_output_size[j]*pass->scale.value[j];
                                else if (pass->scale.mode[j] == SCALE_ABSOLUTE)
                                        pass->state.output_size[j] = pass->scale.value[j];
                                else
                                        pass->state.output_size[j] = pass->state.input_size[j]*pass->scale.value[j];

                                pass->state.output_texture_size[j] = next_pow2(pass->state.output_size[j]);
                        }

                        if (pass->fbo.id >= 0)
                        {
                                recreate_fbo(&pass->fbo, pass->state.output_texture_size[0], pass->state.output_texture_size[1]);

                                glw->glBindFramebuffer(GL_FRAMEBUFFER, pass->fbo.id);
                                glViewport(0, 0, pass->state.output_size[0], pass->state.output_size[1]);
                        }
                        else
                                glViewport(window_rect.x, window_rect.y, window_rect.w, window_rect.h);

                        glClearColor(0, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);

                        GLfloat minx = 0;
                        GLfloat miny = 0;
                        GLfloat maxx = pass->state.output_size[0]/(GLfloat)pass->state.output_texture_size[0];
                        GLfloat maxy = pass->state.output_size[1]/(GLfloat)pass->state.output_texture_size[1];

                        pass->state.tex_coords[0] = minx;
                        pass->state.tex_coords[1] = miny;
                        pass->state.tex_coords[2] = minx;
                        pass->state.tex_coords[3] = maxy;
                        pass->state.tex_coords[4] = maxx;
                        pass->state.tex_coords[5] = miny;
                        pass->state.tex_coords[6] = maxx;
                        pass->state.tex_coords[7] = maxy;

                        glw->glBindVertexArray(pass->vertex_array);

                        glw->glBindBuffer(GL_ARRAY_BUFFER, pass->vbo.tex_coord);
                        glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 8*sizeof(GLfloat), input->state.tex_coords);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                        memset(&data, 0, sizeof(struct render_data));
                        data.shader = shader;
                        data.pass = i;
                        data.shader_pass = pass;
                        data.texture = input->fbo.texture.id;
                        data.output_size = orig_output_size;
                        data.orig_pass = orig;
                        data.frame_count = frame_count;

                        render_pass(&data);

                        glw->glBindFramebuffer(GL_FRAMEBUFFER, 0);

                        if (pass->fbo.texture.mipmap)
                        {
                                glw->glActiveTexture(GL_TEXTURE0);
                                glBindTexture(GL_TEXTURE_2D, pass->fbo.texture.id);
                                glw->glGenerateMipmap(GL_TEXTURE_2D);
                                glBindTexture(GL_TEXTURE_2D, 0);


                        }

                        input = pass;
                }

                if (shader->has_prev && (ticks-shader->last_prev_update) >= (1000.f/refresh_rate))
                {
                        shader->last_prev_update = ticks;

                        /* shift array */
                        memmove(&shader->prev[1], &shader->prev[0], MAX_PREV*sizeof(struct shader_prev));
                        memcpy(&shader->prev[0], &shader->prev[MAX_PREV], sizeof(struct shader_prev));

                        struct shader_pass* pass = orig;
                        struct shader_pass* prev_pass = &shader->prev_scene;
                        struct shader_prev* prev = &shader->prev[0];

                        memcpy(&prev_pass->state, &pass->state, sizeof(struct shader_state));

                        recreate_fbo(&prev->fbo, prev_pass->state.output_texture_size[0], prev_pass->state.output_texture_size[1]);

                        memcpy(&prev_pass->fbo, &prev->fbo, sizeof(struct shader_fbo));

                        glw->glBindFramebuffer(GL_FRAMEBUFFER, prev->fbo.id);
                        glClearColor(0, 0, 0, 1);
                        glClear(GL_COLOR_BUFFER_BIT);

                        glViewport(0, 0, pass->state.output_size[0], pass->state.output_size[1]);

                        glw->glBindVertexArray(prev_pass->vertex_array);

                        glw->glBindBuffer(GL_ARRAY_BUFFER, prev->vbo.tex_coord);
                        glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 8*sizeof(GLfloat), pass->state.tex_coords);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                        glw->glBindBuffer(GL_ARRAY_BUFFER, prev_pass->vbo.tex_coord);
                        glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 8*sizeof(GLfloat), pass->state.tex_coords);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                        memset(&data, 0, sizeof(struct render_data));
                        data.shader = shader;
                        data.pass = -10;
                        data.shader_pass = prev_pass;
                        data.texture = pass->fbo.texture.id;
                        data.output_size = orig_output_size;

                        render_pass(&data);

                        glw->glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }

                orig = input;
        }

        if (active_shader->final_pass.active)
        {
                struct shader_pass* pass = &active_shader->final_pass;

                memcpy(pass->state.input_size, input->state.output_size, 2*sizeof(GLfloat));
                memcpy(pass->state.input_texture_size, input->state.output_texture_size, 2*sizeof(GLfloat));

                for (j = 0; j < 2; ++j)
                {
                        if (pass->scale.mode[j] == SCALE_VIEWPORT)
                                pass->state.output_size[j] = orig_output_size[j]*pass->scale.value[j];
                        else if (pass->scale.mode[j] == SCALE_ABSOLUTE)
                                pass->state.output_size[j] = pass->scale.value[j];
                        else
                                pass->state.output_size[j] = pass->state.input_size[j]*pass->scale.value[j];

                        pass->state.output_texture_size[j] = next_pow2(pass->state.output_size[j]);
                }

                glViewport(window_rect.x, window_rect.y, window_rect.w, window_rect.h);

                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                GLfloat minx = 0;
                GLfloat miny = 0;
                GLfloat maxx = pass->state.output_size[0]/(GLfloat)pass->state.output_texture_size[0];
                GLfloat maxy = pass->state.output_size[1]/(GLfloat)pass->state.output_texture_size[1];

                pass->state.tex_coords[0] = minx;
                pass->state.tex_coords[1] = miny;
                pass->state.tex_coords[2] = minx;
                pass->state.tex_coords[3] = maxy;
                pass->state.tex_coords[4] = maxx;
                pass->state.tex_coords[5] = miny;
                pass->state.tex_coords[6] = maxx;
                pass->state.tex_coords[7] = maxy;

                glw->glBindVertexArray(pass->vertex_array);

                glw->glBindBuffer(GL_ARRAY_BUFFER, pass->vbo.tex_coord);
                glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 8*sizeof(GLfloat), input->state.tex_coords);
                glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                memset(&data, 0, sizeof(struct render_data));
                data.pass = -2;
                data.shader_pass = pass;
                data.texture = input->fbo.texture.id;
                data.output_size = orig_output_size;
                data.orig_pass = orig;

                render_pass(&data);
        }

        if (!take_screenshot)
        {
                if (video_focus_dim && !(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS))
                {
                        struct shader_pass* pass = &active_shader->fs_color;
                        GLfloat r = 0;
                        GLfloat g = 0;
                        GLfloat b = 0;
                        GLfloat a = 0x80/(float)0xff;

                        GLfloat colors[] = {
                                r, g, b, a,
                                r, g, b, a,
                                r, g, b, a,
                                r, g, b, a
                        };

                        glw->glBindVertexArray(pass->vertex_array);

                        glw->glBindBuffer(GL_ARRAY_BUFFER, pass->vbo.color);
                        glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 16*sizeof(GLfloat), colors);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                        memset(&data, 0, sizeof(struct render_data));
                        data.pass = -3;
                        data.shader_pass = pass;
                        data.texture = 0;
                        data.output_size = orig_output_size;
                        data.orig_pass = orig;

                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glEnable(GL_BLEND);
                        render_pass(&data);
                        glDisable(GL_BLEND);
                }
                if (flash.enabled)
                {
                        struct shader_pass* pass = &active_shader->fs_color;
                        GLfloat r = (flash.color[0]&0xff)/(float)0xff;
                        GLfloat g = (flash.color[1]&0xff)/(float)0xff;
                        GLfloat b = (flash.color[2]&0xff)/(float)0xff;
                        GLfloat a = (flash.color[3]&0xff)/(float)0xff;

                        GLfloat colors[] = {
                                r, g, b, a,
                                r, g, b, a,
                                r, g, b, a,
                                r, g, b, a
                        };

                        glw->glBindVertexArray(pass->vertex_array);

                        glw->glBindBuffer(GL_ARRAY_BUFFER, pass->vbo.color);
                        glw->glBufferSubData(GL_ARRAY_BUFFER, 0, 16*sizeof(GLfloat), colors);
                        glw->glBindBuffer(GL_ARRAY_BUFFER, 0);

                        memset(&data, 0, sizeof(struct render_data));
                        data.pass = -3;
                        data.shader_pass = pass;
                        data.texture = 0;
                        data.output_size = orig_output_size;
                        data.orig_pass = orig;

                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glEnable(GL_BLEND);
                        render_pass(&data);
                        glDisable(GL_BLEND);
                }
        }
        else
        {
                take_screenshot = 0;

                int width = window_rect.w;
                int height = window_rect.h;

                SDL_GetWindowSize(window, &width, &height);

                unsigned char* rgba = (unsigned char*)malloc(width*height*4);

                glFinish();
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

                int x, y;
                unsigned char* rgb = (unsigned char*)malloc(width*height*3);

                for (x = 0; x < width; ++x)
                {
                        for (y = 0; y < height; ++y)
                        {
                                rgb[(y*width+x)*3+0] = rgba[((height-y-1)*width+x)*4+0];
                                rgb[(y*width+x)*3+1] = rgba[((height-y-1)*width+x)*4+1];
                                rgb[(y*width+x)*3+2] = rgba[((height-y-1)*width+x)*4+2];
                        }
                }

                screenshot_taken(rgb, width, height);

                free(rgb);
                free(rgba);
        }

        // DEBUG: render FBO
#ifdef SDL2_SHADER_DEBUG
//        GLint texture = scene_texture.id;
        GLint texture = active_shader->lut_textures[0].texture.id;
//        GLint texture = active_shader->scene.fbo.texture;

        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0, 0, orig_output_size[0], orig_output_size[1]);

        struct shader_pass* pass = &active_shader->debug;

        render_pass(-999, pass, orig_output_size, texture);
#endif

        glDisable(GL_FRAMEBUFFER_SRGB);

        SDL_GL_SwapWindow(window);

}

sdl_renderer_t* gl3_renderer_create()
{
        sdl_renderer_t* renderer = malloc(sizeof(sdl_renderer_t));
        renderer->init = gl3_init;
        renderer->close = gl3_close;
        renderer->update = gl3_update;
        renderer->present = gl3_present;
        renderer->always_update = 1;
        return renderer;
}

void gl3_renderer_close(sdl_renderer_t* renderer)
{
        free(renderer);
}

static int available = -1;

int gl3_renderer_available(struct sdl_render_driver* driver)
{
        if (available < 0)
        {
                available = 0;
                SDL_Window* window = SDL_CreateWindow("GL3 test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
                if (window)
                {
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

                        SDL_GLContext context = SDL_GL_CreateContext(window);
                        if (context)
                        {
                                int version = -1;
                                glGetIntegerv(GL_MAJOR_VERSION, &version);

                                SDL_GL_DeleteContext(context);

                                available = version >= 3;
                        }
                        SDL_DestroyWindow(window);
                }

        }
        return available;
}
