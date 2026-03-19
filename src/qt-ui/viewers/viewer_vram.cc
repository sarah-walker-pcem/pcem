#include <QCloseEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
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

class ViewerVRAMCanvas: public QWidget
{
	Q_OBJECT
private:
	class ViewerVRAM *vram_parent;
	svga_t *svga;
	QImage buffer;
	uint32_t custom_start_addr;
	uint32_t custom_pitch;
	ColourDepth custom_colour_depth;
	AddrMode custom_addr_mode;
	bool use_custom_start_addr;
	bool use_custom_pitch;
	bool use_custom_colour_depth;
	bool use_custom_addr_mode;

	int scroll_x_pos, scroll_y_pos;

	uint32_t current_start_addr;
	uint32_t current_pitch;
	int current_bpp;

	int scale_factor;

	QScrollBar *h_scrollbar;
	QScrollBar *v_scrollbar;

	void UpdateLabels();

protected:
	void paintEvent(QPaintEvent *event) override
	{
		QPainter dc(this);
		int disp_w = width();
		int disp_h = height();
		int w = disp_w / scale_factor;
		int h = disp_h / scale_factor;

		dc.fillRect(0, 0, disp_w, disp_h, Qt::white);

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

		if (h_scrollbar) {
			h_scrollbar->setRange(0, std::max(0, line_width - w));
			h_scrollbar->setPageStep(w);
		}
		if (v_scrollbar) {
			v_scrollbar->setRange(0, std::max(0, vram_height - h));
			v_scrollbar->setPageStep(h);
		}

		int x_offset = h_scrollbar ? h_scrollbar->value() : 0;
		int y_offset = v_scrollbar ? v_scrollbar->value() : 0;
		unsigned char *buffer_data = buffer.bits();
		int buffer_bpl = buffer.bytesPerLine();

		for (int y = 0; y < h; y++)
		{
			unsigned char *data = buffer_data + y * buffer_bpl;
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

		QPixmap pixmap = QPixmap::fromImage(buffer);

		if (scale_factor == 1)
			dc.drawPixmap(0, 0, plot_width, h, pixmap, 0, 0, plot_width, h);
		else
			dc.drawPixmap(0, 0, plot_width * scale_factor, h * scale_factor, pixmap, 0, 0, plot_width, h);

		if (current_start_addr != svga->ma_latch * 4 || current_pitch != svga->rowoffset || current_bpp != svga->video_bpp)
			UpdateLabels();
	}

public:
	ViewerVRAMCanvas(QWidget *parent, svga_t *svga, class ViewerVRAM *vram_parent)
	: QWidget(parent),
	  svga(svga),
	  buffer(4096, 4096, QImage::Format_RGB888),
	  custom_pitch(10),
	  use_custom_start_addr(true),
	  use_custom_pitch(false),
	  use_custom_colour_depth(false),
	  use_custom_addr_mode(false),
	  custom_start_addr(0),
	  custom_colour_depth(CD_8BPP),
	  custom_addr_mode(AM_NORMAL),
	  vram_parent(vram_parent),
	  scale_factor(1),
	  h_scrollbar(nullptr),
	  v_scrollbar(nullptr),
	  current_start_addr(0),
	  current_pitch(0),
	  current_bpp(0)
	{
		buffer.fill(Qt::black);
	}

	virtual ~ViewerVRAMCanvas()
	{
	}

	void setScrollBars(QScrollBar *h, QScrollBar *v)
	{
		h_scrollbar = h;
		v_scrollbar = v;
		if (h_scrollbar)
			connect(h_scrollbar, &QScrollBar::valueChanged, this, [this](int) { update(); });
		if (v_scrollbar)
			connect(v_scrollbar, &QScrollBar::valueChanged, this, [this](int) { update(); });
	}

	void set_custom_start_addr(uint32_t addr)
	{
		custom_start_addr = addr;
		update();
	}

	void set_use_custom_start_addr(bool use)
	{
		use_custom_start_addr = use;
		update();
	}

	void set_custom_pitch(uint32_t pitch)
	{
		custom_pitch = pitch;
		update();
	}

	void set_use_custom_pitch(bool use)
	{
		use_custom_pitch = use;
		update();
	}

	void set_custom_colour_depth(ColourDepth depth)
	{
		custom_colour_depth = depth;
		update();
	}

	void set_use_custom_colour_depth(bool use)
	{
		use_custom_colour_depth = use;
		update();
	}

	void set_custom_addr_mode(AddrMode mode)
	{
		custom_addr_mode = mode;
		update();
	}

	void set_use_custom_addr_mode(bool use)
	{
		use_custom_addr_mode = use;
		update();
	}

	void set_scale_factor(int new_scale_factor)
	{
		scale_factor = new_scale_factor;
		update();
	}
};

class ViewerVRAM: public Viewer
{
	Q_OBJECT
private:
	ViewerVRAMCanvas *canvas;
	QHBoxLayout *sz;
	QWidget *panel;

