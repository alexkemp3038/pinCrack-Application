/*
  A set of general-purpose, auxiliary functions, to be used in cw2.c file of the application.
*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#include "cw2-aux.h"

/* ------------------------------------------------------- */
/* aux fcts for timers and signals */

/*
  Delay, i.e. do nothing, for @howlong@ micro-seconds. Using either hardware clock or system call.
 */
void delay(unsigned long howLong) {
      // static uint64_t startT, stopT, diffT;  // used for debugging only
      struct timespec sleeper, dummy ;

      sleeper.tv_sec  = (time_t)(howLong / 1000000L) ;
      sleeper.tv_nsec = (long)(howLong % 1000000L) * 1000L ;

#if defined(DEBUG)  && defined(VERBOSE) 
      fprintf(stderr, "delaying by %u s and %u ns (input: %lu us) ...\n", sleeper.tv_sec, sleeper.tv_nsec, howLong);
#endif

#if defined(DEBUG) && defined(VERBOSE) 
      startT = timeInMicroseconds();
      nanosleep (&sleeper, &dummy) ;
      stopT = timeInMicroseconds();
      diffT= stopT - startT;
      fprintf(stderr, "  measured delay: %f s (%d us) ...\n", ((double)diffT) / 1000000.0, diffT);
#else
      nanosleep (&sleeper, &dummy) ;
#endif
}

/* --------------------------------------------------------------------------- */
/* Miscellaneous aux fcts */

/*
 * delayMicroseconds:
 *	This is somewhat intersting. It seems that on the Pi, a single call
 *	to nanosleep takes some 80 to 130 microseconds anyway, so while
 *	obeying the standards (may take longer), it's not always what we
 *	want!
 *
 *	So what I'll do now is if the delay is less than 100uS we'll do it
 *	in a hard loop, watching a built-in counter on the ARM chip. This is
 *	somewhat sub-optimal in that it uses 100% CPU, something not an issue
 *	in a microcontroller, but under a multi-tasking, multi-user OS, it's
 *	wastefull, however we've no real choice )-:
 *
 *      Plan B: It seems all might not be well with that plan, so changing it
 *      to use gettimeofday () and poll on that instead...
 *********************************************************************************
 */


void delayMicroseconds (unsigned int howLong)
{
  struct timespec sleeper ;
  unsigned int uSecs = howLong % 1000000 ;
  unsigned int wSecs = howLong / 1000000 ;

  /**/ if (howLong ==   0)
    return ;
#if 0
  else if (howLong  < 100)
    delayMicrosecondsHard (howLong) ;
#endif
  else
  {
    sleeper.tv_sec  = wSecs ;
    sleeper.tv_nsec = (long)(uSecs * 1000L) ;
    nanosleep (&sleeper, NULL) ;
  }
}

/*
  waitForEnter: wait for a press of the enter key from the keyboard; useful for testing.
*/

void waitForEnter (void)
{
  printf ("Press ENTER to continue: ") ;
  (void)fgetc (stdin) ;
}

/*
  failure: terminate execution with an error message; using var-args list; useful for error handling.
*/

int failure (bool fatal, const char *message, ...)
{
  va_list argp ;
  char buffer [1024] ;

  if (!fatal)
    return -1 ;

  va_start (argp, message) ;
  vsnprintf (buffer, 1024, message, argp) ;
  va_end (argp) ;

  fprintf (stderr, "%s", buffer) ;
  exit (EXIT_FAILURE) ;

  return 0 ;
}

