PCem v16 BSD supplement


You will need the following libraries :

SDL2
wxWidgets 3.x
OpenAL

and their dependencies.

Open a terminal window, navigate to the PCem directory then enter

./configure --enable-release-build
gmake

then ./pcem to run.

The BSD version stores BIOS ROM images, configuration files, and other data in ~/.pcem

configure options are :
  --enable-release-build : Generate release build. Recommended for regular use.
  --enable-debug         : Compile with debugging enabled.
  --enable-networking    : Build with networking support.
  --enable-alsa          : Build with support for MIDI output through ALSA. Requires libasound.


The menu is a pop-up menu in the BSD port. Right-click on the main window when mouse is not
captured.

CD-ROM support currently only accesses /dev/cdrom. It has not been heavily tested.
