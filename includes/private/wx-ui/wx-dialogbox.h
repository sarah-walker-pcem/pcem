#ifndef SRC_WX_PCEMDIALOGBOX_H_
#define SRC_WX_PCEMDIALOGBOX_H_

#include "wx/dialog.h"

#include "wx-utils.h"

class PCemDialogBox: public wxDialog {
public:
	PCemDialogBox(wxWindow* parent, int(*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2));
	PCemDialogBox(wxWindow* parent, const char* name, int(*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2));
	virtual ~PCemDialogBox() {}
	void OnInit();
private:
	void OnNotebookChanged(wxCommandEvent& event);
	void OnCommand(wxCommandEvent& event);
	int processEvent(int message, INT_PARAM param1, LONG_PARAM param2);
	int(*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2);
	bool commandActive;

	DECLARE_EVENT_TABLE()
};

#endif /* SRC_WX_PCEMDIALOGBOX_H_ */
