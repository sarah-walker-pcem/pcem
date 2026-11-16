#ifndef _PCEM_WX_DISPLAY_H_
#define _PCEM_WX_DISPLAY_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "sdl2-host.h"

class DriveStatusPanel;

class DisplayFrame : public wxFrame {
    public:
        DisplayFrame(wxWindow *owner, wxMenuBar *menubar, const char *title, SDL_DISPLAY_CALLBACK callback);

        static void *HostNativeCanvas(void *frame);
        static void HostCanvasSize(void *frame, int *width, int *height);
        static void HostSetSize(void *frame, int width, int height, int resizable);
        static void HostGetRect(void *frame, int *x, int *y, int *width, int *height);
        static void HostSetPosition(void *frame, int x, int y);
        static void HostCenter(void *frame);
        static void HostShow(void *frame, int show);
        static void HostSetFullscreen(void *frame, int fullscreen);
        static void HostSetTitle(void *frame, const char *title);
        static void HostSetStatus(void *frame, const char *message, int speed, int fps);

    private:
        SDL_DISPLAY_CALLBACK callback;
        wxPanel *canvas;
        DriveStatusPanel *drives;

        void *NativeCanvas();
        void SetDisplaySize(int width, int height, bool resizable);
        void ShowDisplay(bool show);
        void SetDisplayFullscreen(bool fullscreen);
        void SetStatus(const char *message, int speed, int fps);
        void SetStatusField(int field, const wxString &text);
        void LayoutStatus();
        int TopSafeInset();
};

#endif /* _PCEM_WX_DISPLAY_H_ */
