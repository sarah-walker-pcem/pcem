# configure.ac for PCem
#

AC_PREREQ([2.61])
AC_INIT(PCem, v14, Sarah Walker <pcem@pcem-emulator.co.uk>, pcem)

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O



AC_MSG_CHECKING([whether to build for release])
AC_ARG_ENABLE(release_build,
          AC_HELP_STRING([--enable-release-build], [build for release]))
if test "$enable_release_build" = "yes"; then
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(RELEASE_BUILD, [test "$enable_release_build" = "yes"])

AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(debug,
          AC_HELP_STRING([--enable-debug], [build debug executable]))
if test "$enable_debug" = "yes"; then
   CFLAGS="-Wall -O0 -g -D_DEBUG"
   CXXFLAGS="-Wall -O0 -g -D_DEBUG"
   AC_MSG_RESULT([yes])
else
   CFLAGS="-O3"
   CXXFLAGS="-O3"
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to enable networking])
AC_ARG_ENABLE(networking,
          AC_HELP_STRING([--enable-networking], [enable networking]))
if test "$enable_networking" = "yes"; then
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to use ALSA for MIDI output])
AC_ARG_ENABLE(alsa,
          AC_HELP_STRING([--enable-alsa], [use ALSA for MIDI]))
if test "$enable_alsa" = "yes"; then
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([for cpu])
case "${host_cpu}" in
    i?86)
    CPU=i386
    AC_MSG_RESULT(${host_cpu})
    ;;
    x86_64)
    CPU=x86_64
    AC_MSG_RESULT(${host_cpu})
    ;;
    armv7l)
    CPU=arm
    AC_MSG_RESULT(${host_cpu})
    ;;
    aarch64)
    CPU=arm64
    AC_MSG_RESULT(${host_cpu})
    ;;
    *)
    AC_MSG_ERROR([Unsupported CPU. ${host_cpu}])
    ;;
esac

AM_CONDITIONAL(CPU_I386, test "$CPU" = "i386")
AM_CONDITIONAL(CPU_X86_64, test "$CPU" = "x86_64")
AM_CONDITIONAL(CPU_ARM, test "$CPU" = "arm")
AM_CONDITIONAL(CPU_ARM64, test "$CPU" = "arm64")

AC_MSG_CHECKING([for off64_t $CPU])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]],[[off64_t n=0;]])],[has_lfs="yes"],[has_lfs="no"])
case "$host" in
    *-*-freebsd*) # has off64_t but no fopen64/etc.
       has_lfs="no"
       ;;
esac
AC_MSG_RESULT($has_lfs)

#AC_MSG_CHECKING([for libz])
#AX_CHECK_ZLIB

AM_CONDITIONAL(USE_WX, test "$enable_wx" = "yes")
AM_CONDITIONAL(USE_NETWORKING, test "$enable_networking" = "yes")
AM_CONDITIONAL(USE_ALSA, test "$enable_alsa" = "yes")

AM_CONDITIONAL(HAS_OFF64T, test "$has_lfs" = "yes")

SDL_VERSION=2.0.1
AM_PATH_SDL2($SDL_VERSION,
   :,
   AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

AM_OPTIONS_WXCONFIG
reqwx=3.0.0
AM_PATH_WXCONFIG($reqwx, wxWin=1)
if test "$wxWin" != 1; then
   AC_MSG_ERROR([
      wxWidgets must be installed on your system.
 
      Please check that wx-config is in path, the directory
      where wxWidgets libraries are installed (returned by
      'wx-config --libs' or 'wx-config --static --libs' command)
      is in LD_LIBRARY_PATH or equivalent variable and
      wxWidgets version is $reqwx or above.
      ])
fi

if test "$enable_alsa" == "yes"; then
    AC_CHECK_LIB(asound, snd_pcm_open,
        [],
        [echo "You need to install the ALSA library."
     exit -1])
fi

AC_CHECK_LIB([pthread], [pthread_create])

build_macosx="no"
build_linux="no"
build_win="no"
build_other="no"

case "$host" in
    *-*-darwin*)
       LIBS="$LIBS -framework OpenGL"
       LIBS="$LIBS -framework OpenAL"
       build_macosx="yes"
       ;;
    *-*-cygwin* | *-*-mingw32*)
       AC_CHECK_LIB([opengl32], [main], [], \
           [echo "You need to install the OpenGL library."
           exit -1])
       AC_CHECK_LIB([openal], [alGetError], [], \
          [echo "You need to install the OpenAL library."
           exit -1])
       build_win="yes"
       AC_SUBST(SET_MAKE, 'MAKE=mingw32-make')
       ;;
    *-*-linux*)
       AC_CHECK_LIB([GL], [glGetError], [], \
           [echo "You need to install the OpenGL library."
           exit -1])
       AC_CHECK_LIB([openal], [alGetError], [], \
           [echo "You need to install the OpenAL library."
           exit -1])
       build_linux="yes"
       ;;
    *-*-freebsd*)
       CFLAGS="$CFLAGS -I/usr/local/include"
       CXXFLAGS="$CXXFLAGS -I/usr/local/include"
       LDFLAGS="$LDFLAGS -L/usr/local/lib"
       AC_CHECK_LIB([GL], [glGetError], [], \
           [echo "You need to install the OpenGL library."
           exit -1])
       AC_CHECK_LIB([openal], [alGetError], [], \
           [echo "You need to install the OpenAL library."
           exit -1])
       build_other="yes"
       ;;
    *)
       AC_CHECK_LIB([GL], [glGetError], [], \
           [echo "You need to install the OpenGL library."
           exit -1])
       AC_CHECK_LIB([openal], [alGetError], [], \
           [echo "You need to install the OpenAL library."
           exit -1])
       build_other="yes"
       ;;
esac

LIBS="$LIBS $WX_LIBS $SDL_LIBS"

AM_CONDITIONAL(OS_LINUX, [test "$build_linux" = "yes"])
AM_CONDITIONAL(OS_WINDOWS, [test "$build_win" = "yes"])
AM_CONDITIONAL(OS_OTHER, [test "$build_other" = "yes"])
AM_CONDITIONAL(OS_MACOSX, [test "$build_macosx" = "yes"])

AC_OUTPUT([Makefile src/Makefile])
