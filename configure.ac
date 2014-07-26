AC_PREREQ([2.64])

m4_define([tracer_major_version], [0])
m4_define([tracer_minor_version], [0])
m4_define([tracer_micro_version], [90])
m4_define([tracer_version],
	  [tracer_major_version.tracer_minor_version.tracer_micro_version])

AC_INIT([wayland-tracer],
	[tracer_version],
	[],
	[wayland-tracer],
	[https://github.com/dboyan/wayland-tracer/])

AC_SUBST([TRACER_VERSION_MAJOR], [tracer_major_version])
AC_SUBST([TRACER_VERSION_MINOR], [tracer_minor_version])
AC_SUBST([TRACER_VERSION_MICRO], [tracer_micro_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz subdir-objects])

AM_SILENT_RULES([yes])

AC_PROG_CC

LT_PREREQ([2.2])
LT_INIT

PKG_PROG_PKG_CONFIG()

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -Wextra -Wno-used-parameter -g -Wscrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_CHECK_FUNCS([accept4])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
