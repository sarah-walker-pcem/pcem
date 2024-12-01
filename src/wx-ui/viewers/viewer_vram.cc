#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/rawbmp.h>
#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>
#include <string>
#include "viewer.h"
extern "C"
{
#include "ibm.h"
#include "mem.h"
#include "video.h"
#include "vid_svga.h"
}

enum ColourDepth
{
	CD_1BPP,
	CD_2BPP,
	CD_4BPP,
	CD_8BPP,
	CD_1555,
	CD_565,
	CD_888,
	CD_8888
};

enum AddrMode
{
	AM_NORMAL,
	AM_ODDEVEN,
	AM_CHAIN4
};

class ViewerVRAM;

class ViewerVRAMCanvas: public wxWindow
{
private:
	class ViewerVRAM *vram_parent;
	svga_t *svga;
	wxImage buffer;
	wxSize window_size;
	bool size_changed;
	uint32_t custom_start_addr;
	uint32_t custom_pitch;
	ColourDepth custom_colour_depth;
	AddrMode custom_addr_mode;
	bool use_custom_start_addr;
	bool use_custom_pitch;
	bool use_custom_colour_depth;
	bool use_custom_addr_mode;

	int scroll_x_size, scroll_x_range;
	int scroll_y_size, scroll_y_range;

	uint32_t current_start_addr;
	uint32_t current_pitch;
	int current_bpp;

	int scale_factor;

	void update_scroll(int new_x_size, int new_x_range, int new_y_size, int new_y_range)
	{
		scroll_x_size = new_x_size;
		scroll_x_range = new_x_range;
		scroll_y_size = new_y_size;
		scroll_y_range = new_y_range;

		SetScrollbar(wxHORIZONTAL, GetScrollPos(wxHORIZONTAL), new_x_size, new_x_range);
		SetScrollbar(wxVERTICAL, GetScrollPos(wxVERTICAL), new_y_size, new_y_range);
	}

	void UpdateLabels();

