
NULL =

noinst_LTLIBRARIES = libgiseulas.la

pkgdatadir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	-DPKGDATADIR="\"$(pkgdatadir)\""

libgiseulas_la_SOURCES =			\
	gis-eula-pages.c gis-eula-pages.h 	\
	utils.c utils.h 			\
	$(NULL)

libgiseulas_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgiseulas_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiseulas_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
