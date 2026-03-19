#include <QCloseEvent>
#include <QImage>
#include <QPainter>
#include <QWidget>

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
	Q_OBJECT
private:
	svga_t *svga;
	QImage buffer;

protected:
	void paintEvent(QPaintEvent *event) override
	{
		QPainter painter(this);
		int w = width();
		int h = height();

		painter.fillRect(0, 0, w, h, Qt::black);

		unsigned char *buffer_data = buffer.bits();
		int buffer_width = buffer.width();
		int bytes_per_line = buffer.bytesPerLine();

		for (int y = 0; y < 16; y++)
		{
			int font_base = ((y & 8) ? svga->charsetb : svga->charseta) + ((y & 7) * 32 * 128);

			for (int x = 0; x < 32; x++)
			{
				int font_addr = font_base + x * 128;

				for (int yy = 0; yy < 16; yy++)
				{
					unsigned char *data = buffer_data + (y * 16 + yy) * bytes_per_line + x * 8 * 3;

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

		QPixmap pixmap = QPixmap::fromImage(buffer);
		painter.drawPixmap(QRect(0, 0, w, h), pixmap, QRect(0, 0, 256, 256));
	}

	void closeEvent(QCloseEvent *event) override
	{
		viewer_remove(this);
		event->accept();
	}

public:
	ViewerFont(QWidget *parent, QString title, QSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(256, 256, QImage::Format_RGB888)
	{
		buffer.fill(Qt::black);
	}

	virtual ~ViewerFont()
	{
	}
};

static void *viewer_font_open(void *parent, void *p, const char *title)
{
	ViewerFont *w = new ViewerFont((QWidget *)parent, title, QSize(256, 256), p);

	w->show();

	return w;
}

viewer_t viewer_font =
{
	.open = viewer_font_open
};

#include "viewer_font.moc"
