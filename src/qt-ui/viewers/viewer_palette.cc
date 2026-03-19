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

class ViewerPalette: public Viewer
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

		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				QRgb color = qRgb(
					svga->vgapal[x + y*16].r * 4,
					svga->vgapal[x + y*16].g * 4,
					svga->vgapal[x + y*16].b * 4
				);
				buffer.setPixel(x, y, color);
			}
		}

		QPixmap pixmap = QPixmap::fromImage(buffer);
		painter.drawPixmap(QRect(0, 0, w, h), pixmap, QRect(0, 0, 16, 16));
	}

	void closeEvent(QCloseEvent *event) override
	{
		viewer_remove(this);
		event->accept();
	}

public:
	ViewerPalette(QWidget *parent, QString title, QSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(16, 16, QImage::Format_RGB888)
	{
		buffer.fill(Qt::black);
	}

	virtual ~ViewerPalette()
	{
	}
};

static void *viewer_palette_open(void *parent, void *p, const char *title)
{
	ViewerPalette *w = new ViewerPalette((QWidget *)parent, title, QSize(256, 256), p);

	w->show();

	return w;
}

viewer_t viewer_palette =
{
	.open = viewer_palette_open
};

class ViewerPalette16: public Viewer
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

		for (int x = 0; x < 16; x++)
		{
			QRgb color = qRgb(
				svga->vgapal[svga->egapal[x]].r * 4,
				svga->vgapal[svga->egapal[x]].g * 4,
				svga->vgapal[svga->egapal[x]].b * 4
			);
			buffer.setPixel(x, 0, color);
		}

		QPixmap pixmap = QPixmap::fromImage(buffer);
		painter.drawPixmap(QRect(0, 0, w, h), pixmap, QRect(0, 0, 16, 1));
	}

	void closeEvent(QCloseEvent *event) override
	{
		viewer_remove(this);
		event->accept();
	}

public:
	ViewerPalette16(QWidget *parent, QString title, QSize size, void *p)
	: Viewer(parent, title, size, p),
	  svga((svga_t *)p),
	  buffer(16, 1, QImage::Format_RGB888)
	{
		buffer.fill(Qt::black);
	}

	virtual ~ViewerPalette16()
	{
	}
};

static void *viewer_palette_16_open(void *parent, void *p, const char *title)
{
	ViewerPalette16 *w = new ViewerPalette16((QWidget *)parent, title, QSize(256, 20), p);

	w->show();

	return w;
}

viewer_t viewer_palette_16 =
{
	.open = viewer_palette_16_open
};

#include "viewer_palette.moc"
