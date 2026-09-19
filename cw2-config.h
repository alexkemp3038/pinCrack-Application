#ifndef CW2_CONFIG_H
#define CW2_CONFIG_H

/* 
   Config settings for the main app
*/

// HINT: you can use these flags to indicate which tasks are covered
#define VERSION 5
#define VERSION_STR "arbitrary number of dimensions"

// Enable interval-times
#define ITIMER
// Enable output to LCD display
#define OUTPUT_LCD


// simulated delay for a submission in micro-seconds
#define SUBMIT_DELAY 20

// constant: number of possible values at each position in the sequence
#define DIGITS 3
// constant: length of the sequence
#define SEQL 3

/* 
   Wiring (see call to lcd-init() in main, using BCM numbering)
*/

// the green data LED
#define LED 26
// the red control LED
#define LED2 5
// the button
#define BUTTON 19


// GPIO pins used by the LCD
#define LCD_PIN_RS 25
#define LCD_PIN_E 24
#define LCD_PIN_D4 23
#define LCD_PIN_D5 10
#define LCD_PIN_D6 27
#define LCD_PIN_D7 22

/*
  Delays and timings
*/

// all in micro-seconds
#define DELAY   700000
#define TIMEOUT 3000000

#endif
