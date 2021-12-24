extern int pcem_lib_main(int argc, char **argv);

#ifdef WIN32
#include <winbase.h>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char*, int nShowCmd)
{
    return pcem_lib_main(__argc, __argv);
}
#else
int main(int argc, char **argv)
{
    return pcem_lib_main(argc, argv);
}
#endif
