gcc esUtil.c monitor.c nsnet.c nsser.c nsutil.c openedf.c pctimer.c cube.c -o cube -I. `pkg-config --cflags --libs glib-2.0` -lGLESv2 -lEGL -L/opt/vc/lib -lX11 -lrfftw -lfftw -lm -lpthread
