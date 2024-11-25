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

class ViewerFont: public Viewer
{
private:
	svga_t *svga;
	wxImage buffer;

	void OnPaint(wxPaintEvent &event)
	{
		wxPaintDC dc(this);
		wxCoord w, h;

		dc.GetSize(&w, &h);

		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();

		unsigned char *buffer_data = buffer.GetData();
		int buffer_width = buffer.GetWidth();

		for (int y = 0; y < 16; y++)
		{
			int font_base = ((y & 8) ? svga->charsetb : svga->charseta) + ((y & 7) * 32 * 128);

			for (int x = 0; x < 32; x++)
			{
				int font_addr = font_base + x * 128;

				for (int yy = 0; yy < 16; yy++)
				{
					unsigned char *data = buffer_data + (y * 16 + yy) * 3 * buffer_width + x * 8 * 3;

					for (int xx = 0; xx < 8; xx++)
					{
						*data++ = (svga->vram[font_addr] & (0x80 >> xx)) ? 0xff : 0;
						*data++ = (svga->vram[font_addr] & (0x80 >> xx)) ? 0xff : 0;
						*data++ = (svga->vram[font_addr] & (0x80 >> xx)) ? 0xff : 0;
					}

					font_addr += 4;
				}
			}
		}

		wxBitmap bitmap(buffer);

		wxMemoryDC mdc(bitmap);

		dc.StretchBlit(0, 0, w, h, &mdc, 0, 0, 256, 256);

	}

	void OnClose(wxCloseEvent &event)
	{
		viewer_remove(this);
		event.Skip();
	}

public:
	ViewerFont(wxWindow *parent, wxString title, wxSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(256, 256)
	{
	        Bind(wxEVT_CLOSE_WINDOW, &ViewerFont::OnClose, this);
		Bind(wxEVT_PAINT, &ViewerFont::OnPaint, this);
	}

	virtual ~ViewerFont()
	{
	}
};

static void *viewer_font_open(void *parent, void *p, const char *title)
{
	wxFrame *w = new ViewerFont((wxWindow *)parent, title, wxSize(256, 256), p);

	w->Show(true);

	return w;
}

viewer_t viewer_font =
{
	.open = viewer_font_open
};
