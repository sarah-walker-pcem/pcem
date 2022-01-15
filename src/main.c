extern int pcem_lib_main(int argc, char **argv);

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *pCmdLine, int nShowCmd)
{
    return pcem_lib_main(__argc, __argv);
}
#else
int main(int argc, char **argv)
{
    return pcem_lib_main(argc, argv);
}
#endif
