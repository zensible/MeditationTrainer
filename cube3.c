
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
#include "esUtil.h"
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


void Draw ( ESContext *esContext );
int Init ( ESContext *esContext );
void OnKey ( ESContext *esContext, unsigned char key, int x, int y);
GLuint getProgram(int mode, char *vertex_shader, char *fragment_shader);
char* file_read(const char* filename);

int mode = 0;
THRDATA thrdata;

int main()
{

  /*
   * Inialized struct used to share data between main and monitoreeg thread
   */

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





  // We're connected to nsd. Now initiate graphics
  ESContext esContext;
  UserData  userData;

  esInitContext ( &esContext );
  esContext.userData = &userData;

  esCreateWindow ( &esContext, "Meditation Cube", SCREENWID, SCREENHEI, ES_WINDOW_RGB );

  rprintf("About to init\n");

  if ( !Init ( &esContext ) )
     return 0;

  esRegisterDrawFunc ( &esContext, Draw );
  esRegisterKeyFunc( &esContext, OnKey );

  struct timeval t1, t2;
  struct timezone tz;
  float deltatime;
  float totaltime = 0.0f;
  unsigned int frames = 0;

  gettimeofday ( &t1 , &tz );

  // Child process 1: show visualization
  while (userInterrupt(&esContext) == GL_FALSE)
  {
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
      printf("==== FPS=%3.4f %4d frames rendered in %1.4f seconds\n", frames/totaltime, frames, totaltime);
      totaltime -= 2.0f;
      frames = 0;
    }
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

        int i;
        fftw_complex *in;
        fftw_complex *in2;
        fftw_complex *out;
        fftw_plan plan_backward;
        fftw_plan plan_forward;

        in = fftw_malloc ( sizeof ( fftw_complex ) * SAMPLESIZE );

        for (i = 0; i < SAMPLESIZE; i++) {
          int val = thrdata_ptr->sampleBuf[0][i];
          in[i][0] = val;
          in[i][1] = 0;
        }

        out = fftw_malloc ( sizeof ( fftw_complex ) * SAMPLESIZE );
        plan_forward = fftw_plan_dft_1d ( SAMPLESIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE );
        fftw_execute ( plan_forward );

        int num_freqs = 0;
        float avg = 0;
        for ( i = 0; i < SAMPLESIZE; i++ )
        {
          int hz;
          int val;
          val = abs(out[i][0]) ^ 2;
          hz = ((i * SAMPLESIZE) / 100);
          if (hz >= 20 && hz <= 48) {  // beta waves and low gamma waves
            avg += val;
            num_freqs++;
          }
        }
        avg = avg / num_freqs;
        printf("== AVG %f", avg);

        pthread_mutex_lock (&lock_threaddata);
        thrdata_ptr->avg = avg;
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









void OnKey ( ESContext *esContext, unsigned char key, int x, int y)
{
  rprintf("key %d", key);
  switch ( key )
  {
    case 015:  // enter key
      mode += 1;
      if (mode >= NUM_MODES) {
        mode = 0;
      }
      rprintf( "\nMODE: %d\n", mode );
      break;
    case 'm':
      rprintf( "Saw an 'm'\n" );
      break;
    case 'a':
      mode += 1;
      if (mode >= NUM_MODES) {
        mode = 0;
      }
      rprintf( "\nMODE: %d\n", mode );
      break;
    case '1':
      rprintf( "Saw a '1'\n" );
      break;
    case 033: // ASCII Escape Key
      exit( 0 );
      break;
  }
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

GLuint LoadShaderDisk ( GLenum type, const char *filename )
{

  const GLchar* source = file_read(filename);
  if (source == NULL) {
    fprintf(stderr, "Error opening %s: ", filename); perror("");
    return 0;
  }

  //printf("Source:\n\n%s", source);

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

GLuint getProgram(int mode, char *vertex_shader, char *fragment_shader) {
  GLuint vertexShader;
  GLuint fragmentShader;
  GLint linked;

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL)
    perror("getcwd() error");

  char path_vertex[1024];
  strcpy(path_vertex, cwd);
  strcat(path_vertex, vertex_shader);

  char path_fragment[1024];
  strcpy(path_fragment, cwd);
  strcat(path_fragment, fragment_shader);

  // Load the vertex/fragment shaders
  vertexShader = LoadShaderDisk ( GL_VERTEX_SHADER, path_vertex );
  fragmentShader = LoadShaderDisk ( GL_FRAGMENT_SHADER, path_fragment );

  GLuint programObject;

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

  return programObject;
}

///
// Initialize the shader and program object
//
int Init ( ESContext *esContext )
{
  rprintf("INIT.\n");
  srand(time(NULL));

  esContext->userData = malloc(sizeof(UserData));

  UserData *userData = esContext->userData;

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL)
    perror("getcwd() error");

  // Store the program object
  userData->programs[0] = getProgram(0, "/vertex.glsl", "/calibrate.glsl");
  userData->programs[1] = getProgram(1, "/vertex.glsl", "/waves.glsl");
  userData->programs[2] = getProgram(2, "/vertex.glsl", "/fire.glsl");
  userData->programs[3] = getProgram(3, "/vertex.glsl", "/oscope.glsl");
  userData->programs[4] = getProgram(4, "/vertex.glsl", "/fiery_spiral.glsl");
  //userData->programs[5] = getProgram(5, "/vertex.glsl", "/torusjourney.glsl");
  //userData->programs[6] = getProgram(6, "/vertex.glsl", "/galaxy.glsl");

  int i;
  for (i = 0; i < NUM_MODES; i++) {
    // Get the uniform locations
    userData->locGlobalTime[i] = glGetUniformLocation( userData->programs[i], "iGlobalTime" );
    userData->locIChannel0[i] = glGetUniformLocation( userData->programs[i], "iChannel0" );
    userData->locYOffset[i] = glGetUniformLocation( userData->programs[i], "yOffset" );
    userData->locIResolution[i] = glGetUniformLocation( userData->programs[i], "iResolution");
  }

  gettimeofday(&userData->timeStart, NULL);

  //userData->textureId = load_texture_TGA( "/home/ubuntu/openeeg/opengl_c/LinuxX11/Chapter_10/MultiTexture/tex16.tga", NULL, NULL, GL_REPEAT, GL_REPEAT );

  char path_tex16[1024];
  strcpy(path_tex16, cwd);
  strcat(path_tex16, "/tex16.tga");

  userData->textureId = LoadTexture( path_tex16 );


  if ( userData->textureId == 0 )
    return FALSE;

  glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
  return GL_TRUE;
}


///
// Draw a triangle using the shader pair created in Init()
//
void Draw ( ESContext *esContext )
{
  UserData *userData = esContext->userData;

  // Set the viewport
  glViewport ( 0, 0, esContext->width, esContext->height );
   
  // Clear the color buffer
  glClear ( GL_COLOR_BUFFER_BIT );

  if (mode == 0) {
    // Use the program object
    glUseProgram ( userData->programs[0] );

    GLfloat buffer[256*2];
    int i;
    int r;
    for (i = 0; i < SAMPLESIZE; i++) {
      if (i % 2 == 0) {
        // X-coord
        buffer[i] = i * 0.0078125 - 1;
      } else {
        // Y-coord
        if (thrdata.sampleBuf[0][i] > 1024) {
          thrdata.sampleBuf[0][i] = 1024;
        }
        /*
        if (i > 0 && abs(threaddata.sampleBuf[0][i] - threaddata.sampleBuf[0][i - i]) > 300) {
          threaddata.sampleBuf[0][i] = threaddata.sampleBuf[0][i - i];
        }
        */
        GLfloat val = ((float) thrdata.sampleBuf[0][i]) * 0.001953125 - 1;
        buffer[i] = val; // * CALIB_X_SCALE;
      }
      //printf(" #%d: %f\t", i, buffer[i]);
      //printf("\n");
    }

     // Load the vertex data
     glVertexAttribPointer ( 0, 2, GL_FLOAT, GL_FALSE, 0, buffer );
     glEnableVertexAttribArray ( 0 );
     glLineWidth(2);
     glDrawArrays ( GL_LINE_STRIP, 0, 128 );    
  } else {
    glUseProgram ( userData->programs[mode] );
    // Completely cover the screen
    GLfloat vVertices1[] = { -1.0f, -1.0f, 0.0f, 
                             -1.0f, 1.0f, 0.0f,
                             1.0f, 1.0f, 0.0f,
                             1.0f, 1.0f, 0.0f,
                             1.0f, -1.0f, 0.0f,
                             -1.0f, -1.0f, 0.0f,
                            };

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
    glUniform1f( userData->locGlobalTime[mode], diffMs );

    // Set the sampler texture unit to 0
    glUniform1i ( userData->locIChannel0[mode], 0 );

    float avg = thrdata.avg;

    switch(mode) {
    case 1:  // waves    0.005 - .03
      //avg = avg * (0.05/1024.0) + 0.03;
      avg = avg * 0.00004882812 + 0.03;
      break;
    case 2:  // fire  0.005 - .5
      avg = avg * 0.000390625 + 0.005;
      break;
    case 3:  // oscope  0.005 - .01
      avg = avg * 0.00004882812 + 0.01;
      break;
    case 4:  // fiery_spiral   .01 - .05
      avg = (avg * 0.00000976562 + 0.01) * 1.25;
      break;
    }

    //avg = (rand() % 10 + 1) / 10.0;
    printf("\navg %f", thrdata.avg);

    // Set the sampler texture unit to 0
    glUniform1f ( userData->locYOffset[mode], avg );

    glUniform2f( userData->locIResolution[mode], SCREENWID, SCREENHEI );

    glDrawArrays ( GL_TRIANGLE_STRIP, 0, 18 );    
  }

}



