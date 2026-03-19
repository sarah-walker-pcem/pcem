#include <QCloseEvent>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
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
	CMD_START_STRIP,
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

class ViewerVoodooCanvas: public QWidget
{
	Q_OBJECT
private:
	class ViewerVoodoo *voodoo_parent;
	voodoo_t *voodoo;
	QImage buffer;
	QImage depth_buffer;
	std::mutex buffer_mutex;
	int fb_width, fb_height;
	bool is_paused;
	std::list<VoodooTriangle> *triangle_list_display;
	int selected_tri_id;
	int selected_strip_id;
	int selected_texture_id;
	VoodooDisplayMode display_mode;

protected:
	void paintEvent(QPaintEvent *event) override
	{
		std::lock_guard<std::mutex> guard(buffer_mutex);

		if (!fb_width || !fb_height)
			return;

		QPainter dc(this);
		int w = width();
		int h = height();

		dc.fillRect(0, 0, w, h, Qt::white);

		if (!is_paused) {
			QFontMetrics fm(dc.font());
			QString text = "Pause to update";
			int text_w = fm.horizontalAdvance(text);
			int text_h = fm.height();
			dc.drawText((w / 2) - (text_w / 2), (h / 2) - (text_h / 2) + fm.ascent(), text);
		} else {
			QImage composite(fb_width, fb_height, QImage::Format_RGB888);
			QPainter mdc(&composite);

			if (display_mode == DM_FRAMEBUFFER || display_mode == DM_FRAMEBUFFER_WIREFRAME) {
				mdc.drawImage(0, 0, buffer, 0, 0, fb_width, fb_height);
			} else if (display_mode == DM_DEPTHBUFFER || display_mode == DM_DEPTHBUFFER_WIREFRAME) {
				mdc.drawImage(0, 0, depth_buffer, 0, 0, fb_width, fb_height);
			} else {
				mdc.fillRect(0, 0, fb_width, fb_height, Qt::black);
			}

			mdc.setPen(Qt::white);
			mdc.setBrush(Qt::red);

			for (std::list<VoodooTriangle>::iterator it = triangle_list_display->begin(); it != triangle_list_display->end(); it++) {
				VoodooTriangle *tri = &*it;

				if (tri->id == selected_tri_id || tri->strip_id == selected_strip_id ||
				    ((tri->fbzColorPath & (1 << 27)) && (tri->texture[0] == selected_texture_id || (voodoo->dual_tmus && tri->texture[1] == selected_texture_id)))) {
					QPointF points[3] = {
						QPointF(tri->x[0], tri->y[0]),
						QPointF(tri->x[1], tri->y[1]),
						QPointF(tri->x[2], tri->y[2]),
					};
					mdc.drawPolygon(points, 3);
				} else if (display_mode == DM_WIREFRAME || display_mode == DM_DEPTHBUFFER_WIREFRAME || display_mode == DM_FRAMEBUFFER_WIREFRAME) {
					mdc.drawLine(QPointF(tri->x[0], tri->y[0]), QPointF(tri->x[1], tri->y[1]));
					mdc.drawLine(QPointF(tri->x[1], tri->y[1]), QPointF(tri->x[2], tri->y[2]));
					mdc.drawLine(QPointF(tri->x[2], tri->y[2]), QPointF(tri->x[0], tri->y[0]));
				}
			}

			mdc.end();
			dc.drawImage(0, 0, composite);
		}
	}

public:
	ViewerVoodooCanvas(QWidget *parent, voodoo_t *voodoo, class ViewerVoodoo *voodoo_parent, std::list<VoodooTriangle> *triangle_list_display)
	: QWidget(parent),
	  voodoo(voodoo),
	  voodoo_parent(voodoo_parent),
	  buffer(4096, 4096, QImage::Format_RGB888),
	  depth_buffer(4096, 4096, QImage::Format_RGB888),
	  fb_width(0), fb_height(0),
	  is_paused(false),
	  triangle_list_display(triangle_list_display),
	  selected_tri_id(-1),
	  selected_strip_id(-2),
	  selected_texture_id(-1),
	  display_mode(DM_FRAMEBUFFER_WIREFRAME)
	{
		setMinimumSize(512, 384);
	}

