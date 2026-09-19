/* 
 *
 * F28HS CW2
 * pinCrack: button input of a sequence of numbers followed by cracking a secret PIN
 * Uses interval timers for the timeout/delay function

 * Compile:    	      make
 * Run (e.g):         sudo ./cw2 -d -e -s 112
 * Run (unit-test):   sudo ./cw2 -u -s 112 -r 121

 ***********************************************************************
 * The development of this code was heavily based on the wiringPi library by Gordon Henderson.
 * This instance of the code, however, does not depend directly on the wiringPi library any more.
 *
 * wiringPi:
 *	Arduino look-a-like Wiring library for the Raspberry Pi
 *	Copyright (c) 2012-2015 Gordon Henderson
 *	Additional code for pwmSetClock by Chris Hall <chris@kchall.plus.com>
 *
 *	Thanks to code samples from Gert Jan van Loo and the
 *	BCM2835 ARM Peripherals manual, however it's missing
 *	the clock section /grr/mutter/
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as
 *    published by the Free Software Foundation, either version 3 of the
 *    License, or (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with wiringPi.
 *    If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */

/* --------------------------------------------------------------------------- */
/* Config settings */

// NOTE: most config settings are in cw2-config.h

/* --------------------------------------------------------------------------- */
/* Imports */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include "cw2-config.h"
#include "cw2-aux.h"
#include "lcd-binary.h"
#include "lcd-fcts.h"

/* --------------------------------------------------------------------------- */
/* Constants (see cw2-config.h for default values) */

// number of possible values at each position in the sequence
static  int digits = DIGITS;
// length of the sequence
static  int seqlen = SEQL;

// SECRET sequence
static int* theSeq = NULL;

// FOUND sequence
static int* foundSeq = NULL;

// base address of GPIO memory
volatile unsigned int gpiobase ;
volatile uint32_t *gpio ;

// flag to be set in signal handler for interval times
static int timed_out = 0;

/* --------------------------------------------------------------------------- */
/* external prototypes */

#ifdef HAMM_ASM
// Prototype for the Assembler fct; only needed for an Asm implementation
int hamming(const int *x, const int *y, int seqlen);
#endif

/* --------------------------------------------------------------------------- */
// Timers and signal handlers
// Get a timestamp in micro-seconds 

uint64_t timeInMicroseconds(void){
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)tv.tv_usec; // in us
}

/*
  This should be a signal handler for signals issued by the interval timer.
*/
void timer_handler (int signum)
{
  timed_out = 1;
}

/* 
   Initialise the interval timer here.
*/
void initITimer(uint64_t timeout){
  struct sigaction sa;
  struct itimerval timer;
  
  sa.sa_handler = timer_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, NULL);
  
  timer.it_value.tv_sec = timeout / 1000000;
  timer.it_value.tv_usec = timeout % 1000000;
  
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  
  setitimer(ITIMER_REAL, &timer, NULL);
}

/* --------------------------------------------------------------------------- */
/* Helper functions for the main app */

/* 
   Initialise the secret sequence of values (of length seqlen) 
   Uses global variables: @seqlen@ for length of sequence, @digits@ for the possible number of values
*/
void initSeq(int seqlen, int digits) {
  unsigned long value, r;

  if (theSeq==NULL) {
    theSeq = calloc(seqlen, sizeof(int));
    if (theSeq==NULL) {
      failure(true, "calloc failed");
    }
  }

  srand((unsigned int)time(NULL));
  for (int i=0; i<seqlen; i++) {
    r = rand();
    value = (r % digits) + 1;
    theSeq[i] = value;
  }
}

/* 
   Show given sequence @seq@ of length @seqlen@ on the terminal.
*/
void showSeq(const int *seq, int seqlen) {
  // Iterates through each element in the sequence.
  for (int i=0; i<seqlen; i++) {
    printf("%d ", seq[i]);
  }
  printf("\n");
}

