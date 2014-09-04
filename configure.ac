# configure.ac for PCem
#

AC_PREREQ([2.61])
AC_INIT(PCem, v9, Tom Walker <tommowalker@tommowalker.co.uk>, pcem)

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

#AC_MSG_CHECKING([for libz])
#AX_CHECK_ZLIB

AM_CONDITIONAL(OS_WIN, test "$OS" = "win")
AM_CONDITIONAL(OS_LINUX, test "$OS" = "linux")

# Do not run test for Allegro with Win32/MinGW version, as binary builds have
# `allegro-config' missing.
# NOTE: For the following Autoconf macro to be supported, you need to extract
#       allegro.m4 from the DOS/Windows Allegro sources (the file is contained
#       in `misc') and copy it to this directory or MSYS's `/share/aclocal'.
if test "$OS" != "win"; then
   AM_PATH_ALLEGRO(, , AC_MSG_ERROR(building B-em requires Allegro to be installed))
fi

AC_CHECK_LIB([openal], [alGetError], [], \
	[echo "You need to install the OpenAL library."
	 exit -1])
AC_CHECK_LIB([alut], [alutInit], [], \
   [echo "You need to install the ALUT library."
    exit -1])


AC_OUTPUT([Makefile src/Makefile])
