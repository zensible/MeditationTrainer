gcc esUtil.c cube.c -I. `pkg-config --cflags --libs glib-2.0` -g nsutil.o nsnet.o openedf.o pctimer.o monitor.o -o cube -lGLESv2 -lEGL -lm -lX11 -lfftw3  -lpthread