	QRadioButton *rb_startaddr_current;
	QRadioButton *rb_startaddr_custom;
	QRadioButton *rb_pitch_current;
	QRadioButton *rb_pitch_custom;
	QRadioButton *rb_depth_current;

	void closeEvent(QCloseEvent *event) override
	{
		viewer_remove(this);
		event->accept();
	}

public:
	ViewerVRAM(QWidget *parent, QString title, QSize size, void *p)
	: Viewer(parent, title, size, p)
	{
		QWidget *central = new QWidget(this);
		setCentralWidget(central);
		sz = new QHBoxLayout(central);

		/* Build control panel */
		panel = new QWidget(central);
		QVBoxLayout *panelLayout = new QVBoxLayout(panel);
		panel->setFixedWidth(200);

		/* Start Address group */
		QGroupBox *startAddrGroup = new QGroupBox("Start Address", panel);
		QVBoxLayout *startAddrLayout = new QVBoxLayout(startAddrGroup);
		rb_startaddr_current = new QRadioButton("Current", startAddrGroup);
		rb_startaddr_custom = new QRadioButton("Custom:", startAddrGroup);
		rb_startaddr_custom->setChecked(true);
		QLineEdit *startAddrEdit = new QLineEdit("0", startAddrGroup);
		startAddrEdit->setObjectName("IDC_STARTADDR_TEXTCTRL");
		startAddrLayout->addWidget(rb_startaddr_current);
		startAddrLayout->addWidget(rb_startaddr_custom);
		startAddrLayout->addWidget(startAddrEdit);
		panelLayout->addWidget(startAddrGroup);

		/* Pitch group */
		QGroupBox *pitchGroup = new QGroupBox("Pitch", panel);
		QVBoxLayout *pitchLayout = new QVBoxLayout(pitchGroup);
		rb_pitch_current = new QRadioButton("Current", pitchGroup);
		rb_pitch_current->setChecked(true);
		rb_pitch_custom = new QRadioButton("Custom:", pitchGroup);
		QLineEdit *pitchEdit = new QLineEdit("10", pitchGroup);
		pitchEdit->setObjectName("IDC_PITCH_TEXTCTRL");
		pitchLayout->addWidget(rb_pitch_current);
		pitchLayout->addWidget(rb_pitch_custom);
		pitchLayout->addWidget(pitchEdit);
		panelLayout->addWidget(pitchGroup);

		/* Colour Depth group */
		QGroupBox *depthGroup = new QGroupBox("Colour Depth", panel);
		QVBoxLayout *depthLayout = new QVBoxLayout(depthGroup);
		rb_depth_current = new QRadioButton("Current", depthGroup);
		rb_depth_current->setChecked(true);
		QRadioButton *rb_1bpp = new QRadioButton("1 bpp", depthGroup);
		QRadioButton *rb_2bpp = new QRadioButton("2 bpp", depthGroup);
		QRadioButton *rb_4bpp = new QRadioButton("4 bpp", depthGroup);
		QRadioButton *rb_8bpp = new QRadioButton("8 bpp", depthGroup);
		QRadioButton *rb_1555 = new QRadioButton("15 bpp (1555)", depthGroup);
		QRadioButton *rb_565 = new QRadioButton("16 bpp (565)", depthGroup);
		QRadioButton *rb_888 = new QRadioButton("24 bpp (888)", depthGroup);
		QRadioButton *rb_8888 = new QRadioButton("32 bpp (8888)", depthGroup);
		depthLayout->addWidget(rb_depth_current);
		depthLayout->addWidget(rb_1bpp);
		depthLayout->addWidget(rb_2bpp);
		depthLayout->addWidget(rb_4bpp);
		depthLayout->addWidget(rb_8bpp);
		depthLayout->addWidget(rb_1555);
		depthLayout->addWidget(rb_565);
		depthLayout->addWidget(rb_888);
		depthLayout->addWidget(rb_8888);
		panelLayout->addWidget(depthGroup);

		/* Address Mode group */
		QGroupBox *addrModeGroup = new QGroupBox("Address Mode", panel);
		QVBoxLayout *addrModeLayout = new QVBoxLayout(addrModeGroup);
		QRadioButton *rb_addrmode_current = new QRadioButton("Current", addrModeGroup);
		rb_addrmode_current->setChecked(true);
		QRadioButton *rb_addrmode_normal = new QRadioButton("Normal", addrModeGroup);
		QRadioButton *rb_addrmode_oddeven = new QRadioButton("Odd/Even", addrModeGroup);
		QRadioButton *rb_addrmode_chain4 = new QRadioButton("Chain 4", addrModeGroup);
		addrModeLayout->addWidget(rb_addrmode_current);
		addrModeLayout->addWidget(rb_addrmode_normal);
		addrModeLayout->addWidget(rb_addrmode_oddeven);
		addrModeLayout->addWidget(rb_addrmode_chain4);
		panelLayout->addWidget(addrModeGroup);

		/* Scale */
		QHBoxLayout *scaleLayout = new QHBoxLayout();
		scaleLayout->addWidget(new QLabel("Scale:", panel));
		QSpinBox *scaleSpin = new QSpinBox(panel);
		scaleSpin->setRange(1, 8);
		scaleSpin->setValue(1);
		scaleLayout->addWidget(scaleSpin);
		panelLayout->addLayout(scaleLayout);

		panelLayout->addStretch();

		sz->addWidget(panel);

		/* Canvas with scrollbars */
		QWidget *canvasContainer = new QWidget(central);
		QGridLayout *canvasGrid = new QGridLayout(canvasContainer);
		canvasGrid->setContentsMargins(0, 0, 0, 0);
		canvasGrid->setSpacing(0);

		canvas = new ViewerVRAMCanvas(canvasContainer, (svga_t *)p, this);

		QScrollBar *hScroll = new QScrollBar(Qt::Horizontal, canvasContainer);
		QScrollBar *vScroll = new QScrollBar(Qt::Vertical, canvasContainer);
		canvas->setScrollBars(hScroll, vScroll);

		canvasGrid->addWidget(canvas, 0, 0);
		canvasGrid->addWidget(vScroll, 0, 1);
		canvasGrid->addWidget(hScroll, 1, 0);

		sz->addWidget(canvasContainer, 1);

		/* Connect signals */
		connect(rb_startaddr_current, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_start_addr(false);
		});
		connect(rb_startaddr_custom, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_start_addr(true);
		});
		connect(rb_pitch_current, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_pitch(false);
		});
		connect(rb_pitch_custom, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_pitch(true);
		});
		connect(rb_depth_current, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(false);
		});
		connect(rb_1bpp, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_1BPP);
		});
		connect(rb_2bpp, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_2BPP);
		});
		connect(rb_4bpp, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_4BPP);
		});
		connect(rb_8bpp, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_8BPP);
		});
		connect(rb_1555, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_1555);
		});
		connect(rb_565, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_565);
		});
		connect(rb_888, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_888);
		});
		connect(rb_8888, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_colour_depth(true);
			canvas->set_custom_colour_depth(CD_8888);
		});
		connect(rb_addrmode_current, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_addr_mode(false);
		});
		connect(rb_addrmode_normal, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_addr_mode(true);
			canvas->set_custom_addr_mode(AM_NORMAL);
		});
		connect(rb_addrmode_oddeven, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_addr_mode(true);
			canvas->set_custom_addr_mode(AM_ODDEVEN);
		});
		connect(rb_addrmode_chain4, &QRadioButton::clicked, this, [this]() {
			canvas->set_use_custom_addr_mode(true);
			canvas->set_custom_addr_mode(AM_CHAIN4);
		});

		connect(startAddrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
			bool ok;
			unsigned long addr = text.toULong(&ok, 16);
			if (ok)
				canvas->set_custom_start_addr(addr);
		});
		connect(pitchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
			bool ok;
			unsigned long pitch = text.toULong(&ok, 10);
			if (ok)
				canvas->set_custom_pitch(pitch);
		});
		connect(scaleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
			canvas->set_scale_factor(value);
		});
	}

	virtual ~ViewerVRAM()
	{
		delete canvas;
	}

	friend class ViewerVRAMCanvas;
};