	void OnPaint(wxPaintEvent &event)
	{
		wxPaintDC dc(this);
		wxCoord w, h, disp_w, disp_h;

		dc.GetSize(&disp_w, &disp_h);
		w = disp_w / scale_factor;
		h = disp_h / scale_factor;

		dc.SetBackground(*wxWHITE_BRUSH);
		dc.Clear();

		uint32_t addr = use_custom_start_addr ? custom_start_addr : svga->ma_latch * 4;
		uint32_t mask = (svga->vram_mask << 2) | 3;

		ColourDepth depth = custom_colour_depth;

		if (!use_custom_colour_depth) {
			switch (svga->video_bpp) {
				case 2:
				depth = CD_2BPP;
				break;
				case 4:
				depth = CD_4BPP;
				break;
				case 8:
				depth = CD_8BPP;
				break;
				case 15:
				depth = CD_1555;
				break;
				case 16:
				depth = CD_565;
				break;
				case 24:
				depth = CD_888;
				break;
				case 32:
				depth = CD_8888;
				break;
				default:
				depth = CD_8BPP;
				break;
			}
		}

		uint32_t offset = use_custom_pitch ? custom_pitch * 8 : svga->rowoffset * 8;
		int plot_width = 0;
		int line_width = 0;

		switch (depth)
		{
			case CD_1BPP:
			plot_width = std::min((int)(w & ~3), (int)offset*8);
			line_width = offset * 8;
			break;

			case CD_2BPP:
			plot_width = std::min((int)(w & ~3), (int)offset*4);
			line_width = offset * 4;
			break;

			case CD_4BPP:
			plot_width = std::min((int)(w & ~3), (int)offset*2);
			line_width = offset * 2;
			break;

			case CD_8BPP:
			plot_width = std::min((int)(w & ~3), (int)offset);
			line_width = offset;
			break;

			case CD_1555:
			case CD_565:
			plot_width = std::min((int)(w & ~3), (int)offset / 2);
			line_width = offset / 2;
			break;

			case CD_888:
			plot_width = std::min((int)(w & ~3), (int)offset / 3);
			line_width = offset / 3;
			break;

			case CD_8888:
			plot_width = std::min((int)(w & ~3), (int)offset / 4);
			line_width = offset / 4;
			break;
		}

		AddrMode addr_mode = custom_addr_mode;

		if (!use_custom_addr_mode)
		{
			if (svga->fb_only)
				addr_mode = AM_NORMAL;
			else if (svga->chain4 && !svga->packed_chain4)
				addr_mode = AM_CHAIN4;
			else if (svga->chain2_write || svga->chain2_read)
				addr_mode = AM_ODDEVEN;
			else
				addr_mode = AM_NORMAL;
		}

		int vram_height = offset ? (svga->vram_max / offset) : 1;

		if (scroll_x_size != w || scroll_x_range != line_width || scroll_y_size != h || scroll_y_range != vram_height)
			update_scroll(w, line_width, h, vram_height);

		int x_offset = GetScrollPos(wxHORIZONTAL);
		int y_offset = GetScrollPos(wxVERTICAL);
		unsigned char *buffer_data = buffer.GetData();

		for (int y = 0; y < h; y++)
		{
			unsigned char *data = buffer_data + y*3*buffer.GetWidth();
			uint32_t old_addr = addr;

			addr += y_offset * offset;

			switch (depth)
			{
				case CD_1BPP:
				addr += (x_offset / 2) & ~3;
				for (int x = 0; x < plot_width; x += 8)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 8; xx++)
					{
						uint8_t pixel = 0;

						if (svga->vram[use_addr & svga->vram_mask] & (0x80 >> xx))
							pixel |= 1;

						*data++ = svga->vgapal[svga->egapal[pixel]].r * 4;
						*data++ = svga->vgapal[svga->egapal[pixel]].g * 4;
						*data++ = svga->vgapal[svga->egapal[pixel]].b * 4;
					}

					addr += 4;
				}
				break;

				case CD_2BPP:
				for (int x = 0; x < plot_width; x+= 4)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					uint16_t pix_data = *(uint16_t *)&svga->vram[use_addr & svga->vram_mask];

					for (int xx = 0; xx < 8; xx++)
					{
						uint8_t pixel = pix_data >> 14;

						*data++ = svga->vgapal[pixel].r * 4;
						*data++ = svga->vgapal[pixel].g * 4;
						*data++ = svga->vgapal[pixel].b * 4;

						pix_data <<= 2;
					}

					addr += 4;
				}
				break;

