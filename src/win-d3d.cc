#include <stdint.h>
#define BITMAP WINDOWS_BITMAP
#include <d3d9.h>
#undef BITMAP
#include "win-d3d.h"
#include "video.h"

extern "C" void fatal(const char *format, ...);
extern "C" void pclog(const char *format, ...);

void d3d_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h);
void d3d_blit_memtoscreen_8(int x, int y, int w, int h);

LPDIRECT3D9             d3d        = NULL;
LPDIRECT3DDEVICE9       d3ddev     = NULL; 
LPDIRECT3DVERTEXBUFFER9 v_buffer   = NULL;
LPDIRECT3DTEXTURE9      d3dTexture = NULL;
D3DPRESENT_PARAMETERS d3dpp;

HWND d3d_hwnd;

struct CUSTOMVERTEX
{
     FLOAT x, y, z, rhw;    // from the D3DFVF_XYZRHW flag
     DWORD color;    // from the D3DFVF_DIFFUSE flag
     FLOAT tu, tv;
};

CUSTOMVERTEX OurVertices[] =
{
     {  0.0f,   0.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(0, 0, 255), 0.0f, 0.0f},
     {2048.0f, 2048.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(0, 255, 0), 1.0f, 1.0f},
     {  0.0f, 2048.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(255, 0, 0), 0.0f, 1.0f},

     {  0.0f,   0.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(0, 0, 255), 0.0f, 0.0f},
     {2048.0f,   0.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(0, 255, 0), 1.0f, 0.0f},
     {2048.0f, 2048.0f, 1.0f, 1.0f, D3DCOLOR_XRGB(255, 0, 0), 1.0f, 1.0f},
};
  
void d3d_init(HWND h)
{
        VOID* pVoid;    // the void pointer
        D3DLOCKED_RECT dr;
        RECT r;
        int x, y;
        uint32_t *p;
        
        d3d_hwnd = h;
        
        d3d = Direct3DCreate9(D3D_SDK_VERSION);

        memset(&d3dpp, 0, sizeof(d3dpp));      

    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_FLIP;
    d3dpp.hDeviceWindow          = h;
    d3dpp.BackBufferCount        = 1;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality     = 0;
    d3dpp.EnableAutoDepthStencil = false;
    d3dpp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.Windowed               = true;
    d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
    d3dpp.BackBufferWidth        = 0;
    d3dpp.BackBufferHeight       = 0;

        d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, h, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3ddev);
        
        d3ddev->SetTextureStageState(0,D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        d3ddev->SetTextureStageState(0,D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d3ddev->SetTextureStageState(0,D3DTSS_ALPHAOP,   D3DTOP_DISABLE);

        d3ddev->SetSamplerState(0,D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        d3ddev->SetSamplerState(0,D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        
    // create the vertex and store the pointer into v_buffer, which is created globally
     d3ddev->CreateVertexBuffer(6*sizeof(CUSTOMVERTEX),
                                0,
                                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                D3DPOOL_MANAGED,
                                &v_buffer,
                                NULL);


     v_buffer->Lock(0, 0, (void**)&pVoid, 0);    // lock the vertex buffer
     memcpy(pVoid, OurVertices, sizeof(OurVertices));    // copy the vertices to the locked buffer
     v_buffer->Unlock();    // unlock the vertex buffer
     
     d3ddev->CreateTexture(2048, 2048, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &d3dTexture, NULL);
     
        r.top    = r.left  = 0;
        r.bottom = r.right = 2047;
        
        if (FAILED(d3dTexture->LockRect(0, &dr, &r, 0)))
           fatal("LockRect failed\n");
        
        for (y = 0; y < 2048; y++)
        {
                p = (uint32_t *)(dr.pBits + (y * dr.Pitch));
                for (x = 0; x < 2048; x++)
                {
                        p[x] = x + (y << 11);
                }
        }
        
        d3dTexture->UnlockRect(0);
        
//        atexit(d3d_close);
        
        video_blit_memtoscreen = d3d_blit_memtoscreen;
        video_blit_memtoscreen_8 = d3d_blit_memtoscreen_8;
}

void d3d_resize(int x, int y)
{
//        d3d_close();
//        d3d_init(d3d_hwnd);
        d3dpp.BackBufferWidth        = 0;
        d3dpp.BackBufferHeight       = 0;
        d3ddev->Reset(&d3dpp);
}
        
void d3d_close()
{
        if (d3dTexture)
        {
                d3dTexture->Release();
                d3dTexture = NULL;
        }
        if (v_buffer)
        {
                v_buffer->Release();
                v_buffer = NULL;
        }
        if (d3ddev)
        {
                d3ddev->Release();
                d3ddev = NULL;
        }
        if (d3d)
        {
                d3d->Release();
                d3d = NULL;
        }
}

void d3d_blit_memtoscreen(int x, int y, int y1, int y2, int w, int h)
{
        VOID* pVoid;
        float xmax, ymax;
        D3DLOCKED_RECT dr;
        RECT r;
        uint32_t *p, *src;
        int yy;
        
        xmax = (float)w / 2048.0;
        ymax = (float)h / 2048.0;        
        
        OurVertices[1].tu = OurVertices[4].tu = OurVertices[5].tu = xmax;
        OurVertices[1].tv = OurVertices[2].tv = OurVertices[5].tv = ymax;

        GetClientRect(d3d_hwnd, &r);
        OurVertices[1].x = OurVertices[4].x = OurVertices[5].x = r.right  - r.left;
        OurVertices[1].y = OurVertices[2].y = OurVertices[5].y = r.bottom - r.top;
        
        //pclog("Window %i, %i\n", 

     v_buffer->Lock(0, 0, (void**)&pVoid, 0);    // lock the vertex buffer
     memcpy(pVoid, OurVertices, sizeof(OurVertices));    // copy the vertices to the locked buffer
     v_buffer->Unlock();    // unlock the vertex buffer
    // clear the window to a deep blue
     //d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 40, 100), 1.0f, 0);

        r.top    = y1;
        r.left   = 0;
        r.bottom = y2;
        r.right  = 2047;
        
        if (FAILED(d3dTexture->LockRect(0, &dr, &r, 0)))
           fatal("LockRect failed\n");
        
        for (yy = y1; yy < y2; yy++)
            memcpy(dr.pBits + (yy * dr.Pitch), &(((uint32_t *)buffer32->line[y + yy])[x]), w * 4);

        d3dTexture->UnlockRect(0);

        
//        d3dpp.BackBufferWidth  = r.right - r.left;
//	d3dpp.BackBufferHeight = r.bottom - r.top;
	
//	d3ddev->Reset(&d3dpp);
		
     d3ddev->BeginScene();    // begins the 3D scene

        d3ddev->SetTexture( 0, d3dTexture );
        // select which vertex format we are using
         d3ddev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

         // select the vertex buffer to display
         d3ddev->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));

         // copy the vertex buffer to the back buffer
        d3ddev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
        
        d3ddev->SetTexture( 0, NULL );

     d3ddev->EndScene();    // ends the 3D scene

     d3ddev->Present(NULL, NULL, d3d_hwnd, NULL);    // displays the created frame
}

void d3d_blit_memtoscreen_8(int x, int y, int w, int h)
{
    // clear the window to a deep blue
     d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 40, 100), 1.0f, 0);

     d3ddev->BeginScene();    // begins the 3D scene

     // do 3D rendering on the back buffer here

     d3ddev->EndScene();    // ends the 3D scene

     d3ddev->Present(NULL, NULL, NULL, NULL);    // displays the created frame
}