/* 
   Parse an integer value @val@ as a list of digits, and put them into @seq@ 
   Needed for processing command-line with options -s or -u            
*/
void readSeq(int *seq, int seqlen, int val) {
  char valStr[32];
  int i;
  size_t strLen;

  snprintf(valStr, sizeof(valStr), "%d", val);
  strLen = strlen(valStr);
  
  for (i = 0; i < seqlen && i < (int)strLen; i++) {
    seq[i] = valStr[i] - '0';
      if (seq[i] < 1 || seq[i] > digits) {
          seq[i] = 1;
      }
  }

  // pad with 1 values if necessary
  for (; i < seqlen; i++) {
      seq[i] = 1;
  }
}

/* --------------------------------------------------------------------------- */
/* Interface fcts on top of the low-level pin I/O code                         */
/* 
   Turning LED on/off is just a call to low-level fct digital_write()
 */
static inline
void write_LED(volatile uint32_t *gpio, int pin, int value) {
  digital_write (gpio, pin, value);
}

// Blinks LED at GPIO pin number 'led' 'c' times.
void blinkN(volatile uint32_t *gpio, int led, int c) { 
  for (int i = 0; i < c; i++) {
    // Turns on LED
    write_LED(gpio, led, HIGH);
    usleep(200000);
    // Turns off LED
    write_LED(gpio, led, LOW);
    usleep(200000);
  }
}
/* ----------------------------------------------------------------------------- */
/* Helper fcts for this app                                                      */
/*
  HINT: the libc function powl(x, n) computes @x@ to the power of @n@
        and is a useful function for Task~5, using arbitrary sequence length.
*/
/* ***************************************************************************** */
/* NOTE: CPP flag should select Assembler version of Hamming distance            */
/*       Set the flag in the Makefile using -DHAMM_ASM                           */
/* If the flag is NOT set (as below) a C version of the Hamming distance should be selected */
/* ***************************************************************************** */

/*
  Show the Hamming distance (of @seq1@ and @seq2@) in @code@ on the terminal.
*/
void showHamm(int code) {
  printf("--------------------- \n");
  
  // Print Hamming distance
  printf("Hamming Distance: %d\n", code);
  
  // Spacing 
  printf("--------------------- \n");
}

/* ------------------------------------------------------------------------------------------------------  
   @submit_PIN@: Submit a PIN for checking against a secret pin.  EXPENSIVE!
   @attSeq@ is the attempted sequence, submitted for testing against the secret pin.
   @seqlen@ is the length of the sequence.
   @submitDelay@ is the delay in processing the sequence (can be changed with -S cmdline option).
   NOTE: the secret sequence is in the global variable @theSeq@ which is not an argument
         because it should be hidden to the caller.
   The function tests @attSeq@ against the secret sequence @theSeq@, by computing the Hamming distance.
   If the Hamming distance is 0, both sequences are equal, and the sequence has been found (in @attSeq@)
   The return value is a boolean value whether the sequence has been found.
*/

int submit_PIN(const int *attSeq, int seqlen, int submitDelay) {
  int found = 0;
  
  usleep(submitDelay);  // simulating a slow submit action
  found = hamming(theSeq, attSeq, seqlen) == 0;
  return found;
} 

 //-----------------------------
 //-----HELPERS FOR TASK 5------
 //-----------------------------
 int genReplacements( int depth, int code, int *positions, int *checkSeq, int *attemptSeq, int seqlen, int digits, int submitDelay, bool opt_e, int *found, int *found_at, int *submits, int *attempts)
 {
   //base case: all positions have been given new values
   if (depth == code)
   {
     (*attempts)++;//count current sequence
     (*submits)++;//count submission
     int is_match = submit_PIN(checkSeq, seqlen, submitDelay); //test current sequence for match 
     
     //if this is the first match found
     if(is_match) {
       if(!(*found))
       {
         *found = 1;//set to found
         *found_at = *attempts;// store number of attempts
          memcpy(foundSeq, checkSeq, seqlen * sizeof(int));//copy sequence
       }
       
       //if not exhaustive search
       if (!opt_e)
       {
          return 1;//stops recursion when search stops
       }
    }
     
     return 0;//else continue search
   }
   
   int pos = positions[depth];//get current position to change
   
   for(int i = 1; i <= digits; i++)//go through each possible digit value
   {
     if (i == attemptSeq[pos])
     {
       continue;//if original value skip as it needs to be different
     }
     
     checkSeq[pos] = i;//set new value at this position
   
    //recursively change next position
     if (genReplacements(depth + 1, code,positions, checkSeq, attemptSeq, seqlen, digits, submitDelay, opt_e, found, found_at, submits, attempts))
     {
       return 1;//stop if solution found
     }
   }
   return 0;//no solution found at this recursion branch
}



