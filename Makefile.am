
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_CFLAGS = ${WARN_CFLAGS}

SUBDIRS = egg-list-box data gnome-initial-setup player po

EXTRA_DIST = \
    autogen.sh \
    COPYING \
    AUTHORS \
    NEWS

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
