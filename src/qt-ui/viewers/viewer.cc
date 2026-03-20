#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "viewer.h"
#include <list>
#include <string>
#include <vector>

extern "C" {
void pclog(const char *format, ...);
void wx_handle_command(void *, int, int);
extern void *ghwnd;
}

class ViewerRout
{
public:
	const std::string title;
	viewer_t *viewer;
	void *p;

	ViewerRout()
		: viewer(nullptr), p(nullptr)
	{
	}
	ViewerRout(char *title, viewer_t *viewer, void *p) : title(title), viewer(viewer), p(p)
	{
	}
};

std::vector<ViewerRout> viewer_routs;
std::list<Viewer *> viewer_windows;

void viewer_reset()
{
	viewer_routs.clear();
	viewer_windows.clear();
}

void viewer_close_all()
{
	std::list<Viewer *> viewer_windows_2 = viewer_windows;

	for (std::list<Viewer *>::iterator it = viewer_windows_2.begin(); it != viewer_windows_2.end(); it++)
		(*it)->close();
}

void viewer_add(char *title, viewer_t *viewer, void *p)
{
	viewer_routs.push_back(ViewerRout(title, viewer, p));
}

void viewer_open(void *parent, int id)
{
	Viewer *i = (Viewer *)viewer_routs[id].viewer->open(parent, viewer_routs[id].p, viewer_routs[id].title.c_str());
	if (i)
		viewer_windows.push_back(i);
}

void viewer_remove(void *viewer)
{
	viewer_windows.remove((Viewer *)viewer);
}

void viewer_update(viewer_t *viewer, void *p)
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		if ((*it)->p == p)
		{
			(*it)->update();
		}
	}
}

void viewer_notify_pause()
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		(*it)->notifyPause();
	}
}

void viewer_notify_resume()
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		(*it)->notifyResume();
	}
}

void viewer_call(viewer_t *viewer, void *p, void (*func)(void *v, void *param), void *param)
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		if ((*it)->p == p)
		{
			func(*it, param);
		}
	}
}

void update_viewers_menu(void *menu)
{
	QMenuBar *menuBar = (QMenuBar *)menu;
	QMenu *viewMenu = nullptr;

	/* Find the View menu */
	for (QAction *action : menuBar->actions()) {
		if (action->text() == "View" || action->text() == "&View") {
			viewMenu = action->menu();
			break;
		}
	}

	if (!viewMenu)
		return;

	/* Remove existing viewer menu items */
	QList<QAction *> actions = viewMenu->actions();
	for (QAction *action : actions) {
		int actionId = action->data().toInt();
		if (actionId >= IDM_VIEWER && actionId < IDM_VIEWER_MAX) {
			viewMenu->removeAction(action);
			delete action;
		}
	}

	/* Add new viewer menu items */
	int id = IDM_VIEWER;

	for (std::vector<ViewerRout>::iterator it = viewer_routs.begin(); it != viewer_routs.end(); it++)
	{
		QAction *action = viewMenu->addAction(QString::fromStdString((*it).title));
		action->setData(id);
		int viewerId = id;
		QObject::connect(action, &QAction::triggered, [viewerId]() {
			wx_handle_command(ghwnd, viewerId, 0);
		});
		id++;
	}
}
