ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

bin_PROGRAMS = wayland-tracer

wayland_tracer = $(top_builddir)/wayland-tracer
wayland_tracer_SOURCES =		\
	src/connection.c		\
	src/tracer.c			\
	src/wayland-os.c		\
	src/wayland-util.c		\
	src/wayland-util.h		\
	src/wayland-private.h		\
	src/core-protocol.c		\
	src/core-protocol.h

AM_CPPFLAGS =				\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/src
