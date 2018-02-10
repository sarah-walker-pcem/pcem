#include "wx-app.h"
#include <SDL2/SDL.h>

extern "C"
{
int pc_main(int, char**);
}

int main(int argc, char **argv)
{
        if (!pc_main(argc, argv))
                return -1;

        wxApp::SetInstance(new App());
        wxEntry(argc, argv);
        return 0;
}

