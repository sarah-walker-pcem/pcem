#define BITMAP WINDOWS_BITMAP
#include <windows.h>
#undef BITMAP
#include <gl\gl.h>
#include <gl\glu.h>
#include "ibm.h"
#include "video.h"
#include "win-opengl.h"

HDC		hDC=NULL;		// Private GDI Device Context
HGLRC		hRC=NULL;		// Permanent Rendering Context
HWND            opengl_hwnd;


void opengl_init(HWND h)
{
        GLuint		PixelFormat;
        PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,							// Must Support Double Buffering
		PFD_TYPE_RGBA,								// Request An RGBA Format
		16,										// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		16,											// 16Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};
	
	if (!(hDC = GetDC(h)))
	   fatal("Couldn't get DC\n");
	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd)))
	   fatal("Couldn't choose pixel format\n");
	if (!SetPixelFormat(hDC, PixelFormat, &pfd))   
	   fatal("Couldn't set pixel format\n");
	if (!(hRC = wglCreateContext(hDC)))
	   fatal("Couldn't create rendering context\n");
	if (!wglMakeCurrent(hDC, hRC))
	   fatal("Couldn't set rendering context\n");
	   
	ShowWindow(h, SW_SHOW);
	   
	glViewport(0, 0, 200, 200);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();								// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f, (GLfloat)200/(GLfloat)200, 0.1f, 100.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();								// Reset The Modelview Matrix

	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);				// Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations

                glDisable(GL_ALPHA_TEST);
                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
                glDisable(GL_POLYGON_SMOOTH);
                glDisable(GL_STENCIL_TEST);
                glEnable(GL_DITHER);
                glDisable(GL_TEXTURE_2D);
                glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
                glClearColor(0.0, 0.0, 0.0, 0.0);

        	glMatrixMode(GL_PROJECTION);
        	glLoadIdentity();
	
	opengl_hwnd = h;
	atexit(opengl_close);	
	pclog("opengl_init\n");
}

void opengl_close()
{
        pclog("opengl_close\n");
        if (hRC)
        {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(hRC);
                hRC = NULL;
        }
        if (hDC)
        {
                ReleaseDC(opengl_hwnd, hDC);
                hDC = NULL;
        }                
}

void opengl_resize(int x, int y)
{
        x = y = 200;
	glViewport(0, 0, x, y);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();								// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f, (GLfloat)x/(GLfloat)y, 0.1f, 100.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();								// Reset The Modelview Matrix
}

void opengl_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h)
{
        pclog("blit memtoscreen\n");
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer
	glLoadIdentity();									// Reset The Current Modelview Matrix
	glColor3f(1.0f, 1.0f, 0.0f);
	glTranslatef(-1.5f,0.0f,-6.0f);						// Move Left 1.5 Units And Into The Screen 6.0
	glBegin(GL_TRIANGLES);								// Drawing Using Triangles
		glVertex3f( 0.0f, 1.0f, 0.0f);					// Top
		glVertex3f(-1.0f,-1.0f, 0.0f);					// Bottom Left
		glVertex3f( 1.0f,-1.0f, 0.0f);					// Bottom Right
	glEnd();											// Finished Drawing The Triangle
	glTranslatef(3.0f,0.0f,0.0f);						// Move Right 3 Units
	glBegin(GL_QUADS);									// Draw A Quad
		glVertex3f(-1.0f, 1.0f, 0.0f);					// Top Left
		glVertex3f( 1.0f, 1.0f, 0.0f);					// Top Right
		glVertex3f( 1.0f,-1.0f, 0.0f);					// Bottom Right
		glVertex3f(-1.0f,-1.0f, 0.0f);					// Bottom Left
	glEnd();											// Done Drawing The Quad

        SwapBuffers(hDC);
}

void opengl_blit_memtoscreen_8(int x, int y, int w, int h)
{
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        SwapBuffers(hDC);
}
