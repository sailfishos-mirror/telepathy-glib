# Example connection manager with chatrooms where contacts have
# channel-specific handles.

EXAMPLES = telepathy-example-cm-csh
noinst_LTLIBRARIES = libexample-cm-csh.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_csh_la_SOURCES = \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    protocol.c \
    protocol.h \
    room.c \
    room.h \
    room-manager.c \
    room-manager.h

libexample_cm_csh_la_LIBADD = $(LDADD)

telepathy_example_cm_csh_SOURCES = \
    main.c

telepathy_example_cm_csh_LDADD = \
    $(noinst_LTLIBRARIES)

servicedir = ${datadir}/dbus-1/services

if INSTALL_EXAMPLES
service_DATA = _gen/im.telepathy.v1.ConnectionManager.example_csh.service
$(service_DATA): %: Makefile
	$(MKDIR_P) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=im.telepathy.v1.ConnectionManager.example_csh" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-csh"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_csh.manager
endif

distclean-local:
	rm -rf _gen

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path, and put it last; we use
# a different format here because we're part of the telepathy-glib source tree.
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-core.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(NULL)

# Similarly, in an external project you'd put $(TP_GLIB_CFLAGS) at the end of
# AM_CPPFLAGS.
AM_CPPFLAGS = \
    -I${top_srcdir} -I${top_builddir} \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
