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

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>
#include "esUtil.h"

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

static struct OutputBuffer ob;
struct InputBuffer ib;
char lineBuf[MAXLEN];
int linePos = 0;

struct Options opts;

// eeg client-related prototypes
void idleHandler(void);
int isANumber(const char *str);
void serverDied(void);

// Display-related prototypes
GLuint LoadShader ( GLenum type, const char *shaderSrc );
GLuint LoadShaderDisk ( GLenum type, const GLchar *shaderSrc );

int Init ( ESContext *esContext );
void Draw ( ESContext *esContext );

int main(int argc, char **argv)
{

  // Init eeg client
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


  ESContext esContext;
  UserData  userData;

  esInitContext ( &esContext );
  esContext.userData = &userData;

  esCreateWindow ( &esContext, "Hello Triangle", 256, 256, ES_WINDOW_RGB );

  if ( !Init ( &esContext ) )
     return 0;

  esRegisterDrawFunc ( &esContext, Draw );

  //esMainLoop ( &esContext );

  struct timeval t1, t2;
  struct timezone tz;
  float deltatime;
  float totaltime = 0.0f;
  unsigned int frames = 0;

  gettimeofday ( &t1 , &tz );

  while (userInterrupt(&esContext) == GL_FALSE)
  {
    idleHandler();
    idleHandler();
    idleHandler();
    idleHandler();
    idleHandler();
    idleHandler();
    idleHandler();

    gettimeofday(&t2, &tz);
    deltatime = (float)(t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6);
    t1 = t2;

    if (esContext.updateFunc != NULL)
      esContext.updateFunc(&esContext, deltatime);
    if (esContext.drawFunc != NULL)
      esContext.drawFunc(&esContext);

    eglSwapBuffers(esContext.eglDisplay, esContext.eglSurface);

    totaltime += deltatime;
    frames++;
    if (totaltime >  2.0f)
    {
      printf("%4d frames rendered in %1.4f seconds -> FPS=%3.4f\n", frames, totaltime, frames/totaltime);
      totaltime -= 2.0f;
      frames = 0;
    }
  }

  return 0;
}

void serverDied(void)
{
  rprintf("Server died!\n");
  exit(1);
}

/*
 * eeg-client-related functions
 */
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

/*
 * Drawing-related functions
 */
GLuint LoadTexture ( char *fileName )
{
   int width,
       height;
   char *buffer = esLoadTGA ( fileName, &width, &height );
   GLuint texId;

   if ( buffer == NULL )
   {
      esLogMessage ( "Error loading (%s) image.\n", fileName );
      return 0;
   }

   glGenTextures ( 1, &texId );
   glBindTexture ( GL_TEXTURE_2D, texId );

   glTexImage2D ( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );  // GL_CLAMP_TO_EDGE
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );  // GL_CLAMP_TO_EDGE

   free ( buffer );

   return texId;
}


char* file_read(const char* filename)
{
  FILE* input = fopen(filename, "rb");
  if(input == NULL) return NULL;
 
  if(fseek(input, 0, SEEK_END) == -1) return NULL;
  long size = ftell(input);
  if(size == -1) return NULL;
  if(fseek(input, 0, SEEK_SET) == -1) return NULL;
 
  /*if using c-compiler: dont cast malloc's return value*/
  char *content = (char*) malloc( (size_t) size +1  ); 
  if(content == NULL) return NULL;
 
  fread(content, 1, (size_t)size, input);
  if(ferror(input)) {
    free(content);
    return NULL;
  }
 
  fclose(input);
  content[size] = '\0';
  return content;
}


///
// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader ( GLenum type, const char *shaderSrc )
{
   GLuint shader;
   GLint compiled;
   
   // Create the shader object
   shader = glCreateShader ( type );

   if ( shader == 0 )
    return 0;

   // Load the shader source
   glShaderSource ( shader, 1, &shaderSrc, NULL );
   
   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if ( !compiled ) 
   {
      GLint infoLen = 0;

      glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
         esLogMessage ( "Error compiling shader:\n%s\n", infoLog );            
         
         free ( infoLog );
      }

      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}

GLuint LoadShaderDisk ( GLenum type, const char *filename )
{

  const GLchar* source = file_read(filename);
  if (source == NULL) {
    fprintf(stderr, "Error opening %s: ", filename); perror("");
    return 0;
  }

  printf("Source:\n\n%s", source);

  const GLchar* sources[2] = {
    "#version 100\n"
    "#define GLES2\n",
    source };

  GLuint shader;
  GLint compiled;
   
  // Create the shader object
  shader = glCreateShader ( type );

   if ( shader == 0 )
    return 0;

  glShaderSource(shader, 2, sources, NULL);
  free((void*)source);

   // Load the shader source
   //glShaderSource ( shader, 1, &shaderSrc, NULL );
   //glShaderSource ( shader, 1, &array, NULL );
   //glShaderSource ( shader, 1, (const GLchar**)&Src, 0 );
   
   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if ( !compiled ) 
   {
      GLint infoLen = 0;

      glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
         esLogMessage ( "Error compiling shader:\n%s\n", infoLog );            
         
         free ( infoLog );
      }

      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}