int choosePositions(int start,int depth, int code, int *positions, int *checkSeq, int *attemptSeq, int seqlen, int digits, int submitDelay, bool opt_e, int *found, int *found_at, int *submits, int *attempts)
{
  //base case: enough positions have been chosen
  if (depth == code)
  {
    for(int i = 0; i< seqlen; i++)
    {
      checkSeq[i] = attemptSeq[i];//change sequence back to original sequence
    }
    //generate all replacements for chosen positions
    return genReplacements(0, code, positions, checkSeq, attemptSeq, seqlen, digits, submitDelay, opt_e, found, found_at, submits, attempts);
  }
  
  //choose next postion to change
  for (int i = start; i<=seqlen - (code - depth); i++)
  {
    positions[depth] = i;//store position index
    
    //recursively choose next position
    if (choosePositions(i+1, depth + 1, code, positions, checkSeq, attemptSeq, seqlen, digits, submitDelay, opt_e, found, found_at, submits, attempts))
    {
      return 1;//stop if solution found 
    }
  } 
    return 0;//no valid combination found
}

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int main(int argc, char **argv){
  int found = 0, code = 0, refCode = 0;
  int buttonPressed = 0;
  
  // use these to count: number of comparisons in total, found after how many attempts, total number of submits
  int attempts = 0, found_at = 0, submits = 0;
  int *attemptSeq = NULL, *refSeq = NULL;
  double startTime, stopTime;

  // variables holding Pin numbers for LEDs and button
  int pinLED = LED, pinLED2 = LED2, pinButton = BUTTON;
  int   fd ;

  // variables for command-line processing
  // command-line options
  bool opt_e = false, opt_l = false;
  int opt_m = 0, opt_n = 0, opt_S = 0, opt_s = 0, opt_r = 0;
  // variables derived from command line options
  bool verbose = false, help = false, debug = false, unit_test = false;
  int submitDelay = SUBMIT_DELAY;
  
  // -------------------------------------------------------
  // process command-line arguments

  // see: man 3 getopt for docu and an example of command line parsing
  { // see the CW spec for the intended meaning of these options
    int opt;
    while ((opt = getopt(argc, argv, "hvdeluS:s:r:m:n:")) != -1) {
      switch (opt) {
      case 'v':
	verbose = true;
	break;
      case 'h':
	help = true;
	break;
      case 'd':
	debug = true;
	break;
      case 'e':
	opt_e = true;
	break;
      case 'l': // LCD test only
	opt_l = true;
	break;
      case 'u':
	unit_test = true;
	break;
      case 'S':
	opt_S = atoi(optarg);
	submitDelay = opt_S;
	break;
      case 's':
	opt_s = atoi(optarg); 
	break;
      case 'r':
	opt_r = atoi(optarg); 
	break;
      case 'm':
	opt_m = atoi(optarg);
	digits = opt_m;
	break;
      case 'n':
	opt_n = atoi(optarg);
	seqlen = opt_n;
	break;
      default: /* '?' */
	fprintf(stderr, "Usage: %s [-h] [-v] [-d] [-e] [-m <maxval> ] [-n <seqlen>] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]  \n", argv[0]);
	exit(EXIT_FAILURE);
      }
    }
  }

  if (help) {
    fprintf(stderr, "pinCrack program, running on a Raspberry Pi, with connected LED, button and LCD display\n"); 
    fprintf(stderr, "Use the button for input of numbers. The LCD display will show the matches with the secret sequence.\n"); 
    fprintf(stderr, "For full specification of the program see: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2026.pdf\n"); 
    fprintf(stderr, "Usage: %s [-h] [-v] [-d] [-e] [-u <seq1> <seq2>] [-s <secret seq>] [-r <reference seq>]  \n", argv[0]);
    exit(EXIT_SUCCESS);
  }

  if (verbose) {
    printf("Settings for running the program\n");
    printf("Verbose is %s\n", (verbose ? "ON" : "OFF"));
    printf("Debug is %s\n", (debug ? "ON" : "OFF"));
    printf("Unittest is %s\n", (unit_test ? "ON" : "OFF"));
    printf("Exhaustive search is %s\n", (opt_e ? "ON" : "OFF"));
    printf("Submit delay is %d\n", submitDelay);
    if (opt_s)  printf("Secret sequence set to %d\n", opt_s);
    if (opt_r)  printf("Reference sequence set to %d\n", opt_r);
  }

  if (verbose) {
    printf("Hint: remember to compute the Hamming distance in each iteration and assign it to variable code; current (unused) value: %d\n", code);
    printf("Code style requirement: collect the values of the input sequence in the variable attemptSeq; current (unused) value: %p\n", attemptSeq);
  }  

  //Q2 - Initialising the given sequence and the found sequence
  attemptSeq = calloc(seqlen, sizeof(int));
  foundSeq = calloc(seqlen, sizeof(int));
  
  //Check memory was successfully allocated
  if (!attemptSeq || !foundSeq) {
    failure(true, "Allocation has failed.");
  }

  if (opt_s) { // if -s option is given, use the sequence as SECRET sequence
    if (theSeq==NULL) {
      theSeq = calloc(seqlen, sizeof(int));
      if (theSeq==NULL) {
        failure(true, "calloc failed");
      }
    }
    readSeq(theSeq, seqlen, opt_s);
    if (verbose) {
      fprintf(stderr, "Running program with secret sequence:\n");
      showSeq(theSeq,seqlen);
    }
  }
  
  if (opt_r) { // if -r option is given, use the sequence as REFERENCE sequence
    if (refSeq==NULL) {
      refSeq = calloc(seqlen, sizeof(int));
      if (refSeq==NULL) {
        failure(true, "calloc failed");
      }
    }
    readSeq(refSeq, seqlen, opt_r);
    if (verbose) {
      fprintf(stderr, "Running program with reference sequence:\n");
      showSeq(refSeq,seqlen);
    }
  }
  
  /* --------------------------------------------------------------------------- */
  /* Configuration of the LCD display */
  int bits, rows, cols ;

  // hard-coded: 16x2 display, using a 4-bit connection
  bits = 4; 
  cols = 16; 
  rows = 2; 

  printf ("Raspberry Pi configuration: red LED: %d; green LED: %d; button: %d\n", pinLED2, pinLED, pinButton) ;
  printf ("Raspberry Pi LCD driver for a %dx%d display (%d-bit wiring) \n", cols, rows, bits) ;

  /* --------------------------------------------------------------------------- */
  /* Check for root priveleges (needed for controlling LEDs etc) */

  if (geteuid () != 0) {
    fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n") ;
    exit(EXIT_FAILURE);
  }
  
  /* --------------------------------------------------------------------------- */
  /* constants for RPi2/3. NOTE: RPi4 needs a different base address */
  // -----------------------------------------------------------------------------
  // RPi3
  gpiobase = 0x3F200000 ;
  // -----------------------------------------------------------------------------
  // memory mapping 
  // Open the master /dev/memory device

  if ((fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC) ) < 0)
    return failure (false, "setup: Unable to open /dev/mem: %s\n", strerror (errno)) ;

  // GPIO:
  gpio = mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpiobase) ;
  if ((int32_t)gpio == -1)
    return failure (false, "setup: mmap (GPIO) failed: %s\n", strerror (errno)) ;

  // -----------------------------------------------------------------------------
  // Setting mode of pins
  // -----------------------------------------------------------------------------
  
  // Sets green LED to output
  pin_mode(gpio, pinLED, OUTPUT);
  // Sets red LED to output
  pin_mode(gpio, pinLED2, OUTPUT);
  // Sets button to input
  pin_mode(gpio, pinButton, INPUT);

  // -----------------------------------------------------------------------------
  // Initialise the LCD display
    lcd_init(gpio);
    
    // LCD Test
    if (opt_l) {
      lcd_puts(gpio, "TEST");
      exit(EXIT_SUCCESS);
    }
    
  // -----------------------------------------------------------------------------
  // App initialisation
  
  /* Initialise the secret sequence */
  if (!opt_s)  initSeq(seqlen, digits);

  // -----------------------------------------------------------------------------
  // Unit testing: check the Hamming distance between two given sequences

  if (unit_test) { // unit test: just print the Hamming distance

    if (!opt_r) {
      fprintf(stderr, "Need to use both -s and -r for unit testing (with -u)\n");
      exit(EXIT_FAILURE);
    }

    // Show sequences
    refCode = hamming(theSeq, refSeq, seqlen);
    printf("--------------------- \n");
    printf("The secret sequence is: ");
    showSeq(theSeq,seqlen);
    printf("The reference sequence is: ");
    showSeq(refSeq,seqlen);
    
    // Show the hamming distance.
    showHamm(refCode);
    lcd_command(gpio, LCD_HOME);
    lcd_puts(gpio, "Hamming: ");
    char tmp[4];
    sprintf(tmp, "%d", refCode);
    lcd_puts(gpio, tmp);
    
    exit(EXIT_SUCCESS);
  }  

  // -----------------------------------------------------------------------------
  
  /* Print Greetings Message on LCD display */
  //display surname "SCOTT"
  lcd_puts(gpio, "SCOTT");

  //blink the colours for my surname "SCOTT"
  blinkN(gpio, pinLED2, 1);//red
  blinkN(gpio, pinLED2, 1);//red
  blinkN(gpio, pinLED, 1);//green
  blinkN(gpio, pinLED2, 1);//red
  blinkN(gpio, pinLED2, 1);//red

  /* Wait for ENTER key before continuing */
  waitForEnter () ; // -------------------------------------------------------

  // CLEAR DISPLAY
  lcd_command(gpio, LCD_CLEAR);   lcd_command(gpio, LCD_HOME); 
  
  /* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
  /* Main part of the application  */
  // PHASE 1: sequence input

  // Input with buttons case
  if (!opt_r) {
    // Iterate over all elements of the sequence
    for (int i=0; i<seqlen; i++) {
      int pressCount = 0;
      timed_out = 0;

      initITimer(2000000);
   
      while(!timed_out) {
        buttonPressed = read_button(gpio, pinButton);
      
        if (buttonPressed == HIGH) {
        
          //If its at the maximum possible number, ignore press
          if(pressCount == digits) {
            break;
          }
          write_LED(gpio, pinLED, HIGH);
          pressCount++;
        
          // Waits until button is unpressed.
          while (read_button(gpio, pinButton) == HIGH);
        
          // Debounce
          delay(20000);
        
          timed_out = 0;
          // Reset timer for next expected press.
          initITimer(2000000);
        
        } else {
          write_LED(gpio, pinLED, LOW);
        }
      }
    
    // Sets i'th number in sequence to number of presses
    attemptSeq[i] = pressCount;
    
    // Blink red LED to indicate number was processed.
    blinkN(gpio,pinLED2, 1);
    // Blink green LED pressCount times to indicate what number was processed.
    blinkN(gpio, pinLED, pressCount);
    } 
  }
  
  // Reference sequence was given case.
  else
  {
    // Fill attemptSeq with the reference Sequence.
    memcpy(attemptSeq, refSeq, seqlen * sizeof(int));
  }

    // -------------------------------------------------------
    // PHASE 2: Main Task: full search

    // Print the version of the code this is running; set values in cw2-config.h
    printf("--------------------- \n");
    printf(">> Version %d: %s with %d digits and %d sequence length\n", VERSION, VERSION_STR, digits, seqlen);
