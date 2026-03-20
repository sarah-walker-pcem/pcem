#include <QApplication>
#include <SDL2/SDL.h>

#include "qt-app.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" {
int pc_main(int, char **);
int main(int argc, char **argv);
}

#ifdef _WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *, int nShowCmd)
#else
int main(int argc, char **argv)
#endif
{
#ifdef _WIN32
        int argc = __argc;
        char **argv = __argv;
#endif

#ifdef FORCE_X11
        setenv("SDL_VIDEODRIVER", "x11", 1);
#endif

        if (!pc_main(argc, argv))
                return -1;

        QApplication app(argc, argv);
        app.setApplicationName("PCem");

        extern int infocus;
        infocus = 1;
        QObject::connect(&app, &QApplication::applicationStateChanged, [](Qt::ApplicationState state) {
                extern int infocus;
                infocus = (state == Qt::ApplicationActive) ? 1 : 0;
        });

        MainWindow mainWindow;
        mainWindow.start();

        return app.exec();
}
