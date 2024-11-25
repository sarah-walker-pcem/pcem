#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/dataview.h>
#include <wx/grid.h>
#include <wx/notebook.h>
#include <wx/rawbmp.h>
#include <wx/splitter.h>
#include <wx/treebase.h>
#include <wx/xrc/xmlres.h>
#include <list>
#include <mutex>
#include <string>
#include <vector>
#include "viewer.h"
#include "viewer_voodoo.h"
extern "C"
{
#include "ibm.h"
#include "mem.h"
#include "thread.h"
#include "video.h"
#include "vid_svga.h"
#include "vid_voodoo_common.h"
#include "vid_voodoo_regs.h"
#include "vid_voodoo_texture.h"
}

static std::mutex voodoo_viewer_refcount_mutex;

static const char *texture_format_names[16] = {
	"8-bit RGB (3-3-2)",
	"8-bit YIQ (4-4-2)",
	"8-bit Alpha",
	"8-bit Intensity",
	"8-bit Alpha, Intensity (4-4)",
	"8-bit Palette (RGB)",
	"8-bit Palette (RGBA)",
	"Reserved",
	"16-bit ARGB (8-3-3-2)",
	"16-bit AYIQ (8-4-2-2)",
	"16-bit RGB (5-6-5)",
	"16-bit ARGB (1-5-5-5)",
	"16-bit ARGB (4-4-4-4)",
	"16-bit Alpha, Intensity (8-8)",
	"16-bit Alpha, Palette (8-8)",
	"Reserved"
};

class ViewerVoodoo;

enum VoodooDisplayMode
{
	DM_FRAMEBUFFER,
	DM_FRAMEBUFFER_WIREFRAME,
	DM_DEPTHBUFFER,
	DM_DEPTHBUFFER_WIREFRAME,
	DM_WIREFRAME
};

enum VoodooCommand
{
	CMD_TRIANGLE,
	CMD_START_STRIP, /*Covers fans as well as strips - some games will switch between the two in the middle of a run*/
	CMD_END_STRIP
};

typedef struct VoodooTriangle
{
	VoodooCommand cmd;
	int id;
	int strip_id;
	float x[3], y[3], z[3], w[3];
	float r[3], g[3], b[3], a[3];
	float s0[3], t0[3], w0[3];
	float s1[3], t1[3], w1[3];
	int texture[2];
	uint32_t textureMode[2];
	uint32_t chromaKey;
	uint32_t color0, color1;
	rgb_t fogColor;
	uint32_t zaColor;
	uint32_t fbzMode;
	uint32_t fbzColorPath;
	uint32_t alphaMode;
	uint32_t fogMode;
} VoodooTriangle;

class ViewerVoodooCanvas: public wxWindow
{
private:
	class ViewerVoodoo *voodoo_parent;
	voodoo_t *voodoo;
	wxImage buffer;
	wxImage depth_buffer;
	wxSize window_size;
	std::mutex buffer_mutex;
	int width, height;
	bool is_paused;
	std::list<VoodooTriangle> *triangle_list_display;
	int selected_tri_id;
	int selected_strip_id;
	int selected_texture_id;
	VoodooDisplayMode display_mode;

	void OnPaint(wxPaintEvent &event)
	{
		std::lock_guard<std::mutex> guard(buffer_mutex);

		if (!width || !height)
			return;

		wxPaintDC dc(this);
		wxCoord w, h;

		dc.GetSize(&w, &h);

		dc.SetBackground(*wxWHITE_BRUSH);
		dc.Clear();

		if (!is_paused) {
			wxCoord text_w, text_h;
			dc.GetTextExtent("Pause to update", &text_w, &text_h);
			dc.DrawText("Pause to update", (w / 2) - (text_w / 2), (h / 2) - (text_h / 2));
		} else {
			wxBitmap bitmap(width, height);
			wxMemoryDC mdc(bitmap);

			if (display_mode == DM_FRAMEBUFFER || display_mode == DM_FRAMEBUFFER_WIREFRAME) {
				wxBitmap framebuffer_bitmap(buffer);
				wxMemoryDC framebuffer_mdc(framebuffer_bitmap);

				mdc.Blit(0, 0, width, height, &framebuffer_mdc, 0, 0);
			} else if (display_mode == DM_DEPTHBUFFER || display_mode == DM_DEPTHBUFFER_WIREFRAME) {
				wxBitmap depth_bitmap(depth_buffer);
				wxMemoryDC depth_mdc(depth_bitmap);

				mdc.Blit(0, 0, width, height, &depth_mdc, 0, 0);
			} else {
				mdc.SetBackground(*wxBLACK_BRUSH);
				mdc.Clear();
			}

			mdc.SetPen(*wxWHITE_PEN);
			mdc.SetBrush(*wxRED_BRUSH);

			for (std::list<VoodooTriangle>::iterator it = triangle_list_display->begin(); it != triangle_list_display->end(); it++) {
				VoodooTriangle *tri = &*it;

				if (tri->id == selected_tri_id || tri->strip_id == selected_strip_id ||
				    ((tri->fbzColorPath & (1 << 27)) && (tri->texture[0] == selected_texture_id || (voodoo->dual_tmus && tri->texture[1] == selected_texture_id)))) {
					wxPoint points[3] = {
						wxPoint(tri->x[0], tri->y[0]),
						wxPoint(tri->x[1], tri->y[1]),
						wxPoint(tri->x[2], tri->y[2]),
					};
					mdc.DrawPolygon(3, points);
				} else if (display_mode == DM_WIREFRAME || display_mode == DM_DEPTHBUFFER_WIREFRAME || display_mode == DM_FRAMEBUFFER_WIREFRAME) {
					mdc.DrawLine(tri->x[0], tri->y[0], tri->x[1], tri->y[1]);
					mdc.DrawLine(tri->x[1], tri->y[1], tri->x[2], tri->y[2]);
					mdc.DrawLine(tri->x[2], tri->y[2], tri->x[0], tri->y[0]);
				}
			}

			dc.Blit(0, 0, width, height, &mdc, 0, 0);
		}
	}

