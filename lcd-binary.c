#include <stdio.h>  // debugging only
#include "gpio.h"
#include "lcd-binary.h"
#include "cw2-aux.h"


/* ***************************************************************************** */
/* HINT: use the CPP variable ASM with ifdef's to select Asm (or C) versions of the code. */
/* ***************************************************************************** */


/*
  Hardware Interface function.
  Set the mode for pin number @pin@ to @mode@ (can be INPUT or OUTPUT (encoded as int)).
*/
#ifdef ASM
//ARM VERSION
void pin_mode(volatile uint32_t *gpio, int pin, int mode)
{
  asm volatile (
    //R0 = PIN
    "\tMOV R0, %[pin]\n"
    //R1 = MODE
    "\tMOV R1, %[mode]\n"
    //R2 = GPIO
    "\tMOV R2, %[gpio]\n"
    //R3 = 10
    "\tMOV R3, #10\n"
    //R4 = FSEL (PIN / 10)
    "\tUDIV R4, R0, R3\n"
    //R5 = (PIN / 10) * 10
    "\tMUL R5, R4, R3\n"
    //R5 = PIN MOD 10 (also PIN - (PIN / 10) * 10)
    "\tSUB R5, R0, R5\n"
    //R6 = 3
    "\tMOV R6, #3\n"
    //R5 = SHIFT ((PIN MOD 10) * 3)
    "\tMUL R5, R5, R6\n"
    //R4 = Correct offset
    "\tLSL R4, R4, #2\n"
    //R6 = GPIO + FSEL
    "\tADD R6, R2, R4\n"
    //R7 = *(GPIO + FSEL)
    "\tLDR R7, [R6]\n"
    //R8 = 7
    "\tMOV R8, #7\n"
    //R8 = 7 << SHIFT
    "\tLSL R8, R8, R5\n"
    //R8 = ~(7 << SHIFT)
    "\tMVN R8, R8\n"
    //R7 = *(GPIO + FSEL) & ~(7 << SHIFT)
    "\tAND R7, R7, R8\n"
    //R1 = MODE << SHIFT
    "LSL  R1, R1, R5\n"
    //R7 = *(GPIO + FSEL) & ~(7 << SHIFT) | (MODE << SHIFT)
    "ORR  R7, R7, R1\n"
    //R0 = Stores register value
    "STR  R7, [R6]\n"

    :
    : [pin] "r" (pin),
      [mode] "r" (mode),
      [gpio] "r" (gpio)
    : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "memory"
  );
}

#else
//C VERSION
void pin_mode(volatile uint32_t *gpio, int pin, int mode)
{
  int fsel = pin / 10;
  int shift = (pin % 10) * 3;
  *(gpio + fsel) = (*(gpio + fsel) & ~(7 << shift)) | (mode << shift);
}
#endif

/*
  Hardware Interface function.
  Send a @value@ along pin number @pin@. Values should be LOW or HIGH (encoded as int).
*/
#ifdef ASM
//ARM VERSION
void digital_write (volatile uint32_t *gpio, int pin, int value)
{
  asm volatile (
  
    "\t MOV R2, %[gpio]\n"//load gpio into register 2
    "\t CMP %[value], %[high]\n"//compare value with the high register
    "\t BEQ 1f\n"//if equal go to high branch
    "\t MOV R3, #10\n" //else set GPSET0
    "\t B 2f\n" //go to low branch
    
    "\t 1:\n"
    "\t MOV R3, #7\n"//set to GPSET
    
    "\t 2:\n"
    "\t CMP %[pin], #32\n"//compare pin with 32
    "\t ADDGE R3, R3, #1\n"//if greater or equal to 32 add 1 bit to take it to GPSET1
    "\t LSL R3, R3, #2\n"//multiply register index by 4 
    "\t AND R4, %[pin], #31\n" // pin % 32
    "\t MOV R5, #1\n"   //load value 1 into register 5
    "\t LSL R5, R5, R4\n"// create bit mask by shifting left 1  
    "\t ADD R2, R2, R3\n"// add offset to gpio base therefore get address of correct registers
    "\t STR R5, [R2]\n"// write bit mask to register
    
    :
    : [gpio] "r" (gpio),
      [pin] "r" (pin),
      [value] "r" (value),
      [high] "r" (HIGH)
    : "r2", "r3", "r4", "r5", "cc", "memory"

  );
}

#else
//C VERSION
void digital_write (volatile uint32_t *gpio, int pin, int value)
{
  int reg;
  uint32_t binaryMask;

  //choose register GPSET0 OR GPCLRR0
  if (value == HIGH)
  {
    reg = 7;//GPSET0
  }
  else reg = 10;//GPCLR0

  //needs moved to GPSET1/CLR1 if pin >= 32
  if (pin >= 32)
  {
    reg +=1;
  }

  binaryMask = 1 << (pin % 32);//create bitmask

  *(gpio + reg) = binaryMask;//set bitmask to correct register

}
#endif

/*
  Hardware Interface function.
  Read input from a button device connected to pin @button@.. Result can be LOW or HIGH (encoded as int).
*/
#ifdef ASM
//ARM VERSION
int read_button(volatile uint32_t *gpio, int button) {
  
  int result;
  
  asm volatile (
    //R1 = GPIO
    "\tMOV R1, %[gpio]\n"
    //R2 = BUTTON
    "\tMOV R2, %[button]\n"
    //R3 = 31
    "\tMOV R3, #31\n"
    //R1 = GPIO + 13 * 4 bytes
    "\tADD R1, R1, #52\n"
    //R2 = BUTTON & 31
    "\tAND R2, R2, R3\n"
    //R3 = 1
    "\tMOV R3, #1\n"
    //R2 = 1 << (BUTTON & 31)
    "\tLSL R2, R3, R2\n"
    //R3 = *(GPIO + 13)
    "\tLDR R3, [R1]\n"
    //R3 = (*(GPIO + 13)) & (1 << (BUTTON & 31))
    "\tAND R3, R3, R2\n"

    //IF ((*(GPIO + 13)) & (1 << (BUTTON & 31)) == 0)
    "\tCMP R3, #0\n"
    
    //THEN RETURN 0 (LOW)
    "\tMOVEQ %[out], #0\n"

    //ELSE RETURN 1 (HIGH)
    "\tMOVNE %[out], #1\n"

    : [out] "=r" (result)
    : [gpio] "r" (gpio),
      [button] "r" (button)
    : "r1", "r2", "r3", "memory", "cc"
  );

  return result;
}

#else 
//C VERSION
int read_button(volatile uint32_t *gpio, int button) {
  if ((*(gpio + 13) &(1 << (button & 31))) != 0) 
    return HIGH;
  else 
    return LOW;
}
#endif

