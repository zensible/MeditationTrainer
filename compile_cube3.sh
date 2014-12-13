gcc esUtil.c monitor.c nsnet.c nsser.c nsutil.c openedf.c pctimer.c cube3.c -o cube3 -I. `pkg-config --cflags --libs glib-2.0` -lGLESv2 -lEGL -lm -L/opt/vc/lib -lX11 -lfftw3 -lpthread

