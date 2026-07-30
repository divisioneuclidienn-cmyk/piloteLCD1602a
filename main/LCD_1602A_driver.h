#include <driver/gpio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>

#ifndef LCD_1602A_DRIVER_H
#define LCD_1602A_DRIVER_H

typedef enum{
    LCD_8BIT = 1,
    LCD_4BIT = 0

} LCD_DataLength;

typedef enum{
    LCD_LINE_2 = 1,
    LCD_LINE_1 = 0

} LCD_LineNumber;

typedef enum{
    LCD_FONT_5X11 = 1,
    LCD_FONT_5X8 = 0
} LCD_FontSize;

typedef enum{
    LCD_SCREEN = 1,
    LCD_CURSOR = 0
} LCD_ShiftControl;

typedef enum{
    LCD_RIGHT = 1,
    LCD_LEFT = 0
} LCD_Direction;

typedef struct Pinout_struct {
    gpio_num_t E;
    gpio_num_t RS;
    gpio_num_t RW;

    gpio_num_t DB[4];
} LCD_Pinout;

void LCD_print_char(const LCD_Pinout *LCD_CONFIG, char letter);
void LCD_init(const LCD_Pinout *LCD_CONFIG);

#endif