				case CD_4BPP:
				addr += (x_offset / 2) & ~3;
				for (int x = 0; x < plot_width; x += 8)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 8; xx++)
					{
						uint8_t pixel = 0;

						if (svga->vram[use_addr & svga->vram_mask] & (0x80 >> xx))
							pixel |= 1;
						if (svga->vram[(use_addr + 1) & svga->vram_mask] & (0x80 >> xx))
							pixel |= 2;
						if (svga->vram[(use_addr + 2) & svga->vram_mask] & (0x80 >> xx))
							pixel |= 4;
						if (svga->vram[(use_addr + 3) & svga->vram_mask] & (0x80 >> xx))
							pixel |= 8;

						*data++ = svga->vgapal[svga->egapal[pixel]].r * 4;
						*data++ = svga->vgapal[svga->egapal[pixel]].g * 4;
						*data++ = svga->vgapal[svga->egapal[pixel]].b * 4;
					}

					addr += 4;
				}
				break;

				case CD_8BPP:
				addr += x_offset & ~3;
				for (int x = 0; x < plot_width; x += 4)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 4; xx++)
					{
						uint8_t pixel = svga->vram[(use_addr + xx) & svga->vram_mask];

						*data++ = svga->vgapal[pixel].r * 4;
						*data++ = svga->vgapal[pixel].g * 4;
						*data++ = svga->vgapal[pixel].b * 4;
					}

					addr += 4;
				}
				break;

				case CD_1555:
				addr += (x_offset * 2) & ~3;
				for (int x = 0; x < plot_width; x += 2)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 2; xx++)
					{
						uint16_t pixel = *(uint16_t *)&svga->vram[(use_addr + xx*2) & svga->vram_mask];

						*data++ = video_15to32[pixel] >> 16;
						*data++ = video_15to32[pixel] >> 8;
						*data++ = video_15to32[pixel] & 0xff;
					}

					addr += 4;
				}
				break;

				case CD_565:
				addr += (x_offset * 2) & ~3;
				for (int x = 0; x < plot_width; x += 2)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 2; xx++)
					{
						uint16_t pixel = *(uint16_t *)&svga->vram[(use_addr + xx*2) & svga->vram_mask];

						*data++ = video_16to32[pixel] >> 16;
						*data++ = video_16to32[pixel] >> 8;
						*data++ = video_16to32[pixel] & 0xff;
					}

					addr += 4;
				}
				break;

				case CD_888:
				addr += (x_offset & ~3) * 3;
				for (int x = 0; x < plot_width; x += 4)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					for (int xx = 0; xx < 4; xx++)
					{
						*data++ = svga->vram[(use_addr + xx*3 + 2) & svga->vram_mask];
						*data++ = svga->vram[(use_addr + xx*3 + 1) & svga->vram_mask];
						*data++ = svga->vram[(use_addr + xx*3) & svga->vram_mask];
					}

					addr += 12;
				}
				break;

				case CD_8888:
				addr += x_offset * 4;
				for (int x = 0; x < plot_width; x++)
				{
					uint32_t use_addr = addr;

					if (addr_mode == AM_CHAIN4)
						use_addr = ((addr & 0xfffc) << 2) | ((addr & 0x30000) >> 14) | (addr & ~0x3ffff);
					else if (addr_mode == AM_ODDEVEN)
						use_addr = ((addr << 1) & 0x1fff8) | ((addr >> 15) & 0x4) | (addr & ~0x1ffff);

					uint32_t pixel = *(uint32_t *)&svga->vram[use_addr & svga->vram_mask];

					*data++ = pixel >> 16;
					*data++ = pixel >> 8;
					*data++ = pixel & 0xff;

					addr += 4;
				}
				break;
			}

			addr = old_addr + offset;
		}

		wxBitmap bitmap(buffer);

		wxMemoryDC mdc(bitmap);

		if (scale_factor == 1)
			dc.Blit(0, 0, plot_width, h, &mdc, 0, 0);
		else
			dc.StretchBlit(0, 0, plot_width * scale_factor, h * scale_factor, &mdc, 0, 0, plot_width, h);

		if (current_start_addr != svga->ma_latch * 4 || current_pitch != svga->rowoffset || current_bpp != svga->video_bpp)
			UpdateLabels();
	}

	void OnSize(wxSizeEvent &event)
	{
		window_size = event.GetSize();
	}

	void OnScroll(wxScrollWinEvent &event)
	{
		Refresh();
	}

