#ifndef SRC_WX_SDL2_GLW_H_
#define SRC_WX_SDL2_GLW_H_

typedef struct glw_t {
        PFNGLACTIVETEXTUREPROC glActiveTexture;
        PFNGLCREATESHADERPROC glCreateShader;
        PFNGLSHADERSOURCEPROC glShaderSource;
        PFNGLCOMPILESHADERPROC glCompileShader;
        PFNGLGETSHADERIVPROC glGetShaderiv;
        PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
        PFNGLCREATEPROGRAMPROC glCreateProgram;
        PFNGLATTACHSHADERPROC glAttachShader;
        PFNGLLINKPROGRAMPROC glLinkProgram;
        PFNGLGETPROGRAMIVPROC glGetProgramiv;
        PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
        PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
        PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
        PFNGLUSEPROGRAMPROC glUseProgram;
        PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
        PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
        PFNGLDELETESHADERPROC glDeleteShader;
        PFNGLDELETEPROGRAMPROC glDeleteProgram;
        PFNGLDELETEBUFFERSPROC glDeleteBuffers;
        PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
        PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
        PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
        PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
        PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
        PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
        PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
        PFNGLGENBUFFERSPROC glGenBuffers;
        PFNGLBINDBUFFERPROC glBindBuffer;
        PFNGLBUFFERDATAPROC glBufferData;
        PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
        PFNGLUNIFORM1IPROC glUniform1i;
        PFNGLUNIFORM1FPROC glUniform1f;
        PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
        PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
        PFNGLUNIFORM2FVPROC glUniform2fv;
        PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
        PFNGLBUFFERSUBDATAPROC glBufferSubData;
} glw_t;

glw_t* glw_init()
{
        glw_t* glw = malloc(sizeof(glw_t));
        glw->glActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
        glw->glCreateShader = SDL_GL_GetProcAddress("glCreateShader");
        glw->glShaderSource = SDL_GL_GetProcAddress("glShaderSource");
        glw->glCompileShader = SDL_GL_GetProcAddress("glCompileShader");
        glw->glGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
        glw->glGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
        glw->glCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
        glw->glAttachShader = SDL_GL_GetProcAddress("glAttachShader");
        glw->glLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
        glw->glGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
        glw->glGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
        glw->glGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
        glw->glGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");
        glw->glUseProgram = SDL_GL_GetProcAddress("glUseProgram");
        glw->glGenerateMipmap = SDL_GL_GetProcAddress("glGenerateMipmap");
        glw->glDeleteFramebuffers = SDL_GL_GetProcAddress("glDeleteFramebuffers");
        glw->glDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
        glw->glDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
        glw->glDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
        glw->glDeleteVertexArrays = SDL_GL_GetProcAddress("glDeleteVertexArrays");
        glw->glGenFramebuffers = SDL_GL_GetProcAddress("glGenFramebuffers");
        glw->glBindFramebuffer = SDL_GL_GetProcAddress("glBindFramebuffer");
        glw->glFramebufferTexture2D = SDL_GL_GetProcAddress("glFramebufferTexture2D");
        glw->glCheckFramebufferStatus = SDL_GL_GetProcAddress("glCheckFramebufferStatus");
        glw->glGenVertexArrays = SDL_GL_GetProcAddress("glGenVertexArrays");
        glw->glBindVertexArray = SDL_GL_GetProcAddress("glBindVertexArray");
        glw->glGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
        glw->glBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
        glw->glBufferData = SDL_GL_GetProcAddress("glBufferData");
        glw->glVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
        glw->glUniform1i = SDL_GL_GetProcAddress("glUniform1i");
        glw->glUniform1f = SDL_GL_GetProcAddress("glUniform1f");
        glw->glEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        glw->glUniformMatrix4fv = SDL_GL_GetProcAddress("glUniformMatrix4fv");
        glw->glUniform2fv = SDL_GL_GetProcAddress("glUniform2fv");
        glw->glDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
        glw->glBufferSubData = SDL_GL_GetProcAddress("glBufferSubData");
        return glw;
}

void glw_free(glw_t* glw)
{
        free(glw);
}

#endif /* SRC_WX_SDL2_GLW_H_ */
