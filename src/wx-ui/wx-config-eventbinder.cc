#include "wx-config-eventbinder.h"

#include <wx/listctrl.h>
#include <wx/window.h>
#include <wx/xrc/xmlres.h>

class WXConfigEventBinder : public wxWindow
{
private:
    void (*selectedPageCallback)(void *hdlg, int selectedPage);
    wxWindow *hdlg;

public:
    void selectedPageCallbackExecute(wxListEvent &event)
    {
        long item = -1;
        wxListCtrl *control = (wxListCtrl *)this->hdlg->FindWindow(XRCID("IDC_PAGESELECTION"));

        for (;;)
        {
            item = control->GetNextItem(item,
                                        wxLIST_NEXT_ALL,
                                        wxLIST_STATE_SELECTED);
            if (item == -1)
                break;
            if (this->selectedPageCallback != NULL)
                this->selectedPageCallback(this->hdlg, item);
        }
    }

    WXConfigEventBinder(wxWindow *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage))
    {
        this->selectedPageCallback = selectedPageCallback;
        this->hdlg = hdlg;

        wxListCtrl *control = (wxListCtrl *)this->hdlg->FindWindow(XRCID("IDC_PAGESELECTION"));
        control->SetMaxSize(wxSize(250, -1));
        control->SetMinSize(wxSize(250, -1));
        control->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( WXConfigEventBinder::selectedPageCallbackExecute ), NULL, this );
    }

    ~WXConfigEventBinder()
    {
        wxListCtrl *control = (wxListCtrl *)this->hdlg->FindWindow(XRCID("IDC_PAGESELECTION"));
        control->Disconnect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( WXConfigEventBinder::selectedPageCallbackExecute ), NULL, this );
    }
};

void *wx_config_eventbinder(void *hdlg, void (*selectedPageCallback)(void *hdlg, int selectedPage))
{
    WXConfigEventBinder *binder = new WXConfigEventBinder((wxWindow *)hdlg, selectedPageCallback);
    return (void *)binder;
}

void wx_config_destroyeventbinder(void *eventBinder)
{
    delete (WXConfigEventBinder *)eventBinder;
}