public:
	ViewerVRAMCanvas(wxWindow *parent, const wxString& title, const wxPoint& pos, const wxSize& size, svga_t *svga, class ViewerVRAM *vram_parent)
	: wxWindow(parent, wxID_ANY, pos, size, wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL | wxALWAYS_SHOW_SB),
	  svga(svga),
	  buffer(4096, 4096),
	  window_size(wxSize(1, 1)),
	  size_changed(false),
	  custom_pitch(10),
	  use_custom_start_addr(true),    /*Start with VRAM address fixed to 0*/
	  use_custom_pitch(false),
	  use_custom_colour_depth(false),
	  use_custom_addr_mode(false),
	  custom_start_addr(0),
	  vram_parent(vram_parent),
	  scale_factor(1)
	{
		Bind(wxEVT_PAINT, &ViewerVRAMCanvas::OnPaint, this);
		Bind(wxEVT_SIZE, &ViewerVRAMCanvas::OnSize, this);
		Bind(wxEVT_SCROLLWIN_TOP, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_BOTTOM, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_LINEUP, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_LINEDOWN, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_PAGEUP, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_PAGEDOWN, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_THUMBTRACK, &ViewerVRAMCanvas::OnScroll, this);
		Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &ViewerVRAMCanvas::OnScroll, this);
	}

	virtual ~ViewerVRAMCanvas()
	{
	}

	void set_custom_start_addr(uint32_t addr)
	{
		custom_start_addr = addr;
		Refresh();
	}

	void set_use_custom_start_addr(bool use)
	{
		use_custom_start_addr = use;
		Refresh();
	}

	void set_custom_pitch(uint32_t pitch)
	{
		custom_pitch = pitch;
		Refresh();
	}

	void set_use_custom_pitch(bool use)
	{
		use_custom_pitch = use;
		Refresh();
	}

	void set_custom_colour_depth(ColourDepth depth)
	{
		custom_colour_depth = depth;
		Refresh();
	}

	void set_use_custom_colour_depth(bool use)
	{
		use_custom_colour_depth = use;
		Refresh();
	}

	void set_custom_addr_mode(AddrMode mode)
	{
		custom_addr_mode = mode;
		Refresh();
	}

	void set_use_custom_addr_mode(bool use)
	{
		use_custom_addr_mode = use;
		Refresh();
	}

	void set_scale_factor(int new_scale_factor)
	{
		scale_factor = new_scale_factor;
		Refresh();
	}
};

class ViewerVRAM: public Viewer
{
private:
	ViewerVRAMCanvas *canvas;
	wxBoxSizer *sz;
	wxPanel *panel;
	wxFrame *temp_frame;

	void OnClose(wxCloseEvent &event)
	{
		viewer_remove(this);
		event.Skip();
	}

	void OnRadioButton(wxCommandEvent &event)
	{
		wxWindowID id = event.GetId();

		if (id == XRCID("IDC_STARTADDR_CURRENT")) {
			canvas->set_use_custom_start_addr(false);
		} else if (id == XRCID("IDC_STARTADDR_CUSTOM")) {
			canvas->set_use_custom_start_addr(true);
		} else if (id == XRCID("IDC_PITCH_CURRENT")) {
			canvas->set_use_custom_pitch(false);
		} else if (id == XRCID("IDC_PITCH_CUSTOM")) {
			canvas->set_use_custom_pitch(true);
		} else if (id == XRCID("IDC_DEPTH_CURRENT")) {
			canvas->set_use_custom_colour_depth(false);
		} else if (id == XRCID("IDC_DEPTH_1BPP")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_1BPP);
		} else if (id == XRCID("IDC_DEPTH_2BPP")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_2BPP);
		} else if (id == XRCID("IDC_DEPTH_4BPP")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_4BPP);
		} else if (id == XRCID("IDC_DEPTH_8BPP")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_8BPP);
		} else if (id == XRCID("IDC_DEPTH_1555")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_1555);
		} else if (id == XRCID("IDC_DEPTH_565")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_565);
		} else if (id == XRCID("IDC_DEPTH_888")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_888);
		} else if (id == XRCID("IDC_DEPTH_8888")) {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_8888);
		} else if (id == XRCID("IDC_ADDRMODE_CURRENT")) {
			canvas->set_use_custom_addr_mode(false);
		} else if (id == XRCID("IDC_ADDRMODE_NORMAL")) {
			canvas->set_use_custom_addr_mode(true);
			canvas->set_custom_addr_mode(AM_NORMAL);
		} else if (id == XRCID("IDC_ADDRMODE_ODDEVEN")) {
			canvas->set_use_custom_addr_mode(false);
			canvas->set_custom_addr_mode(AM_ODDEVEN);
		} else if (id == XRCID("IDC_ADDRMODE_CHAIN4")) {
			canvas->set_use_custom_addr_mode(false);
			canvas->set_custom_addr_mode(AM_CHAIN4);
		}
	}

	void OnText(wxCommandEvent &event)
	{
		wxWindowID id = event.GetId();

		if (id == XRCID("IDC_STARTADDR_TEXTCTRL")) {
			unsigned long addr = 0;
			wxTextCtrl *text_ctrl = static_cast<wxTextCtrl *>(event.GetEventObject());

			text_ctrl->GetValue().ToULong(&addr, 16);
			canvas->set_custom_start_addr(addr);
		} else if (id == XRCID("IDC_PITCH_TEXTCTRL")) {
			unsigned long pitch = 0;
			wxTextCtrl *text_ctrl = static_cast<wxTextCtrl *>(event.GetEventObject());

			text_ctrl->GetValue().ToULong(&pitch, 10);
			canvas->set_custom_pitch(pitch);
		}
	}

	void OnSpinCtrl(wxSpinEvent &event)
	{
		wxWindowID id = event.GetId();

		if (id == XRCID("IDC_SCALE")) {
			int scale_factor = event.GetPosition();

			canvas->set_scale_factor(scale_factor);
		}
	}