	void OnSize(wxSizeEvent &event)
	{
		window_size = event.GetSize();
	}

public:
	ViewerVoodooCanvas(wxWindow *parent, const wxString& title, const wxPoint& pos, const wxSize& size, voodoo_t *voodoo, class ViewerVoodoo *voodoo_parent, std::list<VoodooTriangle> *triangle_list_display)
	: wxWindow(parent, wxID_ANY, pos, size, wxDEFAULT_FRAME_STYLE | wxVSCROLL | wxHSCROLL | wxALWAYS_SHOW_SB),
	  voodoo(voodoo),
	  buffer(4096, 4096),
	  depth_buffer(4096, 4096),
	  window_size(wxSize(1, 1)),
	  is_paused(false),
	  triangle_list_display(triangle_list_display),
	  selected_tri_id(-1),
	  selected_strip_id(-2),
	  selected_texture_id(-1),
	  display_mode(DM_FRAMEBUFFER_WIREFRAME)
	{
		Bind(wxEVT_PAINT, &ViewerVoodooCanvas::OnPaint, this);
		Bind(wxEVT_SIZE, &ViewerVoodooCanvas::OnSize, this);
	}

	void SwapBuffer()
	{
		{
			std::lock_guard<std::mutex> guard(buffer_mutex);

			width = voodoo->h_disp;
			height = voodoo->v_disp;

			unsigned char *buffer_data = buffer.GetData();
			for (int y = 0; y < voodoo->v_disp; y++) {
				unsigned char *data = buffer_data + y*3*buffer.GetWidth();

				if (voodoo->params.col_tiled) {
					uint16_t *src = (uint16_t *)&voodoo->fb_mem[voodoo->params.draw_offset + (y >> 5) * voodoo->params.row_width +
                                                				    (y & 31) * 128];

					for (int x = 0; x < voodoo->h_disp; x++) {
						int x_tiled = (x & 63) | ((x >> 6) * 128 * 32 / 2);
						uint32_t val = video_16to32[src[x_tiled]];

						*data++ = (val >> 16) & 0xff;
						*data++ = (val >> 8) & 0xff;
						*data++ = val & 0xff;
					}
				} else {
					uint16_t *src = (uint16_t *)&voodoo->fb_mem[voodoo->params.front_offset + y * voodoo->row_width];

					for (int x = 0; x < voodoo->h_disp; x++) {
						uint32_t val = video_16to32[*src++];

						*data++ = (val >> 16) & 0xff;
						*data++ = (val >> 8) & 0xff;
						*data++ = val & 0xff;
					}
				}
			}

			buffer_data = depth_buffer.GetData();
			for (int y = 0; y < voodoo->v_disp; y++) {
				unsigned char *data = buffer_data + y*3*depth_buffer.GetWidth();

				if (voodoo->params.aux_tiled) {
					uint16_t *src = (uint16_t *)&voodoo->fb_mem[voodoo->params.aux_offset + (y >> 5) * voodoo->params.row_width +
                                                				    (y & 31) * 128];

					for (int x = 0; x < voodoo->h_disp; x++) {
						int x_tiled = (x & 63) | ((x >> 6) * 128 * 32 / 2);
						uint16_t val = src[x_tiled];

						*data++ = (val >> 8) & 0xff;
						*data++ = (val >> 8) & 0xff;
						*data++ = (val >> 8) & 0xff;
					}
				} else {
					uint16_t *src = (uint16_t *)&voodoo->fb_mem[voodoo->params.aux_offset + y * voodoo->row_width];

					for (int x = 0; x < voodoo->h_disp; x++) {
						uint16_t val = *src++;

						*data++ = (val >> 8) & 0xff;
						*data++ = (val >> 8) & 0xff;
						*data++ = (val >> 8) & 0xff;
					}
				}
			}
		}

		Refresh();
	}

	void set_paused(bool new_is_paused)
	{
		is_paused = new_is_paused;
		if (!is_paused) {
			selected_tri_id = -1;
			selected_strip_id = -2;
			selected_texture_id = -1;
		}
		SetMinSize(wxSize(width, height));
		Refresh();
	}

	void set_selected_triangle(int id)
	{
		selected_tri_id = id;
		selected_strip_id = -2;
		selected_texture_id = -1;
		Refresh();
	}

	void set_selected_strip(int id)
	{
		selected_tri_id = -1;
		selected_strip_id = id;
		selected_texture_id = -1;
		Refresh();
	}

