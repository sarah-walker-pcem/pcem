# configure.ac for PCem
#

AC_PREREQ([2.61])
AC_INIT(PCem, v12, Sarah Walker <pcem@pcem-emulator.co.uk>, pcem)

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O



AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(debug,
	      AC_HELP_STRING([--enable-debug], [build debug executable]))
if test "$enable_debug" = "yes"; then
   CFLAGS="-Wall -O0 -g -D_DEBUG"
   AC_MSG_RESULT([yes])
else
   CFLAGS="-O3"
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to use the wxWidgets-based GUI])
AC_ARG_ENABLE(wx,
	      AC_HELP_STRING([--enable-wx], [use wxWidgets GUI]))
if test "$enable_wx" = "yes"; then
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
	*)
	AC_MSG_ERROR([Unsupported CPU.])
	;;
esac

AM_CONDITIONAL(CPU_I386, test "$CPU" = "i386")
AM_CONDITIONAL(CPU_X86_64, test "$CPU" = "x86_64")

#AC_MSG_CHECKING([for libz])
#AX_CHECK_ZLIB

AM_CONDITIONAL(OS_WIN, test "$OS" = "win")
AM_CONDITIONAL(OS_LINUX, test "$OS" = "linux")

AM_CONDITIONAL(USE_WX, test "$enable_wx" = "yes")

if test "$enable_wx" != "yes"; then
   # Do not run test for Allegro with Win32/MinGW version, as binary builds have
   # `allegro-config' missing.
   # NOTE: For the following Autoconf macro to be supported, you need to extract
   #       allegro.m4 from the DOS/Windows Allegro sources (the file is contained
   #       in `misc') and copy it to this directory or MSYS's `/share/aclocal'.
   if test "$OS" != "win"; then
      AC_CONFIG_MACRO_DIR([./])
      AM_PATH_ALLEGRO(, , AC_MSG_ERROR(building PCem requires Allegro to be installed))
   fi
else
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
   AC_CHECK_LIB([GL], [glGetError], [], \
      [echo "You need to install the OpenGL library."
       exit -1])
   LIBS="$LIBS $WX_LIBS $SDL_LIBS"
fi

AC_CHECK_LIB([openal], [alGetError], [], \
	[echo "You need to install the OpenAL library."
	 exit -1])

AC_CHECK_LIB([pthread], [pthread_create])

AC_OUTPUT([Makefile src/Makefile])
