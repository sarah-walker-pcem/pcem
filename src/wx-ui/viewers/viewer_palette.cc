#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/rawbmp.h>
#include "viewer.h"
extern "C"
{
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
}

class ViewerPalette: public Viewer
{
private:
	svga_t *svga;
	wxBitmap buffer;

	void OnPaint(wxPaintEvent &event)
	{
		wxPaintDC dc(this);
		wxCoord w, h;

		dc.GetSize(&w, &h);

		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();

		{
			wxNativePixelData data(buffer);
			wxNativePixelData::Iterator p(data);

			for (int y = 0; y < 16; y++)
			{
    				wxNativePixelData::Iterator rowStart = p;

				for (int x = 0; x < 16; x++)
				{
					p.Red() = svga->vgapal[x + y*16].r * 4;
					p.Green() = svga->vgapal[x + y*16].g * 4;
					p.Blue() = svga->vgapal[x + y*16].b * 4;
					p++;
				}

				p = rowStart;
				p.OffsetY(data, 1);
			}
		}

		wxMemoryDC mdc(buffer);

		dc.StretchBlit(0, 0, w, h, &mdc, 0, 0, 16, 16);
	}

	void OnClose(wxCloseEvent &event)
	{
		viewer_remove(this);
		event.Skip();
	}

public:
	ViewerPalette(wxWindow *parent, wxString title, wxSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(16, 16, 24)
	{
	        Bind(wxEVT_CLOSE_WINDOW, &ViewerPalette::OnClose, this);
		Bind(wxEVT_PAINT, &ViewerPalette::OnPaint, this);
	}

	virtual ~ViewerPalette()
	{
	}
};

static void *viewer_palette_open(void *parent, void *p, const char *title)
{
	wxFrame *w = new ViewerPalette((wxWindow *)parent, title, wxSize(256, 256), p);

	w->Show(true);

	return w;
}

viewer_t viewer_palette =
{
	.open = viewer_palette_open
};

class ViewerPalette16: public Viewer
{
private:
	svga_t *svga;
	wxBitmap buffer;

	void OnPaint(wxPaintEvent &event)
	{
		wxPaintDC dc(this);
		wxCoord w, h;

		dc.GetSize(&w, &h);

		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();

		{
			wxNativePixelData data(buffer);
			wxNativePixelData::Iterator p(data);

			for (int x = 0; x < 16; x++)
			{
				p.Red() = svga->vgapal[svga->egapal[x]].r * 4;
				p.Green() = svga->vgapal[svga->egapal[x]].g * 4;
				p.Blue() = svga->vgapal[svga->egapal[x]].b * 4;
				p++;
			}
		}

		wxMemoryDC mdc(buffer);

		dc.StretchBlit(0, 0, w, h, &mdc, 0, 0, 16, 1);
	}

	void OnClose(wxCloseEvent &event)
	{
		viewer_remove(this);
		event.Skip();
	}

public:
	ViewerPalette16(wxWindow *parent, wxString title, wxSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(16, 1, 24)
	{
	        Bind(wxEVT_CLOSE_WINDOW, &ViewerPalette16::OnClose, this);
		Bind(wxEVT_PAINT, &ViewerPalette16::OnPaint, this);
	}

	virtual ~ViewerPalette16()
	{
	}
};

static void *viewer_palette_16_open(void *parent, void *p, const char *title)
{
	wxFrame *w = new ViewerPalette16((wxWindow *)parent, title, wxSize(256, 20), p);

	w->Show(true);

	return w;
}

viewer_t viewer_palette_16 =
{
	.open = viewer_palette_16_open
};