///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
   esContext->userData = malloc(sizeof(UserData));

   UserData *userData = esContext->userData;
   GLbyte vShaderStr[] =  
      "attribute vec4 vPosition;    \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = vPosition;  \n"
      "}                            \n";

  const char* filename;
  filename = "/home/ubuntu/openeeg/opengl_c/LinuxX11/Chapter_2/Hello_Triangle/oscope.glsl";

  GLuint vertexShader;
  GLuint fragmentShader;
  GLuint programObject;
  GLint linked;

  // Load the vertex/fragment shaders
  vertexShader = LoadShader ( GL_VERTEX_SHADER, vShaderStr );
  fragmentShader = LoadShaderDisk ( GL_FRAGMENT_SHADER, filename );

  // Create the program object
  programObject = glCreateProgram ( );
   
  if ( programObject == 0 )
     return 0;

  glAttachShader ( programObject, vertexShader );
  glAttachShader ( programObject, fragmentShader );

  // Bind vPosition to attribute 0   
  glBindAttribLocation ( programObject, 0, "vPosition" );

  // Link the program
  glLinkProgram ( programObject );

  // Check the link status
  glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );

  srand(time(NULL));

  if ( !linked ) 
  {
     GLint infoLen = 0;

     glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );
      
     if ( infoLen > 1 )
     {
        char* infoLog = malloc (sizeof(char) * infoLen );
        glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
        esLogMessage ( "Error linking program:\n%s\n", infoLog );            
        
        free ( infoLog );
     }

     glDeleteProgram ( programObject );
     return GL_FALSE;
  }

  // Store the program object
  userData->programObject = programObject;

  // Get the uniform locations
  userData->locGlobalTime = glGetUniformLocation( userData->programObject, "iGlobalTime" );
  userData->locIChannel0 = glGetUniformLocation( userData->programObject, "iChannel0" );
  userData->locYOffset = glGetUniformLocation( userData->programObject, "yOffset" );

  gettimeofday(&userData->timeStart, NULL);

  //userData->textureId = load_texture_TGA( "/home/ubuntu/openeeg/opengl_c/LinuxX11/Chapter_10/MultiTexture/tex16.tga", NULL, NULL, GL_REPEAT, GL_REPEAT );
  userData->textureId = LoadTexture("/home/ubuntu/openeeg/opengl_c/LinuxX11/Chapter_2/Hello_Triangle/tex16.tga" );


  if ( userData->textureId == 0 )
    return FALSE;

  glClearColor ( 0.0f, 1.0f, 0.0f, 0.0f );
  return GL_TRUE;
}

///
// Draw a triangle using the shader pair created in Init()
//
void Draw ( ESContext *esContext )
{
  UserData *userData = esContext->userData;
  GLfloat vVertices1[] = { -1.0f, -1.0f, 0.0f, 
                           -1.0f, 1.0f, 0.0f,
                           1.0f, 1.0f, 0.0f,
                           1.0f, 1.0f, 0.0f,
                           1.0f, -1.0f, 0.0f,
                           -1.0f, -1.0f, 0.0f,
                          };

  // Set the viewport
  glViewport ( 0, 0, esContext->width, esContext->height );
   
  // Clear the color buffer
  glClear ( GL_COLOR_BUFFER_BIT );

  // Use the program object
  glUseProgram ( userData->programObject );

  // Load the vertex data
  glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, vVertices1 );
  glEnableVertexAttribArray ( 0 );

  struct timeval timeNow, timeResult;
  gettimeofday(&timeNow, NULL);
  timersub(&timeNow, &userData->timeStart, &timeResult);
  float diffMs = (float)(timeResult.tv_sec + (timeResult.tv_usec / 1000000.0));

  //printf("Time elapsed: %ld.%06ld %f\n", (long int)timeResult.tv_sec, (long int)timeResult.tv_usec, diffMs);

   // Bind the texture
  glActiveTexture ( GL_TEXTURE0 );
  glBindTexture ( GL_TEXTURE_2D, userData->textureId );

  // Load the MVP matrix
  glUniform1f( userData->locGlobalTime, diffMs );

  // Set the sampler texture unit to 0
  glUniform1i ( userData->locIChannel0, 0 );

  // Set the sampler texture unit to 0
  glUniform1f ( userData->locYOffset, 0.5 );

  //printf("Diffms: %f", diffMs);

  glDrawArrays ( GL_TRIANGLE_STRIP, 0, 18 );
}