	void set_selected_texture(int id)
	{
		selected_tri_id = -1;
		selected_strip_id = -2;
		selected_texture_id = id;
		Refresh();
	}

	void set_display_mode(VoodooDisplayMode dm)
	{
		display_mode = dm;
		Refresh();
	}
};

#define TEXTURE_DATA_SIZE ((256 * 256 + 256 * 256 + 128 * 128 + 64 * 64 + 32 * 32 + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2) * 4)

class ViewerVoodoo;

class ViewerVoodooTextureBitmap: public wxStaticBitmap
{
private:
	ViewerVoodoo *viewer;
	int texture_id;

	void OnLeftDown(wxMouseEvent &event);

public:
	ViewerVoodooTextureBitmap(wxWindow *parent, const wxBitmap &bmp, const wxSize& size, ViewerVoodoo *viewer, int id)
	: wxStaticBitmap(parent, wxID_ANY, bmp, wxDefaultPosition, size),
	  viewer(viewer),
	  texture_id(id)
	{
		Bind(wxEVT_LEFT_DOWN, &ViewerVoodooTextureBitmap::OnLeftDown, this);
	}
};

class ViewerVoodooTexture: public wxBoxSizer
{
public:
	ViewerVoodooTexture(wxWindow *parent, const wxString &label, const wxBitmap &bmp, ViewerVoodoo *viewer, int id) :
		wxBoxSizer(wxVERTICAL)
	{
		ViewerVoodooTextureBitmap *sbmp = new ViewerVoodooTextureBitmap(parent, bmp, wxSize(128, 128), viewer, id);
		wxStaticText *text = new wxStaticText(parent, wxID_ANY, label);
		Add(sbmp);
		Add(text);
	}
};

typedef struct viewer_texture_t
{
	texture_t t;

	int w[10];
	int h[10];
	uint32_t offset[10];

	uint32_t textureMode;
	int tmu;
} viewer_texture_t;

class ViewerVoodoo: public Viewer
{
private:
	voodoo_t *voodoo;
	ViewerVoodooCanvas *canvas;
	wxComboBox *display_box;
	wxBoxSizer *display_sz;
	//wxPanel *display_panel;

	std::vector<ViewerVoodooTexture *> textures;
	std::vector<viewer_texture_t> texture_list_display;
	std::vector<viewer_texture_t> texture_list_active;
	std::vector<uint32_t *> texture_data_display;
	std::vector<uint32_t *> texture_data_active;
	bool is_paused;
	std::list<VoodooTriangle> triangle_list_display;
	std::list<VoodooTriangle> triangle_list_active;
	int triangle_next_id;
	int strip_next_id;
	int current_strip_id;
	int current_texture[2];

	wxTextCtrl *text_ctrl;
	wxDataViewTreeCtrl *dv_ctrl;
	wxTextCtrl *texture_text_ctrl;
	wxStaticBitmap *texture_bitmap_ctrl;
	wxGridSizer *texture_sz;
	wxScrolledWindow *texture_panel;

 	class TriangleItemData: public wxTreeItemData
	{
	private:
		VoodooTriangle *tri;

	public:
		TriangleItemData(VoodooTriangle *tri) : tri(tri)
		{
		}

		VoodooTriangle *GetTri()
		{
			return tri;
		}
	};

	void OnClose(wxCloseEvent &event)
	{
		viewer_remove(this);
		event.Skip();
	}

	void OnComboBox(wxCommandEvent &event)
	{
		switch (event.GetSelection()) {
		case 0:
			canvas->set_display_mode(DM_FRAMEBUFFER);
			break;
		case 1:
			canvas->set_display_mode(DM_FRAMEBUFFER_WIREFRAME);
			break;
		case 2:
			canvas->set_display_mode(DM_DEPTHBUFFER);
			break;
		case 3:
			canvas->set_display_mode(DM_DEPTHBUFFER_WIREFRAME);
			break;
		case 4:
			canvas->set_display_mode(DM_WIREFRAME);
			break;
		}
	}

	void UpdateDVCtrl()
	{
		dv_ctrl->DeleteAllItems();

		wxDataViewItem root = wxDataViewItem(0);

		for (std::list<VoodooTriangle>::iterator it = triangle_list_display.begin(); it != triangle_list_display.end(); it++) {
			VoodooTriangle tri = *it;
			wxString s;

			if (tri.cmd == CMD_TRIANGLE) {
				s.Printf("Triangle %u", tri.id);
				wxDataViewItem newitem = dv_ctrl->AppendItem(root, s);
				dv_ctrl->SetItemData(newitem, new TriangleItemData(&*it));
			} else if (tri.cmd == CMD_START_STRIP) {
				s.Printf("Strip %u", tri.id);
				root = dv_ctrl->AppendContainer(wxDataViewItem(0), s);
				dv_ctrl->SetItemData(root, new TriangleItemData(&*it));
			} else if (tri.cmd == CMD_END_STRIP) {
				root = wxDataViewItem(0);
			}
		}
	}

