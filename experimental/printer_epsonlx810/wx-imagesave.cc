#include "lpt_epsonlx810.h"

#include <wx/xrc/xmlres.h>
#include <wx/wfstream.h>
#include <wx/log.h>

int wx_image_save_fullpath(const char* fullpath, const char* format, unsigned char* rgba, int width, int height, int alpha)
{
        int x, y;
        wxLogNull logNull;
        wxImage image(width, height);
        if (alpha)
        {
                // these will be automatically freed
                unsigned char* rgb = (unsigned char*)malloc(width*height*3);
                unsigned char* a = (unsigned char*)malloc(width*height);
                for (x = 0; x < width; ++x)
                {
                        for (y = 0; y < height; ++y)
                        {
                                rgb[(y*width+x)*3+0] = rgba[(y*width+x)*4+0];
                                rgb[(y*width+x)*3+1] = rgba[(y*width+x)*4+1];
                                rgb[(y*width+x)*3+2] = rgba[(y*width+x)*4+2];
                                a[y*width+x] = rgba[(y*width+x)*4+3];
                        }
                }
                image.SetData(rgb);
                image.SetAlpha(a);
        }
        else
                image.SetData(rgba, true);

        wxImageHandler* h;

        if (!strcmp(format, IMAGE_TIFF))
                h = new wxTIFFHandler();
        else if (!strcmp(format, IMAGE_BMP))
                h = new wxBMPHandler();
        else if (!strcmp(format, IMAGE_JPG))
                h = new wxJPEGHandler();
        else
                h = new wxPNGHandler();

        int res = 0;
        if (h)
        {
                wxString p(fullpath);

                wxFileOutputStream stream(p);
                res = h->SaveFile(&image, stream, false);
                delete h;
        }

        return res;
}