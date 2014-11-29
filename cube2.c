/*
NeuroServer

A collection of programs to translate between EEG data and TCP network
messages. This is a part of the OpenEEG project, see http://openeeg.sf.net
for details.
            
Copyright (C) 2003, 2004 Rudi Cilibrasi (cilibrar@ofb.net)
                
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
                                
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
                                                
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/                                                            

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <openedf.h>
#include <sys/time.h>
#include <pctimer.h>
#include <ctype.h>
#include <nsutil.h>
#include <nsnet.h>
#include <glib.h>
#include "monitor.h"

sock_t sock_fd;
char EDFPacket[MAXHEADERLEN];
GIOChannel *neuroserver;
const char *helpText =
"sampleClient   v 0.34 by Rudi Cilibrasi\n"
"\n"
"Usage:  sampleClient [options]\n"
"\n"
"        -p port          Port number to use (default 8336)\n"
"        -n hostname      Host name of the NeuroCaster server to connect to\n"
"                         (this defaults to 'localhost')\n"
"        -e <intnum>      Integer ID specifying which EEG to log (default: 0)\n"
"The filename specifies the new EDF file to create.\n"
;

#define MINLINELENGTH 4
#define DELIMS " \r\n"


#define VIEWWIDTH 768
#define VIEWHEIGHT 128
#define TOPMARGIN 80
#define MARGIN 160

#define DRAWINGAREAHEIGHT (TOPMARGIN + VIEWHEIGHT*2 + MARGIN)

#define UPDATEINTERVAL 64

static int sampleBuf[2][VIEWWIDTH];
static int readSamples = 0;

struct Options {
    char hostname[MAXLEN];
    unsigned short port;
    char filename[MAXLEN];
    int eegNum;
    int isFilenameSet;
    int isLimittedTime;
    double seconds;
};

static struct OutputBuffer ob;
struct InputBuffer ib;
char lineBuf[MAXLEN];
int linePos = 0;

struct Options opts;

void idleHandler(void);
int isANumber(const char *str);
void serverDied(void);

int main(int argc, char **argv)
{
  char cmdbuf[80];
  int EDFLen = MAXHEADERLEN;
  struct EDFDecodedConfig cfg;
  int i;
  double t0;
  int retval;
  strcpy(opts.hostname, DEFAULTHOST);
  opts.port = DEFAULTPORT;
  opts.eegNum = 0;
  for (i = 1; i < argc; ++i) {
    char *opt = argv[i];
    if (opt[0] == '-') {
      switch (opt[1]) {
        case 'h':
          printf("%s", helpText);
          exit(0);
          break;
        case 'e':
          opts.eegNum = atoi(argv[i+1]);
          i += 1;
          break;
        case 'n':
          strcpy(opts.hostname, argv[i+1]);
          i += 1;
          break;
        case 'p':
          opts.port = atoi(argv[i+1]);
          i += 1;
          break;
        }
      }
      else {
          fprintf(stderr, "Error: option %s not allowed", argv[i]);
          exit(1);
      }
  }

  rinitNetworking();

  sock_fd = rsocket();
  if (sock_fd < 0) {
      perror("socket");
      rexit(1);
  }
  rprintf("Got socket.\n");

  retval = rconnectName(sock_fd, opts.hostname, opts.port);
  if (retval != 0) {
      rprintf("connect error\n");
      rexit(1);
  }

  rprintf("Socket connected.\n");

  writeString(sock_fd, "display\n", &ob);
  getOK(sock_fd, &ib);
  rprintf("Finished display, doing getheader %d.\n", opts.eegNum);

  sprintf(cmdbuf, "getheader %d\n", opts.eegNum);
  writeString(sock_fd, cmdbuf, &ob);
  getOK(sock_fd, &ib);
  if (isEOF(sock_fd, &ib))
      serverDied();

  EDFLen = readline(sock_fd, EDFPacket, sizeof(EDFPacket), &ib);
//  rprintf("Got EDF Header <%s>\n", EDFPacket);
  readEDFString(&cfg, EDFPacket, EDFLen);

  sprintf(cmdbuf, "watch %d\n", opts.eegNum);
  writeString(sock_fd, cmdbuf, &ob);
  getOK(sock_fd, &ib);

  t0 = pctimer();
  for (;;) {
      idleHandler();
  }

  return 0;
}

void serverDied(void)
{
  rprintf("Server died!\n");
  exit(1);
}

void handleSample(int channel, int val)
{
    static int updateCounter = 0;
    assert(channel == 0 || channel == 1);
    if (!(val >= 0 && val < 1024)) {
        printf("Got bad value: %d\n", val);
        return;
        //exit(0);
    }
    if (readSamples == VIEWWIDTH-1) {
      memmove(&sampleBuf[channel][0], &sampleBuf[channel][1],  sizeof(int)*(VIEWWIDTH-1));
    }

    sampleBuf[channel][readSamples] = val;
    if (readSamples < VIEWWIDTH-1 && channel == 1)
        readSamples += 1;
    if (updateCounter++ % UPDATEINTERVAL == 0) {
        //gtk_widget_draw(onscreen, NULL);
        printf("draw");
    }
}

void idleHandler(void)
{
    int i;
    char *cur;
    int vals[MAXCHANNELS + 5];
    int curParam = 0;
    int devNum, packetCounter, channels, *samples;

    linePos = readline(sock_fd, lineBuf, sizeof(EDFPacket), &ib);
    rprintf("Got line retval=<%d>, <%s>\n", linePos, lineBuf);

    if (isEOF(sock_fd, &ib))
        exit(0);

    if (linePos < MINLINELENGTH)
        return;

    if (lineBuf[0] != '!')
        return;

    for (cur = strtok(lineBuf, DELIMS); cur ; cur = strtok(NULL, DELIMS)) {
        if (isANumber(cur))
            vals[curParam++] = atoi(cur);
    // <devicenum> <packetcounter> <channels> data0 data1 data2 ...
        if (curParam < 3)
            continue;
        devNum = vals[0];
        packetCounter = vals[1];
        channels = vals[2];
        samples = vals + 3;
        for (i = 0; i < channels; ++i) {
          rprintf("Sample #%d: %d\n", i, samples[i]);
        }
    }
//  rprintf("Got sample with %d channels: %d\n", channels, packetCounter);
        for (i = 0; i < 2; ++i)
            handleSample(i, samples[i]);
}

int isANumber(const char *str) {
  int i;
  for (i = 0; str[i]; ++i)
    if (!isdigit(str[i]))
      return 0;
  return 1;
}

gboolean readHandler(GIOChannel *source, GIOCondition cond, gpointer data)
{
    idleHandler();
    return TRUE;
}

void initGTKSystem(void)
{
  neuroserver = g_io_channel_unix_new(sock_fd);

  g_io_add_watch(neuroserver, G_IO_IN, readHandler, NULL);

}
