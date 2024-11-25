#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/xrc/xmlres.h>

#include "viewer.h"
#include <list>
#include <string>
#include <vector>

extern "C" void pclog(const char *format, ...);

class ViewerRout
{
public:
	const std::string title;
	viewer_t *viewer;
	void *p;

	ViewerRout()
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
		(*it)->Close(true);
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
			(*it)->Refresh();
		}
	}
}

void viewer_notify_pause()
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		(*it)->NotifyPause();
	}
}

void viewer_notify_resume()
{
	for (std::list<Viewer *>::iterator it = viewer_windows.begin(); it != viewer_windows.end(); it++)
	{
		(*it)->NotifyResume();
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
	wxMenuItem *m = ((wxMenu *)menu)->FindItem(XRCID("IDM_VIEW"));
	wxMenu *wm = m->GetSubMenu();

	for (int i = IDM_VIEWER; i < IDM_VIEWER_MAX; i++)
	{
		wxMenuItem *menu_item = wm->FindChildItem(i);

		if (menu_item)
			wm->Delete(i);
	}

	int id = IDM_VIEWER;

	for (std::vector<ViewerRout>::iterator it = viewer_routs.begin(); it != viewer_routs.end(); it++)
	{
		wm->Append(id++, (*it).title, wxEmptyString, wxITEM_NORMAL);
	}
}