public:
	ViewerVRAM(wxWindow *parent, wxString title, wxSize size, void *p)
	: Viewer(parent, title, size, p)
	{
		panel = wxXmlResource::Get()->LoadPanel(this, "ViewerVRAMPanel");

		sz = new wxBoxSizer(wxHORIZONTAL);
		sz->Add(panel, 0, wxEXPAND | wxALL, 0);

		canvas = new ViewerVRAMCanvas(this, "vram", wxDefaultPosition, wxDefaultSize, (svga_t *)p, this);
		sz->Add(canvas, 1, wxEXPAND | wxALL, 0);
		SetSizer(sz);

	        Bind(wxEVT_CLOSE_WINDOW, &ViewerVRAM::OnClose, this);

		panel->Bind(wxEVT_RADIOBUTTON, &ViewerVRAM::OnRadioButton, this);
		panel->Bind(wxEVT_TEXT_ENTER, &ViewerVRAM::OnText, this);
		panel->Bind(wxEVT_SPINCTRL, &ViewerVRAM::OnSpinCtrl, this);
	}

	virtual ~ViewerVRAM()
	{
		delete canvas;
	}
};

void ViewerVRAMCanvas::UpdateLabels()
{
	current_start_addr = svga->ma_latch * 4;
	current_pitch = svga->rowoffset;
	current_bpp = svga->video_bpp;

	char s[128];

	wxRadioButton *rb = static_cast<wxRadioButton *>(vram_parent->FindWindow("IDC_STARTADDR_CURRENT"));
	sprintf(s, "Current (%x)", current_start_addr);
	rb->SetLabel(wxString(s));

	rb = static_cast<wxRadioButton *>(vram_parent->FindWindow("IDC_PITCH_CURRENT"));
	sprintf(s, "Current (%u)", current_pitch);
	rb->SetLabel(wxString(s));

	rb = static_cast<wxRadioButton *>(vram_parent->FindWindow("IDC_DEPTH_CURRENT"));
	switch (current_bpp)
	{
		case 1:
		rb->SetLabel("Current (2 cols)");
		break;
		case 2:
		rb->SetLabel("Current (4 cols)");
		break;
		case 4:
		rb->SetLabel("Current (16 cols)");
		break;
		case 8:
		rb->SetLabel("Current (256 cols)");
		break;
		case 15:
		rb->SetLabel("Current (1555)");
		break;
		case 16:
		rb->SetLabel("Current (555)");
		break;
		case 24:
		rb->SetLabel("Current (888)");
		break;
		case 32:
		rb->SetLabel("Current (8888)");
		break;
		default:
		rb->SetLabel("Current");
		break;
	}
}

static void *viewer_vram_open(void *parent, void *p, const char *title)
{
	wxFrame *w = new ViewerVRAM((wxWindow *)parent, title, wxDefaultSize, p);

	w->Show(true);

	return w;
}

viewer_t viewer_vram =
{
	.open = viewer_vram_open
};
