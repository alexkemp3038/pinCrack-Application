#ifndef CW2_AUX_H
#define CW2_AUX_H

#include <stdbool.h>
#include <stdlib.h>

// Modes for pin_mode
#define	INPUT			 0
#define	OUTPUT			 1

// Modes for pin_mode
#define	LOW			 0
#define	HIGH			 1

// Constants needed  in cw2.c for memory mapping
#define	PAGE_SIZE		(4*1024)
#define	BLOCK_SIZE		(4*1024)

void delay(unsigned long howLong);
void delayMicroseconds (unsigned int howLong);
void waitForEnter (void);
int failure (bool fatal, const char *message, ...);

#endif