	void UpdateTextures()
	{
		int id = 0;

		for (std::vector<viewer_texture_t>::iterator it = texture_list_display.begin(); it != texture_list_display.end(); it++) {
			viewer_texture_t *tex = &*it;
			wxString label;

			label.Printf("Texture %u", id);

			int lod_min = (tex->t.tLOD >> 2) & 15;
		        lod_min = MIN(lod_min, 8);

			wxImage image(tex->w[lod_min], tex->h[lod_min]);
			unsigned char *buffer_data = image.GetData();

			int addr = texture_offset[lod_min];

			for (int y = 0; y < tex->h[lod_min]; y++) {
				unsigned char *data = buffer_data + y*3*image.GetWidth();

				for (int x = 0; x < tex->w[lod_min]; x++) {
					uint32_t val = texture_data_display[id][addr + x];

					*data++ = (val >> 16) & 0xff;
					*data++ = (val >> 8) & 0xff;
					*data++ = val & 0xff;
				}

				addr += (1 << (8 - lod_min));
			}

			if (tex->w[lod_min] > tex->h[lod_min]) {
				int h = (tex->h[lod_min] * 128) / tex->w[lod_min];
				image.Rescale(128, h, wxIMAGE_QUALITY_HIGH);
			} else {
				int w = (tex->w[lod_min] * 128) / tex->h[lod_min];
				image.Rescale(w, 128, wxIMAGE_QUALITY_HIGH);
			}

			ViewerVoodooTexture *vvt = new ViewerVoodooTexture(texture_panel, label, wxBitmap(image), this, id);

			texture_sz->Add(vvt);
			textures.push_back(vvt);

			id++;
		}

		texture_sz->Fit(texture_panel);
		texture_panel->Layout();

		wxSize size = texture_panel->GetBestVirtualSize();
		texture_panel->SetVirtualSize( size );

		texture_panel->SetScrollRate(15, 15);
		texture_panel->Refresh();
	}

	void ClearTextures()
	{
		texture_sz->Clear(true);
		texture_sz->Fit(texture_panel);

		texture_panel->Layout();
		texture_panel->SetScrollRate(15, 15);
		texture_panel->Refresh();

		textures.clear();
	}

