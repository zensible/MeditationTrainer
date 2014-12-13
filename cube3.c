
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

//http://timmurphy.org/2010/05/04/pthreads-in-c-a-minimal-working-example/


/* this function is run by the second thread */
void *inc_x(void *thrdata_void_ptr)
{

  /* increment x to 100 */
  THRDATA *thrdata_ptr = thrdata_void_ptr;

  while(1) {
    thrdata_ptr->avg += 1.1;
    sleep(1);

    printf("avg increment finished: %f\n", thrdata_ptr->avg);
  }

  /* the function must return something - NULL will do */
  return NULL;
}

int main()
{

  THRDATA thrdata;
  thrdata.avg = 0.05;

  int x = 0, y = 0;

  /* show the initial values of x and y */
  printf("x: %d, y: %d\n", x, y);

  /* this variable is our reference to the second thread */
  pthread_t inc_x_thread;

  /* create a second thread which executes inc_x(&x) */
  if (pthread_create(&inc_x_thread, NULL, inc_x, &thrdata)) {

    fprintf(stderr, "Error creating thread\n");
    return 1;

  }

  while(1) {
    printf("thrdata %f\n", thrdata.avg);
  }

  /* wait for the second thread to finish */
  if (pthread_join(inc_x_thread, NULL)) {
    fprintf(stderr, "Error joining thread\n");
    return 2;
  }

  /* show the results - x is now 100 thanks to the second thread */
  printf("x: %d, y: %d\n", x, y);

  return 0;

}