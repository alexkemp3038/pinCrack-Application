#ifndef LCD_BINARY_H
#define LCD_BINARY_H

#include <stdint.h>

/* Core hardware/binary interface */
void pin_mode(volatile uint32_t *gpio, int pin, int mode);
void digital_write(volatile uint32_t *gpio, int pin, int value);
int read_button(volatile uint32_t *gpio, int button);

#endif
