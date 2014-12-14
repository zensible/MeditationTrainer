
#ifndef CUBE_H
#define CUBE_H

#include <nsutil.h>
#include <nsnet.h>
#include "esUtil.h"


/*
 * Used by the modeedriver
 */
#define SAMPLESIZE 256
#define MINLINELENGTH 4
#define DELIMS " \r\n"


#define X_SIZE 256
#define ADC_RESOLUTION 1024
#define FFT_SCALE_FACTOR (1024*1024*10)

typedef struct 
{
  float     avg; 
  unsigned int sampleBuf[2][SAMPLESIZE];
} THRDATA;

struct Options {
    char hostname[MAXLEN];
    unsigned short port;
    char filename[MAXLEN];
    int eegNum;
    int isFilenameSet;
    int isLimittedTime;
    double seconds;
};

/*
 * Used by opengl es
 */

#ifdef RPI_NO_X

#define SCREENWID 0
#define SCREENHEI 0

#else

#define SCREENWID 640
#define SCREENHEI 360

#endif

#define NUM_MODES 5

typedef struct
{
  // Handle to a program object
  GLuint programs[NUM_MODES];

  GLuint locGlobalTime[NUM_MODES];
  GLuint locIChannel0[NUM_MODES];
  GLuint locYOffset[NUM_MODES];
  GLuint locIResolution[NUM_MODES];

  void*       thrdata_void_ptr;

  struct timeval timeStart;

   // Texture handle
   GLuint textureId;

} UserData;

#endif // CUBE_H