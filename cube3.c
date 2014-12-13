
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

#include <fftw3.h>
#include <pthread.h>

#include <pthread.h>
#include <stdio.h>

#include "cube3.h"
#include "lib/eeg.h"

//http://timmurphy.org/2010/05/04/pthreads-in-c-a-minimal-working-example/
//https://computing.llnl.gov/tutorials/pthreads/

pthread_mutex_t lock_threaddata;

void *poll_eeg_thread(void *thrdata_void_ptr);
void get_line_edf();
void handleSample(int channel, int val);
int isANumber(const char *str);
void initNsdClient();

struct Options opts;

int main()
{

  /*
   * Inialized struct used to share data between main and monitoreeg thread
   */
  THRDATA thrdata;

  thrdata.avg = 0.05;

  // Fill eeg sample data with 0s to prevent buffer overruns
  int i;
  for (i = 0; i < SAMPLESIZE; i++) {
    thrdata.sampleBuf[0][i] = 0;
    thrdata.sampleBuf[1][i] = 0;
  }

  initNsdClient();

  // Start thread to poll EEG:

  /* this variable is our reference to the second thread */
  pthread_t poll_eeg;
  pthread_mutex_init(&lock_threaddata, NULL);

  /* create a second thread which executes inc_x(&x) */
  if (pthread_create(&poll_eeg, NULL, poll_eeg_thread, &thrdata)) {

    fprintf(stderr, "Error creating thread\n");
    return 1;

  }

  while(1) {
    rprintf("yay");
    rprintf("thrdata %f\n", thrdata.avg);
    sleep(1);
  }

  /* wait for the second thread to finish */
  if (pthread_join(poll_eeg, NULL)) {
    fprintf(stderr, "Error joining thread\n");
    return 2;
  }

  pthread_mutex_destroy(&lock_threaddata);
  pthread_exit(NULL);

  return 0;

}

sock_t sock_fd;
char EDFPacket[MAXHEADERLEN];
GIOChannel *neuroserver;
static int readSamples = 0;

static struct OutputBuffer ob;
struct InputBuffer ib;
char lineBuf[MAXLEN];
int linePos = 0;

void initNsdClient() {
  // Init eeg client
  char cmdbuf[80];
  int EDFLen = MAXHEADERLEN;
  struct EDFDecodedConfig cfg;
  double t0;
  int retval;
  strcpy(opts.hostname, DEFAULTHOST);
  opts.port = DEFAULTPORT;
  opts.eegNum = 0;
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
  if (isEOF(sock_fd, &ib)) {
    rprintf("Server died!\n");
    exit(1);
  }

  EDFLen = readline(sock_fd, EDFPacket, sizeof(EDFPacket), &ib);
//  rprintf("Got EDF Header <%s>\n", EDFPacket);
  readEDFString(&cfg, EDFPacket, EDFLen);

  sprintf(cmdbuf, "watch %d\n", opts.eegNum);
  writeString(sock_fd, cmdbuf, &ob);
  getOK(sock_fd, &ib);

  rprintf("Watching for EEG data.\n");
}

/* this function is run by the second thread */
void *poll_eeg_thread(void *thrdata_void_ptr)
{

  THRDATA *thrdata_ptr = thrdata_void_ptr;

  printf("start");
  int counter = 0;
  while(1) {
    counter++;

    int i;
    char *cur;
    int vals[MAXCHANNELS + 5];
    int curParam = 0;
    int devNum, packetCounter, channels, *samples;

    linePos = readline(sock_fd, lineBuf, sizeof(EDFPacket), &ib);
    //rprintf("Got line retval=<%d>, <%s>\n", linePos, lineBuf);

    if (isEOF(sock_fd, &ib)) {
      printf("Got EOF\n");
      return NULL;
    }

    if (linePos < MINLINELENGTH) {
      printf(" < MINLINE\n");
      continue;
    }

    if (lineBuf[0] != '!') {
      printf("!!\n");
      continue;
    }

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
        //rprintf("Sample #%d: %d\n", i, samples[i]);
      }
    }

    int channel;
    for (channel = 0; channel < 2; ++channel) {
      int val = samples[channel];
      //handleSample(i, samples[i], thrdata_ptr);

      static int updateCounter = 0;
      assert(channel == 0 || channel == 1);
      if (!(val >= 0 && val < 1024)) {
        printf("Got bad value: %d -> ", val);
        /*
         * Instead use previous value, so the graph doesn't jump around
         */ 
        if (readSamples > 0) {
          val = thrdata_ptr->sampleBuf[channel][readSamples-1];
        }
        printf("%d\n", val);
      }

      /*
       * Fill buffer from left to right until you reach the end.
       *
       * Once there, instead keep shuffling sample values left by one and filling in the last slot (SAMPLESIZE-1) 
       */

      pthread_mutex_lock (&lock_threaddata);
      if (readSamples == SAMPLESIZE-1) {
        memmove(&thrdata_ptr->sampleBuf[channel][0], &thrdata_ptr->sampleBuf[channel][1],  sizeof(int)*(SAMPLESIZE-1));
      }

      thrdata_ptr->sampleBuf[channel][readSamples] = val;
      pthread_mutex_unlock (&lock_threaddata);

      if (readSamples < SAMPLESIZE-1 && channel == 1)
        readSamples += 1;

      if (counter % 50 == 0) {
        counter = 1;

        pthread_mutex_lock (&lock_threaddata);
        thrdata_ptr->avg += 1.1;
        pthread_mutex_unlock (&lock_threaddata);

        rprintf("avg increment finished: %f\n", thrdata_ptr->avg);
      }
    }


  }

  /* the function must return something - NULL will do */
  return NULL;
}

int isANumber(const char *str) {
  int i;
  for (i = 0; str[i]; ++i)
    if (!isdigit(str[i]))
      return 0;
  return 1;
}
