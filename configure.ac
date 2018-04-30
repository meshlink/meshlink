dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([MeshLink], [0.1])
AM_INIT_AUTOMAKE([std-options subdir-objects nostdinc silent-rules -Wall])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SUBDIRS([catta])
AM_SILENT_RULES([yes])

# Enable GNU extensions.
# Define this here, not in acconfig's @TOP@ section, since definitions
# in the latter don't make it into the configure-time tests.
AC_GNU_SOURCE
AC_DEFINE([__USE_BSD], 1, [Enable BSD extensions])

dnl Checks for programs.
AM_PROG_CC_C_O
AM_PROG_AR
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AX_PTHREAD

dnl Check and set OS

AC_CANONICAL_HOST

case $host_os in
  *linux*)
    linux=true
    AC_DEFINE(HAVE_LINUX, 1, [Linux])
  ;;
  *mingw*)
    mingw=true
    AC_DEFINE(HAVE_MINGW, 1, [MinGW])
    LIBS="$LIBS -lws2_32 -lgdi32 -lcrypt32"
  ;;
esac

AM_CONDITIONAL(LINUX, test "$linux" = true)
AM_CONDITIONAL(MINGW, test "$mingw" = true)

AC_CACHE_SAVE

if test -d /sw/include ; then
  CPPFLAGS="$CPPFLAGS -I/sw/include"
fi
if test -d /sw/lib ; then
  LIBS="$LIBS -L/sw/lib"
fi

dnl Compiler hardening flags
dnl No -fstack-protector-all because it doesn't work on all platforms or architectures.

AC_ARG_ENABLE([hardening], AS_HELP_STRING([--disable-hardening], [disable compiler and linker hardening flags]))
AS_IF([test "x$enable_hardening" != "xno"],
  [AX_CHECK_COMPILE_FLAG([-DFORTIFY_SOURCE=2], [CPPFLAGS="$CPPFLAGS -DFORITFY_SOURCE=2"])
   AX_CHECK_COMPILE_FLAG([-fwrapv], [CPPFLAGS="$CPPFLAGS -fwrapv"],
     [AX_CHECK_COMPILE_FLAG([-fno-strict-overflow], [CPPFLAGS="$CPPFLAGS -fno-strict-overflow"])]
   )
   case $host_os in
     *mingw*)
       AX_CHECK_LINK_FLAG([-Wl,--dynamicbase], [LDFLAGS="$LDFLAGS -Wl,--dynamicbase"])
       AX_CHECK_LINK_FLAG([-Wl,--nxcompat], [LDFLAGS="$LDFLAGS -Wl,--nxcompat"])
       ;;
     *)
       AX_CHECK_COMPILE_FLAG([-fPIE], [CPPFLAGS="$CPPFLAGS -fPIE"])
       AX_CHECK_COMPILE_FLAG([-fPIC], [CPPFLAGS="$CPPFLAGS -fPIC"])
       AX_CHECK_LINK_FLAG([-pie], [LDFLAGS="$LDFLAGS -pie"])
       ;;
   esac
   AX_CHECK_LINK_FLAG([-Wl,-z,relro], [LDFLAGS="$LDFLAGS -Wl,-z,relro"])
   AX_CHECK_LINK_FLAG([-Wl,-z,now], [LDFLAGS="$LDFLAGS -Wl,-z,now"])
  ]
);

dnl UTCP debug flags
AC_ARG_ENABLE([utcp_debug], AS_HELP_STRING([--enable-utcp-debug], [compile utcp with debug output]))
AS_IF([test "x$enable_utcp_debug" = "xyes"],
  [AX_CHECK_COMPILE_FLAG([-DUTCP_DEBUG], [CPPFLAGS="$CPPFLAGS -DUTCP_DEBUG"])
  ]
);

dnl Install test binaries
AC_ARG_ENABLE([install_tests],
  [AS_HELP_STRING([--enable-install-tests], [include test binaries in installation])],
  [AS_IF([test "x$enable_install_tests" = "xyes"], [install_tests=true], [install_tests=false])],
  [install_tests=false]
)

AM_CONDITIONAL(INSTALL_TESTS, test "$install_tests" = true)

dnl Checks for header files.
dnl We do this in multiple stages, because unlike Linux all the other operating systems really suck and don't include their own dependencies.

AC_CHECK_HEADERS([syslog.h sys/file.h sys/param.h sys/resource.h sys/socket.h sys/time.h sys/un.h sys/wait.h netdb.h arpa/inet.h dirent.h])

dnl Checks for typedefs, structures, and compiler characteristics.
MeshLink_ATTRIBUTE(__malloc__)
MeshLink_ATTRIBUTE(__warn_unused_result__)

AC_CHECK_TYPES([socklen_t, struct addrinfo, struct sockaddr_in6], , ,
  [#include "$srcdir/src/have.h"]
)

AC_CHECK_TYPES([struct sockaddr_storage], ,AC_MSG_ERROR([System must support struct sockaddr_storage.]),
  [#include "$srcdir/src/have.h"]
)

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([asprintf fchmod fork gettimeofday random select strdup usleep],
  [], [], [#include "$srcdir/src/have.h"]
)

dnl Support for SunOS

AC_CHECK_FUNC(socket, [], [
  AC_CHECK_LIB(socket, connect)
])
AC_CHECK_FUNC(gethostbyname, [], [
  AC_CHECK_LIB(nsl, gethostbyname)
])

AC_CHECK_DECLS([freeaddrinfo, gai_strerror, getaddrinfo, getnameinfo],
  [], [], [#include "$srcdir/src/have.h"]
)

AC_CACHE_SAVE

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile test/Makefile examples/Makefile])

AC_OUTPUT
