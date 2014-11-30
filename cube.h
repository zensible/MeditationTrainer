#ifndef CUBE_H
#define CUBE_H
#include "esUtil.h"

#define MINLINELENGTH 4
#define DELIMS " \r\n"
#define SAMPLESIZE 768

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
  GLuint programObject;
  GLint locGlobalTime;
  GLint locIChannel0;
  GLfloat locYOffset;
  struct timeval timeStart;

   // Texture handle
   GLuint textureId;

} UserData;


#endif // CUBE_H