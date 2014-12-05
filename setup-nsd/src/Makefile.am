# nsd is the NeuroServer daemon

AM_CFLAGS= -Wall -g
AM_CPPFLAGS= -I.

bin_PROGRAMS = nsd modeegdriver readedf writeedf

nsd_SOURCES = nsd.c nsutil.c nsnet.c monitor.c monitor.h cmdhandler.c \
openedf.c config.h cmdhandler.h edfmacros.h nsd.h nsnet.h nsutil.h openedf.h pctimer.h

modeegdriver_SOURCES = modeegdriver.c nsser.c nsutil.c openedf.c \
nsnet.c monitor.c monitor.h \
config.h edfmacros.h nsnet.h nsser.h nsutil.h openedf.h

readedf_SOURCES = readedf.c nsutil.c nsnet.c monitor.c monitor.h openedf.c \
pctimer.c config.h edfmacros.h nsnet.h nsutil.h pctimer.h openedf.h

writeedf_SOURCES = writeedf.c nsutil.c nsnet.c monitor.c monitor.h \
pctimer.c openedf.c config.h edfmacros.h openedf.h nsutil.h nsnet.h pctimer.h

