#ifndef LCD_H
#define LCD_H

// GPIO pins used by the LCD now in cw2-config.h

// Commands
#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY 0x04
#define LCD_CTRL 0x08
#define LCD_CDSHIFT 0x10
#define LCD_FUNC 0x20
#define LCD_CGRAM 0x40
#define LCD_DGRAM 0x80

// Flags for LCD_ENTRY
#define LCD_ENTRY_ID 0x02
#define LCD_ENTRY_S 0x01

// Flags for LCD_CTRL
#define LCD_CTRL_D 0x04
#define LCD_CTRL_C 0x02
#define LCD_CTRL_B 0x01

// Flags for LCD_FUNC
#define LCD_FUNC_DL 0x10
#define LCD_FUNC_N 0x08
#define LCD_FUNC_F 0x04

#include <stdint.h>

void lcd_command(volatile uint32_t *gpio, int command);
void lcd_putchar(volatile uint32_t *gpio, char data);
void lcd_puts(volatile uint32_t *gpio, const char *data);
void lcd_init(volatile uint32_t *gpio);

void lcd_clear(volatile uint32_t *gpio);
void lcd_home(volatile uint32_t *gpio);
void lcd_write_row(volatile uint32_t *gpio, int row, const char *data);

#endif