	void OnSelectionChanged(wxDataViewEvent &event)
	{
		wxDataViewItem item = event.GetItem();
		TriangleItemData *tid = static_cast<TriangleItemData *>(dv_ctrl->GetItemData(item));

		canvas->set_selected_texture(-1);
		if (tid) {
			VoodooTriangle *tri = tid->GetTri();

			if (tri->cmd == CMD_TRIANGLE) {
				canvas->set_selected_triangle(tri->id);

				wxString s, s2;

				if (voodoo->dual_tmus) {
					s.Printf("Vertex 0:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=[%f,%f] T=[%f,%f]\n",
						 tri->x[0], tri->y[0], tri->z[0], tri->w[0], tri->r[0], tri->g[0], tri->b[0], tri->a[0], tri->s0[0], tri->s1[0], tri->t0[0], tri->t1[0]);

					s2.Printf("Vertex 1:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=[%f,%f] T=[%f,%f]\n",
						 tri->x[1], tri->y[1], tri->z[1], tri->w[1], tri->r[1], tri->g[1], tri->b[1], tri->a[1], tri->s0[1], tri->s1[1], tri->t0[1], tri->t1[1]);
					s.Append(s2);

					s2.Printf("Vertex 2:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=[%f,%f] T=[%f,%f]\n",
						 tri->x[2], tri->y[2], tri->z[2], tri->w[2], tri->r[2], tri->g[2], tri->b[2], tri->a[2], tri->s0[2], tri->s1[2], tri->t0[2], tri->t1[2]);
					s.Append(s2);
				} else {
					s.Printf("Vertex 0:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=%f T=%f\n",
						 tri->x[0], tri->y[0], tri->z[0], tri->w[0], tri->r[0], tri->g[0], tri->b[0], tri->a[0], tri->s0[0], tri->t0[0]);

					s2.Printf("Vertex 1:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=%f T=%f\n",
						 tri->x[1], tri->y[1], tri->z[1], tri->w[1], tri->r[1], tri->g[1], tri->b[1], tri->a[1], tri->s0[1], tri->t0[1]);
					s.Append(s2);

					s2.Printf("Vertex 2:\n\tX=%f Y=%f Z=%f W=%f\n\t\tRed=%f Green=%f Blue=%f Alpha=%f S=%f T=%f\n",
						 tri->x[2], tri->y[2], tri->z[2], tri->w[2], tri->r[2], tri->g[2], tri->b[2], tri->a[2], tri->s0[2], tri->t0[2]);
					s.Append(s2);
				}

				if (!(tri->fbzColorPath & (1 << 27))) {
					s2.Printf("Untextured\n");
					s.Append(s2);
				} else if (voodoo->dual_tmus) {
					s2.Printf("TMU0: Texture ID=%u  textureMode=%08x\n", tri->texture[0], tri->textureMode[0]);
					s.Append(s2);
					s2.Printf("TMU1: Texture ID=%u  textureMode=%08x\n", tri->texture[1], tri->textureMode[1]);
					s.Append(s2);
				} else {
					s2.Printf("Texture ID=%u  textureMode=%08x\n", tri->texture[0], tri->textureMode[0]);
					s.Append(s2);
				}

				s2.Printf("Chroma-key: Red=%u Green=%u Blue=%u\n", (tri->chromaKey >> 16) & 0xff, (tri->chromaKey >> 8) & 0xff, tri->chromaKey & 0xff);
				s.Append(s2);

				s2.Printf("Color0: Red=%u Green=%u Blue=%u\n", (tri->color0 >> 16) & 0xff, (tri->color0 >> 8) & 0xff, tri->color0 & 0xff);
				s.Append(s2);

				s2.Printf("Color1: Red=%u Green=%u Blue=%u\n", (tri->color1 >> 16) & 0xff, (tri->color1 >> 8) & 0xff, tri->color1 & 0xff);
				s.Append(s2);

				s2.Printf("fogColor: Red=%u Green=%u Blue=%u\n", tri->fogColor.r, tri->fogColor.g, tri->fogColor.b);
				s.Append(s2);

				s2.Printf("zaColor: Depth=%u Alpha=%u\n", tri->zaColor & 0xffff, (tri->zaColor >> 24) & 0xff);
				s.Append(s2);

				s2.Printf("fbzMode=%08x fbzColorPath=%08x alphaMode=%08x fogMode=%08x", tri->fbzMode, tri->fbzColorPath, tri->alphaMode, tri->fogMode);
				s.Append(s2);

				text_ctrl->SetValue(s);
			} else if (tri->cmd == CMD_START_STRIP) {
				canvas->set_selected_strip(tri->id);
				text_ctrl->SetValue(wxEmptyString);
			} else {
				text_ctrl->SetValue(wxEmptyString);
			}
		} else {
			text_ctrl->SetValue(wxEmptyString);
		}
	}

public:
	ViewerVoodoo(wxWindow *parent, wxString title, wxSize size, void *p)
	: Viewer(parent, title, size, p),
	  voodoo((voodoo_t *)p),
	  is_paused(false),
	  triangle_next_id(0),
	  strip_next_id(0),
	  current_strip_id(-2)
	{
		{
			std::lock_guard<std::mutex> guard(voodoo_viewer_refcount_mutex);

			voodoo->viewer_active++;
		}

		wxSplitterWindow *splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
		splitter->SetSashGravity(0.5);
		splitter->SetMinimumPaneSize(1);

		wxPanel *display_panel = new wxPanel(splitter, wxID_ANY);
		display_sz = new wxBoxSizer(wxVERTICAL);
		display_panel->SetSizer(display_sz);

		canvas = new ViewerVoodooCanvas(display_panel, "canvas", wxDefaultPosition, wxSize(512, 384), (voodoo_t *)p, this, &triangle_list_display);
		display_sz->Add(canvas);
		display_box = new wxComboBox(display_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
		display_box->Append("Frame buffer");
		display_box->Append("Frame buffer + wireframe");
		display_box->Append("Depth buffer");
		display_box->Append("Depth buffer + wireframe");
		display_box->Append("Wireframe");
		display_box->SetValue("Frame buffer + wireframe");
		display_box->Disable();
		display_sz->Add(display_box);

		wxNotebook *notebook = new wxNotebook(static_cast<wxWindow *>(splitter), wxID_ANY);

		wxSplitterWindow *splitter2 = new wxSplitterWindow(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
		splitter2->SetSashGravity(0.75);
		splitter2->SetMinimumPaneSize(1);

		dv_ctrl = new wxDataViewTreeCtrl(splitter2, wxID_ANY, wxDefaultPosition, wxDefaultSize);

		text_ctrl = new wxTextCtrl(splitter2, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

		splitter2->SplitHorizontally(dv_ctrl, text_ctrl);

		notebook->AddPage(splitter2, "Commands");


		wxSplitterWindow *texture_splitter = new wxSplitterWindow(notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
		texture_splitter->SetSashGravity(0.75);
		texture_splitter->SetMinimumPaneSize(1);

		wxSplitterWindow *texture_splitter2 = new wxSplitterWindow(texture_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
		texture_splitter2->SetSashGravity(0.75);
		texture_splitter2->SetMinimumPaneSize(1);

		texture_panel = new wxScrolledWindow(texture_splitter2, wxID_ANY);
		texture_panel->SetScrollRate(15, 15);
		texture_sz = new wxGridSizer(4, 16, 16);
		texture_panel->SetSizer(texture_sz);

		texture_text_ctrl = new wxTextCtrl(texture_splitter2, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

		texture_splitter2->SplitHorizontally(texture_panel, texture_text_ctrl);

		wxBitmap *texture_bitmap = new wxBitmap(256, 256+128+64+32+16+8+4+2+1);
		texture_bitmap_ctrl = new wxStaticBitmap(static_cast<wxWindow *>(texture_splitter), wxID_ANY, *texture_bitmap);

		texture_splitter->SplitVertically(texture_splitter2, texture_bitmap_ctrl);

		notebook->AddPage(texture_splitter, "Textures");

		splitter->SplitVertically(display_panel, notebook);


	        Bind(wxEVT_CLOSE_WINDOW, &ViewerVoodoo::OnClose, this);
		Bind(wxEVT_COMBOBOX, &ViewerVoodoo::OnComboBox, this);
		Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ViewerVoodoo::OnSelectionChanged, this);
	}

	virtual ~ViewerVoodoo()
	{
		{
			std::lock_guard<std::mutex> guard(voodoo_viewer_refcount_mutex);

			voodoo->viewer_active--;
		}

		delete canvas;
	}

	void SwapBuffer()
	{
		canvas->SwapBuffer();
		triangle_list_display = triangle_list_active;
		triangle_list_active.clear();
		triangle_next_id = 0;
		strip_next_id = 0;
		current_strip_id = -1;

		texture_list_display = texture_list_active;
		texture_list_active.clear();
		texture_data_display = texture_data_active;
	}

	void QueueTriangle()
	{
		float dxB = (float)(voodoo->params.vertexBx - voodoo->params.vertexAx) / 16.0f;
		float dyB = (float)(voodoo->params.vertexBy - voodoo->params.vertexAy) / 16.0f;
		float dxC = (float)(voodoo->params.vertexCx - voodoo->params.vertexCx) / 16.0f;
		float dyC = (float)(voodoo->params.vertexCy - voodoo->params.vertexCy) / 16.0f;

		VoodooTriangle tri = {
			.cmd = CMD_TRIANGLE,
			.id = triangle_next_id,
			.strip_id = current_strip_id,
			.x = {
				(float)voodoo->params.vertexAx / 16.0f,
				(float)voodoo->params.vertexBx / 16.0f,
				(float)voodoo->params.vertexCx / 16.0f
			},
			.y = {
				(float)voodoo->params.vertexAy / 16.0f,
				(float)voodoo->params.vertexBy / 16.0f,
				(float)voodoo->params.vertexCy / 16.0f
			},
			.z = {
				(float)voodoo->params.startZ / 4096.0f,
				((float)voodoo->params.startZ + (float)voodoo->params.dZdX * dxB + (float)voodoo->params.dZdY * dyB) / 4096.0f,
				((float)voodoo->params.startZ + (float)voodoo->params.dZdX * dxC + (float)voodoo->params.dZdY * dyC) / 4096.0f,
			},
			.w = {
				(float)voodoo->params.startW / 4294967296.0f,
				((float)voodoo->params.startW + (float)voodoo->params.dWdX * dxB + (float)voodoo->params.dWdY * dyB) / 4294967296.0f,
				((float)voodoo->params.startW + (float)voodoo->params.dWdX * dxC + (float)voodoo->params.dWdY * dyC) / 4294967296.0f,
			},
			.r = {
				(float)voodoo->params.startR / 4096.0f,
				((float)voodoo->params.startR + (float)voodoo->params.dRdX * dxB + (float)voodoo->params.dRdY * dyB) / 4096.0f,
				((float)voodoo->params.startR + (float)voodoo->params.dRdX * dxC + (float)voodoo->params.dRdY * dyC) / 4096.0f,
			},
			.g = {
				(float)voodoo->params.startG / 4096.0f,
				((float)voodoo->params.startG + (float)voodoo->params.dGdX * dxB + (float)voodoo->params.dGdY * dyB) / 4096.0f,
				((float)voodoo->params.startG + (float)voodoo->params.dGdX * dxC + (float)voodoo->params.dGdY * dyC) / 4096.0f,
			},
			.b = {
				(float)voodoo->params.startB / 4096.0f,
				((float)voodoo->params.startB + (float)voodoo->params.dBdX * dxB + (float)voodoo->params.dBdY * dyB) / 4096.0f,
				((float)voodoo->params.startB + (float)voodoo->params.dBdX * dxC + (float)voodoo->params.dBdY * dyC) / 4096.0f,
			},
			.a = {
				(float)voodoo->params.startA / 4096.0f,
				((float)voodoo->params.startA + (float)voodoo->params.dAdX * dxB + (float)voodoo->params.dAdY * dyB) / 4096.0f,
				((float)voodoo->params.startA + (float)voodoo->params.dAdX * dxC + (float)voodoo->params.dAdY * dyC) / 4096.0f,
			},
			.s0 = {
				(float)voodoo->params.tmu[0].startS / 4294967296.0f,
				((float)voodoo->params.tmu[0].startS + (float)voodoo->params.tmu[0].dSdX * dxB + (float)voodoo->params.tmu[0].dSdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[0].startS + (float)voodoo->params.tmu[0].dSdX * dxC + (float)voodoo->params.tmu[0].dSdY * dyC) / 4294967296.0f,
			},
			.t0 = {
				(float)voodoo->params.tmu[0].startT / 4294967296.0f,
				((float)voodoo->params.tmu[0].startT + (float)voodoo->params.tmu[0].dTdX * dxB + (float)voodoo->params.tmu[0].dTdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[0].startT + (float)voodoo->params.tmu[0].dTdX * dxC + (float)voodoo->params.tmu[0].dTdY * dyC) / 4294967296.0f,
			},
			.w0 = {
				(float)voodoo->params.tmu[0].startW / 4294967296.0f,
				((float)voodoo->params.tmu[0].startW + (float)voodoo->params.tmu[0].dWdX * dxB + (float)voodoo->params.tmu[0].dWdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[0].startW + (float)voodoo->params.tmu[0].dWdX * dxC + (float)voodoo->params.tmu[0].dWdY * dyC) / 4294967296.0f,
			},
			.s1 = {
				(float)voodoo->params.tmu[1].startS / 4294967296.0f,
				((float)voodoo->params.tmu[1].startS + (float)voodoo->params.tmu[1].dSdX * dxB + (float)voodoo->params.tmu[1].dSdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[1].startS + (float)voodoo->params.tmu[1].dSdX * dxC + (float)voodoo->params.tmu[1].dSdY * dyC) / 4294967296.0f,
			},
			.t1 = {
				(float)voodoo->params.tmu[1].startT / 4294967296.0f,
				((float)voodoo->params.tmu[1].startT + (float)voodoo->params.tmu[1].dTdX * dxB + (float)voodoo->params.tmu[1].dTdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[1].startT + (float)voodoo->params.tmu[1].dTdX * dxC + (float)voodoo->params.tmu[1].dTdY * dyC) / 4294967296.0f,
			},
			.w1 = {
				(float)voodoo->params.tmu[1].startW / 4294967296.0f,
				((float)voodoo->params.tmu[1].startW + (float)voodoo->params.tmu[1].dWdX * dxB + (float)voodoo->params.tmu[1].dWdY * dyB) / 4294967296.0f,
				((float)voodoo->params.tmu[1].startW + (float)voodoo->params.tmu[1].dWdX * dxC + (float)voodoo->params.tmu[1].dWdY * dyC) / 4294967296.0f,
			},
			.texture = {
				current_texture[0],
				current_texture[1]
			},
			.textureMode = {
				voodoo->params.textureMode[0],
				voodoo->params.textureMode[1]
			},
			.chromaKey = voodoo->params.chromaKey,
			.color0 = voodoo->params.color0,
			.color1 = voodoo->params.color1,
			.fogColor = voodoo->params.fogColor,
			.zaColor = voodoo->params.zaColor,
			.fbzMode = voodoo->params.fbzMode,
			.fbzColorPath = voodoo->params.fbzColorPath,
			.alphaMode = voodoo->params.alphaMode,
			.fogMode = voodoo->params.fogMode
		};

                if (voodoo->params.fbzMode & (1 << 17)) {
			int y_origin = (voodoo->type >= VOODOO_BANSHEE) ? voodoo->y_origin_swap : (voodoo->v_disp - 1);

			for (int i = 0; i < 3; i++)
	                        tri.y[i] = y_origin - tri.y[i];
		}

		if (voodoo->params.textureMode[0] & 1) {
			/*Apply perspective correction*/
			for (int i = 0; i < 3; i++) {
				float oow = 1.0 / tri.w0[i];
				tri.s0[i] *= oow;
				tri.t0[i] *= oow;
			}
		}
		if (voodoo->params.textureMode[1] & 1) {
			/*Apply perspective correction*/
			for (int i = 0; i < 3; i++) {
				float oow = 1.0 / tri.w1[i];
				tri.s1[i] *= oow;
				tri.t1[i] *= oow;
			}
		}

		triangle_next_id++;

		triangle_list_active.push_back(tri);
	}

	void BeginStrip()
	{
		VoodooTriangle tri = {
			.cmd = CMD_START_STRIP,
			.id = strip_next_id
		};

		current_strip_id = strip_next_id;
		strip_next_id++;

		triangle_list_active.push_back(tri);
	}

	void EndStrip()
	{
		VoodooTriangle tri = {
			.cmd = CMD_END_STRIP
		};

		current_strip_id = -1;

		triangle_list_active.push_back(tri);
	}

	void UseTexture(int tmu)
	{
		voodoo_params_t *params = &voodoo->params;
	        uint32_t addr = 0;
        	uint32_t palette_checksum;

	        if (params->tformat[tmu] == TEX_PAL8 || params->tformat[tmu] == TEX_APAL8 || params->tformat[tmu] == TEX_APAL88) {
        	        if (voodoo->palette_dirty[tmu]) {
                	        palette_checksum = 0;

	                        for (int i = 0; i < 256; i++)
        	                        palette_checksum ^= voodoo->palette[tmu][i].u;

	                        voodoo->palette_checksum[tmu] = palette_checksum;
        	                voodoo->palette_dirty[tmu] = 0;
                	} else
                        	palette_checksum = voodoo->palette_checksum[tmu];
	        } else
        	        palette_checksum = 0;

        	if ((voodoo->params.tLOD[tmu] & LOD_SPLIT) && (voodoo->params.tLOD[tmu] & LOD_ODD) &&
	            (voodoo->params.tLOD[tmu] & LOD_TMULTIBASEADDR))
        	        addr = params->texBaseAddr1[tmu];
	        else
        	        addr = params->texBaseAddr[tmu];

		int id = 0;

		for (std::vector<viewer_texture_t>::iterator it = texture_list_active.begin(); it != texture_list_active.end(); it++) {
			viewer_texture_t *tex = &*it;

	                if (tex->t.base == addr &&
                    	    tex->t.tLOD == (params->tLOD[tmu] & 0xf00fff) &&
                    	    tex->t.palette_checksum == palette_checksum) {
				/*Found in cache*/
				current_texture[tmu] = id;
				return;
			}
			id++;
		}

		/*Add to cache*/
		int cache_entry = params->tex_entry[tmu];
		id = texture_list_active.size();
		viewer_texture_t tex;

		tex.t = voodoo->texture_cache[tmu][cache_entry];
		uint32_t base = params->tex_base[tmu][0];

		for (int lod = 0; lod < 10; lod++) {
			tex.w[lod] = params->tex_w_mask[tmu][lod] + 1;
			tex.h[lod] = params->tex_h_mask[tmu][lod] + 1;
			tex.offset[lod] = params->tex_base[tmu][lod] - base;
		}
		tex.textureMode = params->textureMode[tmu];
		tex.tmu = tmu;

		texture_list_active.push_back(tex);

		while (id >= texture_data_active.size())
			texture_data_active.push_back((uint32_t *)malloc(TEXTURE_DATA_SIZE));

		memcpy(texture_data_active[id], voodoo->texture_cache[tmu][cache_entry].data, TEXTURE_DATA_SIZE);
		current_texture[tmu] = id;
	}

	void SelectTexture(int id)
	{
		viewer_texture_t *tex = &texture_list_display[id];
		wxString s, s2;

		s.Printf("Texture #%u", id);

		int lod_min = (tex->t.tLOD >> 2) & 15;
		int lod_max = (tex->t.tLOD >> 8) & 15;
	        lod_min = MIN(lod_min, 8);
	        lod_max = MIN(lod_max, 8);

		s2.Printf("\n\tLOD: min=%u (%ux%u) max=%u (%ux%u)",
			  lod_min, tex->w[lod_min], tex->h[lod_min],
			  lod_max, tex->w[lod_max], tex->h[lod_max]);
		s.Append(s2);

		s2.Printf("\n\tFormat: %s", texture_format_names[(tex->textureMode >> 8) & 0xf]);
		s.Append(s2);

		s2.Printf("\n\tTMU #%u", tex->tmu);
		s.Append(s2);

		texture_text_ctrl->SetValue(s);

		wxImage image(256, 256+128+64+32+16+8+4+2+1);
		image.InitAlpha();

		unsigned char *buffer_data = image.GetData();
		unsigned char *alpha_data = image.GetAlpha();
		int yy = 0;

		memset(alpha_data, 0, image.GetWidth() * image.GetHeight());

		for (int lod = lod_min; lod <= lod_max; lod++) {
			int addr = texture_offset[lod];

			for (int y = 0; y < tex->h[lod]; y++) {
				unsigned char *data = buffer_data + yy*3*image.GetWidth();
				unsigned char *alpha = alpha_data + yy*image.GetWidth();
				int x;

				for (x = 0; x < tex->w[lod]; x++) {
					uint32_t val = texture_data_display[id][addr + x];

					*data++ = (val >> 16) & 0xff;
					*data++ = (val >> 8) & 0xff;
					*data++ = val & 0xff;
					*alpha++ = 0xff;
				}
				for (; x < 256; x++) {
					*data++ = 0;
					*data++ = 0;
					*data++ = 0;
					*alpha++ = 0;
				}

				addr += (1 << (8 - lod));
				yy++;
			}
		}

		wxBitmap bitmap(image);
		texture_bitmap_ctrl->SetBitmap(bitmap);

		canvas->set_selected_texture(id);
	}

	void NotifyPause()
	{
		if (!is_paused)
		{
			is_paused = true;
			canvas->set_paused(true);

			UpdateDVCtrl();
			UpdateTextures();
			display_box->Enable();
			display_sz->Layout();

			Layout();
			Refresh();
			Update();
		}
	}

	void NotifyResume()
	{
		if (is_paused)
		{
			is_paused = false;
			canvas->set_paused(false);

			dv_ctrl->DeleteAllItems();
			text_ctrl->SetValue(wxEmptyString);
			texture_text_ctrl->SetValue(wxEmptyString);

			wxImage image(256, 256+128+64+32+16+8+4+2+1);
			wxBitmap bitmap(image);
			texture_bitmap_ctrl->SetBitmap(bitmap);

			ClearTextures();

			display_box->Disable();

			Refresh();
		}
	}
};

void ViewerVoodooTextureBitmap::OnLeftDown(wxMouseEvent &event)
{
	viewer->SelectTexture(texture_id);
}

void voodoo_viewer_swap_buffer(void *v, void *param)
{
	ViewerVoodoo *viewer = static_cast<ViewerVoodoo *>(v);

	viewer->SwapBuffer();
}

void voodoo_viewer_queue_triangle(void *v, void *param)
{
	ViewerVoodoo *viewer = static_cast<ViewerVoodoo *>(v);

	viewer->QueueTriangle();
}

void voodoo_viewer_begin_strip(void *v, void *param)
{
	ViewerVoodoo *viewer = static_cast<ViewerVoodoo *>(v);

	viewer->BeginStrip();
}

void voodoo_viewer_end_strip(void *v, void *param)
{
	ViewerVoodoo *viewer = static_cast<ViewerVoodoo *>(v);

	viewer->EndStrip();
}

void voodoo_viewer_use_texture(void *v, void *param)
{
	ViewerVoodoo *viewer = static_cast<ViewerVoodoo *>(v);

	viewer->UseTexture((int)(uintptr_t)param);
}

static void *viewer_voodoo_open(void *parent, void *p, const char *title)
{
	wxFrame *w = new ViewerVoodoo((wxWindow *)parent, title, wxSize(800, 600), p);

	w->Show(true);

	return w;
}

viewer_t viewer_voodoo =
{
	.open = viewer_voodoo_open
};
