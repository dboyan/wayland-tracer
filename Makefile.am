ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

aclocaldir = $(datadir)/aclocal

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

bin_PROGRAMS = wayland-tracer

wayland_tracer = $(top_builddir)/wayland-tracer
wayland_tracer_SOURCES =		\
	src/connection.c		\
	src/frontend-analyze.c		\
	src/frontend-analyze.h		\
	src/frontend-bin.c		\
	src/frontend-bin.h		\
	src/tracer.c			\
	src/tracer.h			\
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

man_MANS = wayland-tracer.1

MAN_SUBSTS = 						\
	-e 's|__version__|$(PACKAGE_VERSION)|g'

SUFFIXES = .1 .man

%.1 : man/%.man
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

EXTRA_DIST =				\
	man/wayland-tracer.man

CLEANFILES = $(man_MANS)
