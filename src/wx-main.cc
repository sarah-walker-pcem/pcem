#include "wx-app.h"

extern "C"
{
int pc_main(int, char**);
}

int main(int argc, char **argv)
{
    pc_main(argc, argv);

    wxApp::SetInstance(new App());
    wxEntry(argc, argv);
}