void ViewerVRAMCanvas::UpdateLabels()
{
	current_start_addr = svga->ma_latch * 4;
	current_pitch = svga->rowoffset;
	current_bpp = svga->video_bpp;

	char s[128];

	sprintf(s, "Current (%x)", current_start_addr);
	vram_parent->rb_startaddr_current->setText(s);

	sprintf(s, "Current (%u)", current_pitch);
	vram_parent->rb_pitch_current->setText(s);

	switch (current_bpp)
	{
		case 1:
		vram_parent->rb_depth_current->setText("Current (2 cols)");
		break;
		case 2:
		vram_parent->rb_depth_current->setText("Current (4 cols)");
		break;
		case 4:
		vram_parent->rb_depth_current->setText("Current (16 cols)");
		break;
		case 8:
		vram_parent->rb_depth_current->setText("Current (256 cols)");
		break;
		case 15:
		vram_parent->rb_depth_current->setText("Current (1555)");
		break;
		case 16:
		vram_parent->rb_depth_current->setText("Current (555)");
		break;
		case 24:
		vram_parent->rb_depth_current->setText("Current (888)");
		break;
		case 32:
		vram_parent->rb_depth_current->setText("Current (8888)");
		break;
		default:
		vram_parent->rb_depth_current->setText("Current");
		break;
	}
}

static void *viewer_vram_open(void *parent, void *p, const char *title)
{
	ViewerVRAM *w = new ViewerVRAM((QWidget *)parent, title, QSize(800, 600), p);

	w->show();

	return w;
}

viewer_t viewer_vram =
{
	.open = viewer_vram_open
};

#include "viewer_vram.moc"
