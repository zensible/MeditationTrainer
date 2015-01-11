# 
# Makefile: Meditation Trainer - v1
# 
# Built with help from:  http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html
# 
# 
CC = gcc
CFLAGS  = -g -Wall
MEDITRAINER_X11 = meditrainer_x11

MEDITRAINER_RPI = meditrainer_rpi
INCLUDES = -I. -Isetup-nsd/src
INCLUDES_RPI = -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux

SRCS = meditrainer.c esUtil.c setup-nsd/src/monitor.c setup-nsd/src/nsnet.c setup-nsd/src/nsser.c setup-nsd/src/nsutil.c setup-nsd/src/openedf.c setup-nsd/src/pctimer.c
OBJS = $(SRCS:.c=.o)
LIBS = -lGLESv2 -lEGL -lrfftw -lfftw -lm -lpthread
LIBS_X11 = -lX11
LIBS_RPI = -lbcm_host -L/opt/vc/lib

#gcc -DRPI_NO_X esUtil.c monitor.c nsnet.c nsser.c nsutil.c openedf.c pctimer.c meditrainer.c -o meditrainer -I. `pkg-config --cflags --libs glib-2.0` -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -lGLESv2 -lEGL -lm -lbcm_host -L/opt/vc/lib -lrfftw -lfftw -lpthread

#gcc esUtil.c monitor.c nsnet.c nsser.c nsutil.c openedf.c pctimer.c meditrainer.c -o meditrainer -I. `pkg-config --cflags --libs glib-2.0` -lGLESv2 -lEGL -L/opt/vc/lib -lX11 -lrfftw -lfftw -lm -lpthread

all: $(MEDITRAINER_X11)

setup_nsd:
	-rm nsd
	-rm modeegdriver
	cd setup-nsd && ./configure && make && cd src && cp nsd ../../ && cp modeegdriver ../../ && cd ../../

setup_fftw2:
	cd setup-fftw2 && ./configure && make && sudo make install && cd ../../

$(MEDITRAINER_X11): $(SRCS)
	$(CC) -o $(MEDITRAINER_X11) $(SRCS) $(INCLUDES) -I. `pkg-config --cflags --libs glib-2.0` $(LIBS) $(LIBS_X11)

$(MEDITRAINER_RPI): $(SRCS)
	$(CC) -DRPI_NO_X -o $(MEDITRAINER_RPI) $(SRCS) $(INCLUDES) -I. $(INCLUDES_RPI) `pkg-config --cflags --libs glib-2.0` $(LIBS) $(LIBS_RPI)

clean:
	$(RM) $(MEDITRAINER_X11) $(MEDITRAINER_RPI)
