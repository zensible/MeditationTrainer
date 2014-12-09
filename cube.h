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
void OnKey ( ESContext *esContext, unsigned char key, int x, int y);

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

#define NUM_MODES 5

typedef struct
{
  // Handle to a program object
  GLuint programs[NUM_MODES];

  GLuint locGlobalTime[NUM_MODES];
  GLuint locIChannel0[NUM_MODES];
  GLuint locYOffset[NUM_MODES];
  GLuint locIResolution[NUM_MODES];

  struct timeval timeStart;

   // Texture handle
   GLuint textureId;

} UserData;


#ifdef RPI_NO_X

#define SCREENWID 1920
#define SCREENHEI 1200

#else

#define SCREENWID 640
#define SCREENHEI 360

#endif


#endif // CUBE_H