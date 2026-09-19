#define _DEFAULT_SOURCE
#include <unistd.h>

#include "cw2-config.h"
#include "cw2-aux.h"
#include "lcd-fcts.h"
#include "lcd-binary.h"

// Send four bits of data
static void lcd_send4(volatile uint32_t *gpio, int value) {
    // Set the data lines
    digital_write(gpio, LCD_PIN_D7, (value >> 3) & 1);
    digital_write(gpio, LCD_PIN_D6, (value >> 2) & 1);
    digital_write(gpio, LCD_PIN_D5, (value >> 1) & 1);
    digital_write(gpio, LCD_PIN_D4, value & 1);

    // Pulse the E line
    digital_write(gpio, LCD_PIN_E, 1);
    usleep(50);
    digital_write(gpio, LCD_PIN_E, 0);
    usleep(50);
}

// Send eight bits of data
static void lcd_send(volatile uint32_t *gpio, int value) {
    // Send the top four bits
    lcd_send4(gpio, (value >> 4) & 0xf);

    // Send the bottom four bits
    lcd_send4(gpio, value & 0xf);
}

// Send a command
void lcd_command(volatile uint32_t *gpio, int command) {
    digital_write(gpio, LCD_PIN_RS, 0);
    lcd_send(gpio, command);

    // As we can only write to the LCD, we can't poll the status register to
    // see when the command is finished, so we have to delay instead. Most
    // commands take 37 us to complete, so the 50 us delay above is
    // sufficient, except LCD_CMD_HOME which takes 1.52 ms.
    if (command == LCD_HOME) {
        usleep(2000);
    }
}

// Display a character
void lcd_putchar(volatile uint32_t *gpio, char data) {
    digital_write(gpio, LCD_PIN_RS, 1);
    lcd_send(gpio, data);
}

// Display a string
void lcd_puts(volatile uint32_t *gpio, const char *data) {
    const char *p = data;
    while (*p != '\0') {
        lcd_putchar(gpio, *p++);
    }
}

// Some specific commands, just as examples
void lcd_clear(volatile uint32_t *gpio) {
  lcd_command(gpio, LCD_CLEAR);
}

void lcd_home(volatile uint32_t *gpio) {
  lcd_command(gpio, LCD_HOME);
}

void lcd_write_row(volatile uint32_t *gpio, int row, const char *data) {
  if (row<0 || row>2) return;
  lcd_command(gpio, (row==1) ? LCD_DGRAM : LCD_DGRAM | 0x40);
  lcd_puts(gpio, data);
}


// Initialise the GPIO pins and the LCD itself
void lcd_init(volatile uint32_t *gpio) {
    // Make all the pins outputs
    pin_mode(gpio, LCD_PIN_RS, OUTPUT);
    pin_mode(gpio, LCD_PIN_E, OUTPUT);
    pin_mode(gpio, LCD_PIN_D4, OUTPUT);
    pin_mode(gpio, LCD_PIN_D5, OUTPUT);
    pin_mode(gpio, LCD_PIN_D6, OUTPUT);
    pin_mode(gpio, LCD_PIN_D7, OUTPUT);

    // This implements the 4-bit initialisation sequence from the HD44780U
    // datasheet, figure 24. This will work regardless of the initial state
    // of the LCD; we don't know if it's currently in 4-bit or 8-bit mode.

    // Ensure E is low for a while to start with, and the LCD
    // has had time to start up
    digital_write(gpio, LCD_PIN_E, 0);
    usleep(20000);

    // Put the LCD into 4-bit mode
    digital_write(gpio, LCD_PIN_RS, 0);
    lcd_send4(gpio, (LCD_FUNC | LCD_FUNC_DL) >> 4);
    usleep(10000);
    lcd_send4(gpio, (LCD_FUNC | LCD_FUNC_DL) >> 4);
    usleep(200);
    lcd_send4(gpio, (LCD_FUNC | LCD_FUNC_DL) >> 4);
    lcd_send4(gpio, LCD_FUNC >> 4);

    // Set the display size
    lcd_command(gpio, LCD_FUNC | LCD_FUNC_N);

    // Turn the display on, cursor off
    lcd_command(gpio, LCD_CTRL | LCD_CTRL_D);

    // Clear the display
    lcd_command(gpio, LCD_CLEAR);

    // Set entry mode
    lcd_command(gpio, LCD_ENTRY | LCD_ENTRY_ID);

    // Return home, setting shift to 0
    lcd_command(gpio, LCD_HOME);
}
