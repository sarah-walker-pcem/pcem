#ifndef _WX_VIEWER_H_
#define _WX_VIEWER_H_

#include <wx/wx.h>
#include "viewer.h"

class Viewer: public wxFrame
{
public:
	void *p;

	Viewer(wxWindow *parent, wxString title, wxSize size, void *p)
	: wxFrame(parent, wxID_ANY, title, wxPoint(50, 50), size, wxDEFAULT_FRAME_STYLE),
	  p(p)
	{
		SetClientSize(size);
	}

	virtual ~Viewer()
	{
	}

	virtual void NotifyPause()
	{
	}

	virtual void NotifyResume()
	{
	}
};

#endif /* _WX_VIEWER_H_ */
