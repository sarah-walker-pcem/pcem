#include "wx-app.h"
#include <SDL2/SDL.h>

#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C"
{
int pc_main(int, char **);
int main(int argc, char **argv);
}

#if WIN32
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *, int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
#if WIN32
	int argc = __argc;
	char **argv = __argv;
#endif

	if (!pc_main(argc, argv))
		return -1;

	wxApp::SetInstance(new App());
	wxEntry(argc, argv);
	return 0;
}

