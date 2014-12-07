#ifndef CUBE_H
#define CUBE_H
#include "esUtil.h"

#define MINLINELENGTH 4
#define DELIMS " \r\n"
#define SAMPLESIZE 256

// eeg client-related prototypes
void idleHandler(void);
int isANumber(const char *str);
void serverDied(void);


// Display-related prototypes
GLuint LoadShader ( GLenum type, const char *shaderSrc );
GLuint LoadShaderDisk ( GLenum type, const GLchar *shaderSrc );

int Init ( ESContext *esContext );
void Draw ( ESContext *esContext );
char* file_read(const char* filename);
void kill_child(int sig);

struct Options {
    char hostname[MAXLEN];
    unsigned short port;
    char filename[MAXLEN];
    int eegNum;
    int isFilenameSet;
    int isLimittedTime;
    double seconds;
};

typedef struct
{
  // Handle to a program object
  GLuint programObjectCalib;
  GLuint programShader;

  GLint locCalibGlobalTime;
  GLint locCalibIChannel0;
  GLint locCalibYOffset;
  GLint locCalibIResolution;

  GLint locShaderGlobalTime;
  GLint locShaderIChannel0;
  GLint locShaderYOffset;
  GLint locShaderIResolution;

  struct timeval timeStart;

   // Texture handle
   GLuint textureId;

} UserData;

#define NUM_MODES 2

#ifdef RPI_NO_X

#define SCREENWID 1920
#define SCREENHEI 1200

#else

#define SCREENWID 640
#define SCREENHEI 360

#endif


#endif // CUBE_H