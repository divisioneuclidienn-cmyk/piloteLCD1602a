#include <stdio.h>
#include <driver/gpio.h>
#include "LCD_1602A_driver.h"

void app_main(void){
    LCD_Pinout LCD_CONFIG = {
        .E = GPIO_NUM_27,
        .RS = GPIO_NUM_14,
        .RW = GPIO_NUM_12,
        .DB = {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32}
    }; 

    LCD_init(&LCD_CONFIG);

    LCD_print_char(&LCD_CONFIG, 'X');

}