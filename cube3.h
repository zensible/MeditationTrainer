
#ifndef CUBE_H
#define CUBE_H

#include <nsutil.h>
#include <nsnet.h>

#define SAMPLESIZE 256


#define MINLINELENGTH 4
#define DELIMS " \r\n"
#define SAMPLESIZE 256


typedef struct 
{
  float     avg; 
  int sampleBuf[2][SAMPLESIZE];
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

#endif // CUBE_H