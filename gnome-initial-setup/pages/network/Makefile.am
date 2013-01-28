cappletname = network

AM_CPPFLAGS = \
	      $(INITIAL_SETUP_CFLAGS) \
	      $(NULL)

noinst_LTLIBRARIES = libgisnetwork.la

dbus_built_sources =	manager.c manager.h 	\
			technology.c technology.h \
			service.c service.h
manager.c: manager.h
manager.h: Makefile.am manager-interface.xml
	gdbus-codegen						\
		--interface-prefix net.connman.			\
		--generate-c-code manager 			\
		$(srcdir)/manager-interface.xml

technology.c: technology.h
technology.h: Makefile.am technology-interface.xml
	gdbus-codegen						\
		--interface-prefix net.connman.			\
		--generate-c-code technology 			\
		$(srcdir)/technology-interface.xml

service.c: service.h
service.h: Makefile.am service-interface.xml
	gdbus-codegen						\
		--interface-prefix net.connman.			\
		--generate-c-code service 			\
		$(srcdir)/service-interface.xml

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/network.gresource.xml)
gis-network-resources.c: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source --c-name gis_network $<
gis-network-resources.h: network.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header --c-name gis_network $<

BUILT_SOURCES =			\
	gis-network-resources.c	\
	gis-network-resources.h

libgisnetwork_la_SOURCES =	gis-network-page.c	\
				gis-network-page.h	\
				$(dbus_built_sources)	\
				$(BUILT_SOURCES)

libgisnetwork_la_LIBADD = $(INITIAL_SETUP_LIBS)

libgisnetwork_la_CFLAGS = $(INITIAL_SETUP_LIBS) -I "$(srcdir)/../.."