#ifdef HAMM_ASM
    printf(">> HAMM_ASM version: Hamming distance in ARM Assembler\n");
#else
    printf(">> Hamming in C version\n");
#endif

    if (debug) {
      printf("--------------------- \n");
      printf("---- Debug mode ----\n");
      printf("The secret sequence is: ");
      showSeq(theSeq,seqlen);
      printf("The inputted sequence is: ");
      showSeq(attemptSeq,seqlen);
    }

    // time-stamp
    startTime = timeInMicroseconds();

    // Calculate and show the hamming distance
    code = hamming(attemptSeq, theSeq, seqlen);
    showHamm(code);
    
    // Allocates Memory for sequence array (will store each possible sequence).
    int *checkSeq = calloc(seqlen, sizeof(int));

    //Checks for successful allocation
    if (!checkSeq) {
      failure(true, "Memory Allocation unsuccessful.");
    }
    
    //allocate memory for positions that will be modified
    int *positions = NULL;
    if (code > 0)
    {
      positions = calloc(code, sizeof(int));
      if (!positions)
      {
        failure(true, "Memory Allocation unsuccessful.");
      }
    }
    
    //if sequence guessed correctly
    if (code == 0)
    {
      // Populate the found sequence for print message at the end.
      memcpy(foundSeq, attemptSeq, seqlen * sizeof(int));
      
      //Indicate an attempt was made
      attempts++;
      submits++;
      
      //Indicate pin was found
      found = submit_PIN(attemptSeq, seqlen, submitDelay);
      found_at = attempts;
    }
    
    //if sequence has not been found yet
    else
    { 
      choosePositions(0,0,code, positions,  checkSeq, attemptSeq, seqlen, digits, submitDelay, opt_e, &found, &found_at, &submits, &attempts);
      
    }
     
    stopTime = timeInMicroseconds();

    printf("Runtime; %.4f secs\n", (stopTime-startTime) / 1000000.0);
    printf("Sequence %s\n", found ? "found" : "not found");
    printf("%s search finished for %d digits and %d seqlen:\n%d attempts (found at %d), %d submits\n",
	 (opt_e ? "Exhaustive" : "Non-exhaustive"), digits, seqlen, attempts, found_at, submits);
    printf("--------------------- \n");
    printf("Secret sequence was: ");
    showSeq(theSeq,seqlen);

  /* write an exit message to the LCD display                                      */
  printf("Pin found!\n");
  printf("--------------------- \n");
  
  // Blink green LED twice
  blinkN(gpio, pinLED, 2);

  // Display to LCD that pin was found on first row
  lcd_command(gpio, LCD_CLEAR);
  lcd_command(gpio, LCD_HOME);
  lcd_puts(gpio, "PIN FOUND");
  
  // Go to second row
  lcd_command(gpio, 0xC0);
  
  //Display found sequence on LCD Display
  for (int i = 0; i < seqlen; i++) {
    char tmp[4];
    snprintf(tmp, sizeof(tmp), "%d ", foundSeq[i]);
    lcd_puts(gpio, tmp);
  }

  //Free allocated arrays
  free(checkSeq);
  free(theSeq);
  free(refSeq);
  free(attemptSeq);
  free(foundSeq);
  free(positions);
  
  //Close GPIO mapping
  munmap((void *)gpio, BLOCK_SIZE);
  close(fd);
  
  return 0;
}