	void SwapBuffer()
	{
		{
			std::lock_guard<std::mutex> guard(buffer_mutex);

			fb_width = voodoo->h_disp;
			fb_height = voodoo->v_disp;

			unsigned char *buffer_data = buffer.bits();
			int buffer_bpl = buffer.bytesPerLine();
			for (int y = 0; y < voodoo->v_disp; y++) {
				unsigned char *data = buffer_data + y * buffer_bpl;

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

			buffer_data = depth_buffer.bits();
			buffer_bpl = depth_buffer.bytesPerLine();
			for (int y = 0; y < voodoo->v_disp; y++) {
				unsigned char *data = buffer_data + y * buffer_bpl;

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

		update();
	}

	void set_paused(bool new_is_paused)
	{
		is_paused = new_is_paused;
		if (!is_paused) {
			selected_tri_id = -1;
			selected_strip_id = -2;
			selected_texture_id = -1;
		}
		setMinimumSize(fb_width, fb_height);
		update();
	}

	void set_selected_triangle(int id)
	{
		selected_tri_id = id;
		selected_strip_id = -2;
		selected_texture_id = -1;
		update();
	}

	void set_selected_strip(int id)
	{
		selected_tri_id = -1;
		selected_strip_id = id;
		selected_texture_id = -1;
		update();
	}

	void set_selected_texture(int id)
	{
		selected_tri_id = -1;
		selected_strip_id = -2;
		selected_texture_id = id;
		update();
	}

	void set_display_mode(VoodooDisplayMode dm)
	{
		display_mode = dm;
		update();
	}
};

#define TEXTURE_DATA_SIZE ((256 * 256 + 256 * 256 + 128 * 128 + 64 * 64 + 32 * 32 + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2) * 4)

class ViewerVoodoo;

class ViewerVoodooTextureBitmap: public QLabel
{
	Q_OBJECT
private:
	ViewerVoodoo *viewer;
	int texture_id;

	void mousePressEvent(QMouseEvent *event) override;

public:
	ViewerVoodooTextureBitmap(QWidget *parent, const QPixmap &pixmap, const QSize& size, ViewerVoodoo *viewer, int id)
	: QLabel(parent),
	  viewer(viewer),
	  texture_id(id)
	{
		setPixmap(pixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		setFixedSize(size);
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
	Q_OBJECT
private:
	voodoo_t *voodoo;
	ViewerVoodooCanvas *canvas;
	QComboBox *display_box;
	QVBoxLayout *display_sz;

	std::vector<QVBoxLayout *> textures;
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

	QTextEdit *text_ctrl;
	QTreeWidget *dv_ctrl;
	QTextEdit *texture_text_ctrl;
	QLabel *texture_bitmap_ctrl;
	QGridLayout *texture_sz;
	QScrollArea *texture_scroll;
	QWidget *texture_panel;

	void closeEvent(QCloseEvent *event) override
	{
		viewer_remove(this);
		event->accept();
	}

	void OnComboBox(int selection)
	{
		switch (selection) {
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
		dv_ctrl->clear();

		QTreeWidgetItem *root = nullptr;

		for (std::list<VoodooTriangle>::iterator it = triangle_list_display.begin(); it != triangle_list_display.end(); it++) {
			VoodooTriangle *tri = &*it;
			QString s;

			if (tri->cmd == CMD_TRIANGLE) {
				s = QString("Triangle %1").arg(tri->id);
				QTreeWidgetItem *item;
				if (root)
					item = new QTreeWidgetItem(root);
				else
					item = new QTreeWidgetItem(dv_ctrl);
				item->setText(0, s);
				item->setData(0, Qt::UserRole, QVariant::fromValue((quintptr)tri));
			} else if (tri->cmd == CMD_START_STRIP) {
				s = QString("Strip %1").arg(tri->id);
				root = new QTreeWidgetItem(dv_ctrl);
				root->setText(0, s);
				root->setData(0, Qt::UserRole, QVariant::fromValue((quintptr)tri));
			} else if (tri->cmd == CMD_END_STRIP) {
				root = nullptr;
			}
		}
	}

	void UpdateTextures()
	{
		int id = 0;

		for (std::vector<viewer_texture_t>::iterator it = texture_list_display.begin(); it != texture_list_display.end(); it++) {
			viewer_texture_t *tex = &*it;
			QString label = QString("Texture %1").arg(id);

			int lod_min = (tex->t.tLOD >> 2) & 15;
		        lod_min = MIN(lod_min, 8);

			QImage image(tex->w[lod_min], tex->h[lod_min], QImage::Format_RGB888);
			unsigned char *buffer_data = image.bits();
			int bpl = image.bytesPerLine();

			int addr = texture_offset[lod_min];

			for (int y = 0; y < tex->h[lod_min]; y++) {
				unsigned char *data = buffer_data + y * bpl;

				for (int x = 0; x < tex->w[lod_min]; x++) {
					uint32_t val = texture_data_display[id][addr + x];

					*data++ = (val >> 16) & 0xff;
					*data++ = (val >> 8) & 0xff;
					*data++ = val & 0xff;
				}

				addr += (1 << (8 - lod_min));
			}

			QImage scaled;
			if (tex->w[lod_min] > tex->h[lod_min]) {
				int h = (tex->h[lod_min] * 128) / tex->w[lod_min];
				scaled = image.scaled(128, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			} else {
				int w = (tex->w[lod_min] * 128) / tex->h[lod_min];
				scaled = image.scaled(w, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			}

			QVBoxLayout *vvt = new QVBoxLayout();
			ViewerVoodooTextureBitmap *bmp = new ViewerVoodooTextureBitmap(texture_panel, QPixmap::fromImage(scaled), QSize(128, 128), this, id);
			QLabel *textLabel = new QLabel(label, texture_panel);
			vvt->addWidget(bmp);
			vvt->addWidget(textLabel);

			int row = id / 4;
			int col = id % 4;
			texture_sz->addLayout(vvt, row, col);
			textures.push_back(vvt);

			id++;
		}

		texture_panel->adjustSize();
	}

	void ClearTextures()
	{
		/* Remove all items from the grid */
		while (texture_sz->count() > 0) {
			QLayoutItem *item = texture_sz->takeAt(0);
			if (item->layout()) {
				while (item->layout()->count() > 0) {
					QLayoutItem *child = item->layout()->takeAt(0);
					delete child->widget();
					delete child;
				}
				delete item->layout();
			} else if (item->widget()) {
				delete item->widget();
			}
			delete item;
		}

		textures.clear();
	}

	void OnSelectionChanged()
	{
		QList<QTreeWidgetItem *> items = dv_ctrl->selectedItems();
		canvas->set_selected_texture(-1);

		if (!items.isEmpty()) {
			QTreeWidgetItem *item = items.first();
			quintptr ptr = item->data(0, Qt::UserRole).value<quintptr>();
			VoodooTriangle *tri = (VoodooTriangle *)ptr;

			if (tri) {
				if (tri->cmd == CMD_TRIANGLE) {
					canvas->set_selected_triangle(tri->id);

					QString s, s2;

					if (voodoo->dual_tmus) {
						s = QString("Vertex 0:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=[%9,%10] T=[%11,%12]\n")
							.arg(tri->x[0]).arg(tri->y[0]).arg(tri->z[0]).arg(tri->w[0])
							.arg(tri->r[0]).arg(tri->g[0]).arg(tri->b[0]).arg(tri->a[0])
							.arg(tri->s0[0]).arg(tri->s1[0]).arg(tri->t0[0]).arg(tri->t1[0]);

						s2 = QString("Vertex 1:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=[%9,%10] T=[%11,%12]\n")
							.arg(tri->x[1]).arg(tri->y[1]).arg(tri->z[1]).arg(tri->w[1])
							.arg(tri->r[1]).arg(tri->g[1]).arg(tri->b[1]).arg(tri->a[1])
							.arg(tri->s0[1]).arg(tri->s1[1]).arg(tri->t0[1]).arg(tri->t1[1]);
						s.append(s2);

						s2 = QString("Vertex 2:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=[%9,%10] T=[%11,%12]\n")
							.arg(tri->x[2]).arg(tri->y[2]).arg(tri->z[2]).arg(tri->w[2])
							.arg(tri->r[2]).arg(tri->g[2]).arg(tri->b[2]).arg(tri->a[2])
							.arg(tri->s0[2]).arg(tri->s1[2]).arg(tri->t0[2]).arg(tri->t1[2]);
						s.append(s2);
					} else {
						s = QString("Vertex 0:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=%9 T=%10\n")
							.arg(tri->x[0]).arg(tri->y[0]).arg(tri->z[0]).arg(tri->w[0])
							.arg(tri->r[0]).arg(tri->g[0]).arg(tri->b[0]).arg(tri->a[0])
							.arg(tri->s0[0]).arg(tri->t0[0]);

						s2 = QString("Vertex 1:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=%9 T=%10\n")
							.arg(tri->x[1]).arg(tri->y[1]).arg(tri->z[1]).arg(tri->w[1])
							.arg(tri->r[1]).arg(tri->g[1]).arg(tri->b[1]).arg(tri->a[1])
							.arg(tri->s0[1]).arg(tri->t0[1]);
						s.append(s2);

						s2 = QString("Vertex 2:\n\tX=%1 Y=%2 Z=%3 W=%4\n\t\tRed=%5 Green=%6 Blue=%7 Alpha=%8 S=%9 T=%10\n")
							.arg(tri->x[2]).arg(tri->y[2]).arg(tri->z[2]).arg(tri->w[2])
							.arg(tri->r[2]).arg(tri->g[2]).arg(tri->b[2]).arg(tri->a[2])
							.arg(tri->s0[2]).arg(tri->t0[2]);
						s.append(s2);
					}

					if (!(tri->fbzColorPath & (1 << 27))) {
						s.append("Untextured\n");
					} else if (voodoo->dual_tmus) {
						s2 = QString("TMU0: Texture ID=%1  textureMode=%2\n")
							.arg(tri->texture[0])
							.arg(tri->textureMode[0], 8, 16, QChar('0'));
						s.append(s2);
						s2 = QString("TMU1: Texture ID=%1  textureMode=%2\n")
							.arg(tri->texture[1])
							.arg(tri->textureMode[1], 8, 16, QChar('0'));
						s.append(s2);
					} else {
						s2 = QString("Texture ID=%1  textureMode=%2\n")
							.arg(tri->texture[0])
							.arg(tri->textureMode[0], 8, 16, QChar('0'));
						s.append(s2);
					}

					s2 = QString("Chroma-key: Red=%1 Green=%2 Blue=%3\n")
						.arg((tri->chromaKey >> 16) & 0xff)
						.arg((tri->chromaKey >> 8) & 0xff)
						.arg(tri->chromaKey & 0xff);
					s.append(s2);

					s2 = QString("Color0: Red=%1 Green=%2 Blue=%3\n")
						.arg((tri->color0 >> 16) & 0xff)
						.arg((tri->color0 >> 8) & 0xff)
						.arg(tri->color0 & 0xff);
					s.append(s2);

					s2 = QString("Color1: Red=%1 Green=%2 Blue=%3\n")
						.arg((tri->color1 >> 16) & 0xff)
						.arg((tri->color1 >> 8) & 0xff)
						.arg(tri->color1 & 0xff);
					s.append(s2);

					s2 = QString("fogColor: Red=%1 Green=%2 Blue=%3\n")
						.arg(tri->fogColor.r)
						.arg(tri->fogColor.g)
						.arg(tri->fogColor.b);
					s.append(s2);

					s2 = QString("zaColor: Depth=%1 Alpha=%2\n")
						.arg(tri->zaColor & 0xffff)
						.arg((tri->zaColor >> 24) & 0xff);
					s.append(s2);

					s2 = QString("fbzMode=%1 fbzColorPath=%2 alphaMode=%3 fogMode=%4")
						.arg(tri->fbzMode, 8, 16, QChar('0'))
						.arg(tri->fbzColorPath, 8, 16, QChar('0'))
						.arg(tri->alphaMode, 8, 16, QChar('0'))
						.arg(tri->fogMode, 8, 16, QChar('0'));
					s.append(s2);

					text_ctrl->setText(s);
				} else if (tri->cmd == CMD_START_STRIP) {
					canvas->set_selected_strip(tri->id);
					text_ctrl->clear();
				} else {
					text_ctrl->clear();
				}
			} else {
				text_ctrl->clear();
			}
		}
	}

public:
	ViewerVoodoo(QWidget *parent, QString title, QSize size, void *p)
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

		QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
		setCentralWidget(splitter);

		QWidget *display_panel = new QWidget(splitter);
		display_sz = new QVBoxLayout(display_panel);
		display_panel->setLayout(display_sz);

		canvas = new ViewerVoodooCanvas(display_panel, (voodoo_t *)p, this, &triangle_list_display);
		display_sz->addWidget(canvas);
		display_box = new QComboBox(display_panel);
		display_box->addItem("Frame buffer");
		display_box->addItem("Frame buffer + wireframe");
		display_box->addItem("Depth buffer");
		display_box->addItem("Depth buffer + wireframe");
		display_box->addItem("Wireframe");
		display_box->setCurrentIndex(1);
		display_box->setEnabled(false);
		display_sz->addWidget(display_box);

		QTabWidget *notebook = new QTabWidget(splitter);

		QSplitter *splitter2 = new QSplitter(Qt::Vertical, notebook);

		dv_ctrl = new QTreeWidget(splitter2);
		dv_ctrl->setHeaderLabel("Commands");

		text_ctrl = new QTextEdit(splitter2);
		text_ctrl->setReadOnly(true);
		text_ctrl->setLineWrapMode(QTextEdit::NoWrap);

		splitter2->addWidget(dv_ctrl);
		splitter2->addWidget(text_ctrl);
		splitter2->setStretchFactor(0, 3);
		splitter2->setStretchFactor(1, 1);

		notebook->addTab(splitter2, "Commands");

		QSplitter *texture_splitter = new QSplitter(Qt::Horizontal, notebook);

		QSplitter *texture_splitter2 = new QSplitter(Qt::Vertical, texture_splitter);

		texture_scroll = new QScrollArea(texture_splitter2);
		texture_scroll->setWidgetResizable(true);
		texture_panel = new QWidget();
		texture_sz = new QGridLayout(texture_panel);
		texture_sz->setSpacing(16);
		texture_panel->setLayout(texture_sz);
		texture_scroll->setWidget(texture_panel);

		texture_text_ctrl = new QTextEdit(texture_splitter2);
		texture_text_ctrl->setReadOnly(true);
		texture_text_ctrl->setLineWrapMode(QTextEdit::NoWrap);

		texture_splitter2->addWidget(texture_scroll);
		texture_splitter2->addWidget(texture_text_ctrl);
		texture_splitter2->setStretchFactor(0, 3);
		texture_splitter2->setStretchFactor(1, 1);

		QImage texture_image(256, 256+128+64+32+16+8+4+2+1, QImage::Format_RGB888);
		texture_image.fill(Qt::black);
		texture_bitmap_ctrl = new QLabel(texture_splitter);
		texture_bitmap_ctrl->setPixmap(QPixmap::fromImage(texture_image));

		texture_splitter->addWidget(texture_splitter2);
		texture_splitter->addWidget(texture_bitmap_ctrl);

		notebook->addTab(texture_splitter, "Textures");

		splitter->addWidget(display_panel);
		splitter->addWidget(notebook);

		connect(display_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewerVoodoo::OnComboBox);
		connect(dv_ctrl, &QTreeWidget::itemSelectionChanged, this, &ViewerVoodoo::OnSelectionChanged);
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
			for (int i = 0; i < 3; i++) {
				float oow = 1.0 / tri.w0[i];
				tri.s0[i] *= oow;
				tri.t0[i] *= oow;
			}
		}
		if (voodoo->params.textureMode[1] & 1) {
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
				current_texture[tmu] = id;
				return;
			}
			id++;
		}

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

		while ((int)id >= (int)texture_data_active.size())
			texture_data_active.push_back((uint32_t *)malloc(TEXTURE_DATA_SIZE));

		memcpy(texture_data_active[id], voodoo->texture_cache[tmu][cache_entry].data, TEXTURE_DATA_SIZE);
		current_texture[tmu] = id;
	}

	void SelectTexture(int id)
	{
		viewer_texture_t *tex = &texture_list_display[id];
		QString s;

		s = QString("Texture #%1").arg(id);

		int lod_min = (tex->t.tLOD >> 2) & 15;
		int lod_max = (tex->t.tLOD >> 8) & 15;
	        lod_min = MIN(lod_min, 8);
	        lod_max = MIN(lod_max, 8);

		s.append(QString("\n\tLOD: min=%1 (%2x%3) max=%4 (%5x%6)")
			.arg(lod_min).arg(tex->w[lod_min]).arg(tex->h[lod_min])
			.arg(lod_max).arg(tex->w[lod_max]).arg(tex->h[lod_max]));

		s.append(QString("\n\tFormat: %1").arg(texture_format_names[(tex->textureMode >> 8) & 0xf]));

		s.append(QString("\n\tTMU #%1").arg(tex->tmu));

		texture_text_ctrl->setText(s);

		int img_height = 256+128+64+32+16+8+4+2+1;
		QImage image(256, img_height, QImage::Format_RGBA8888);
		image.fill(Qt::transparent);

		int yy = 0;

		for (int lod = lod_min; lod <= lod_max; lod++) {
			int addr = texture_offset[lod];

			for (int y = 0; y < tex->h[lod]; y++) {
				for (int x = 0; x < tex->w[lod]; x++) {
					uint32_t val = texture_data_display[id][addr + x];

					image.setPixelColor(x, yy, QColor(
						(val >> 16) & 0xff,
						(val >> 8) & 0xff,
						val & 0xff,
						0xff
					));
				}

				addr += (1 << (8 - lod));
				yy++;
			}
		}

		texture_bitmap_ctrl->setPixmap(QPixmap::fromImage(image));

		canvas->set_selected_texture(id);
	}

	void notifyPause() override
	{
		if (!is_paused)
		{
			is_paused = true;
			canvas->set_paused(true);

			UpdateDVCtrl();
			UpdateTextures();
			display_box->setEnabled(true);
			display_sz->update();

			update();
		}
	}

	void notifyResume() override
	{
		if (is_paused)
		{
			is_paused = false;
			canvas->set_paused(false);

			dv_ctrl->clear();
			text_ctrl->clear();
			texture_text_ctrl->clear();

			QImage image(256, 256+128+64+32+16+8+4+2+1, QImage::Format_RGB888);
			image.fill(Qt::black);
			texture_bitmap_ctrl->setPixmap(QPixmap::fromImage(image));

			ClearTextures();

			display_box->setEnabled(false);

			update();
		}
	}
};

void ViewerVoodooTextureBitmap::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
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
	ViewerVoodoo *w = new ViewerVoodoo((QWidget *)parent, title, QSize(800, 600), p);

	w->show();

	return w;
}

viewer_t viewer_voodoo =
{
	.open = viewer_voodoo_open
};

#include "viewer_voodoo.moc"
