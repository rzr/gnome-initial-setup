cappletname = connman

AM_CPPFLAGS = \
	      $(INITIAL_SETUP_CFLAGS) \
	      $(NULL)

noinst_LTLIBRARIES = libgisconnman.la

dbus_built_sources =	manager.c manager.h 	\
			technology.c technology.h \
			service.c service.h
manager.c: manager.h
manager.h: Makefile.am $(srcdir)/manager-interface.xml
		$(AM_V_GEN) gdbus-codegen			\
		--interface-prefix net.connman.			\
		--generate-c-code manager 			\
		$(srcdir)/manager-interface.xml

technology.c: technology.h
technology.h: Makefile.am $(srcdir)/technology-interface.xml
		$(AM_V_GEN) gdbus-codegen			\
		--interface-prefix net.connman.			\
		--generate-c-code technology 			\
		$(srcdir)/technology-interface.xml

service.c: service.h
service.h: Makefile.am $(srcdir)/service-interface.xml
		$(AM_V_GEN) gdbus-codegen			\
		--interface-prefix net.connman.			\
		--generate-c-code service 			\
		$(srcdir)/service-interface.xml

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/network.gresource.xml)
gis-connman-resources.c: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name gis_connman $<
gis-connman-resources.h: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --c-name gis_connman $<

BUILT_SOURCES =			\
	gis-connman-resources.c	\
	gis-connman-resources.h

libgisconnman_la_SOURCES =	gis-connman-page.c	\
				gis-connman-page.h	\
				$(dbus_built_sources)	\
				$(BUILT_SOURCES)


libgisconnman_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisconnman_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisconnman_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

CLEANFILES = $(BUILT_SOURCES) $(dbus_built_sources)

EXTRA_DIST = network.gresource.xml $(resource_files)
