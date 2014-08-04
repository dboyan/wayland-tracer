ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

bin_PROGRAMS = wayland-tracer

wayland_tracer = $(top_builddir)/wayland-tracer
wayland_tracer_SOURCES =		\
	src/connection.c		\
	src/tracer.c			\
	src/tracer-analyzer.c		\
	src/tracer-analyzer.h		\
	src/wayland-os.c		\
	src/wayland-util.c		\
	src/wayland-util.h		\
	src/wayland-private.h
wayland_tracer_LDADD = $(EXPAT_LIBS) -lrt

AM_CPPFLAGS =				\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/src
