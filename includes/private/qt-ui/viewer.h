#ifndef _VIEWER_H_
#define _VIEWER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct viewer_t
{
	void *(*open)(void *parent, void *p, const char *title);
} viewer_t;

void viewer_reset();
void viewer_add(char *title, viewer_t *viewer, void *p);
void viewer_open(void *hwnd, int id);
void viewer_remove(void *viewer);
void viewer_update(viewer_t *viewer, void *p);
void viewer_call(viewer_t *viewer, void *p, void (*func)(void *v, void *param), void *param);
void viewer_close_all();
void viewer_notify_pause();
void viewer_notify_resume();
void update_viewers_menu(void *menu);

extern viewer_t viewer_font;
extern viewer_t viewer_palette;
extern viewer_t viewer_palette_16;
extern viewer_t viewer_voodoo;
extern viewer_t viewer_vram;

#define IDM_VIEWER 1600
#define IDM_VIEWER_MAX 1700

#ifdef __cplusplus
}

#include <QMainWindow>

class Viewer : public QMainWindow
{
        Q_OBJECT
public:
	void *p;

	Viewer(QWidget *parent, QString title, QSize size, void *p)
	: QMainWindow(parent),
	  p(p)
	{
		setWindowTitle(title);
		resize(size);
	}

	virtual ~Viewer()
	{
	}

	virtual void notifyPause()
	{
	}

	virtual void notifyResume()
	{
	}
};

#endif

#endif
