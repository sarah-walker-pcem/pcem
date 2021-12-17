#include "wx-app.h"
#include <SDL2/SDL.h>

extern "C"
{
int pc_main(int, char**);
}

#ifdef WIN32
#include <winbase.h>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char*, int nShowCmd)
{
    return main(__argc, __argv);
}
#endif

int main(int argc, char **argv)
{
        if (!pc_main(argc, argv))
                return -1;

        wxApp::SetInstance(new App());
        wxEntry(argc, argv);
        return 0;
}

