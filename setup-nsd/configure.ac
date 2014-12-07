# Process this file with autoconf to produce a configure script.
AC_INIT(src/nsd.c)
AC_PROG_CC
AM_CONFIG_HEADER(src/config.h)
AM_INIT_AUTOMAKE(NeuroServer, 0.7.4)

# Checks for programs.

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(strings.h unistd.h)
AC_CHECK_HEADER(windows.h, [
LIBS="$LIBS -lwsock32"
AM_CONDITIONAL(HAVE_WINDOWS_H, true)
],
AM_CONDITIONAL(HAVE_WINDOWS_H, false)
)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(strerror)
#AC_PROG_INSTALL()
AC_OUTPUT([
Makefile
src/Makefile
scripts/NeuroServer.iss
])


