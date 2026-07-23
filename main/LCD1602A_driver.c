#include <stdio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>

#define DB4 26
#define DB5 25
#define DB6 33
#define DB7 32

// 7, 6, 5, 4
int GPIO_DB[4] = {32, 33, 25, 26};

#define RS 14 //0
#define RW 12 //0 (Write)
#define E 27 //0 (1 = DO)

#define DDRAM_SECOND_LIGN_STARTING_ADRESS 0x40

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

void LCD_send_nibble(uint8_t nibble){
    int offset = 3;
    int offset_nibble;
    int gpio_value;

    //setup DB
    for(int i = 0; i < 4; i++){
        offset_nibble = nibble >> offset;
        gpio_value = offset_nibble & 1; 
        gpio_set_level(GPIO_DB[i], gpio_value);
        offset--;
    }

    //stable data wait
    esp_rom_delay_us(1);

    //sending data
    gpio_set_level(E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(E, 0);
}

void LCD_send_command(uint8_t RS_value, uint8_t RW_value, uint8_t command, int delay_us){
    int nibble1 = command >> 4;
    int nibble2 = command & 0b00001111;

    gpio_set_level(RS, RS_value);
    gpio_set_level(RW, RW_value);

    LCD_send_nibble(nibble1);
    LCD_send_nibble(nibble2);

    esp_rom_delay_us(delay_us);
}

void LCD_clear_display(){
    LCD_send_command(0, 0, 0b00000001, 1550);
}

void LCD_4bit_mode(){
    gpio_set_level(RS, 0);
    LCD_send_nibble(0b0010);
    esp_rom_delay_us(40); //39us
}

//set DDRAM adress
int LCD_set_AC(uint8_t adress){
    if(adress > 80) { return 1; } //adress not valid (0-80)

    int final_command = 0b10000000 | adress;
    LCD_send_command(0, 0, final_command, 40);

    ESP_LOGI("LCD", "address=0x%02X command=0x%02X", adress, final_command);

    return 0;
}

//écrire uniquement sur la partie visible du LCD
int LCD_set_cursor(int row, int column){
    if(row > 2 || row < 1 || column > 16 || column < 1) { return 1; }
    uint8_t AC_final_adress;

    if (row == 1){
        AC_final_adress = column - 1;
    }
    else {
        AC_final_adress = (DDRAM_SECOND_LIGN_STARTING_ADRESS + column) - 1;
    }

    ESP_LOGI("LCD", "row=%d column=%d AC=0x%02X", row, column, AC_final_adress);

    return LCD_set_AC(AC_final_adress);
}

//setup un peu moche ?
void setup(){

    gpio_reset_pin(RS);
    gpio_set_direction(RS, GPIO_MODE_OUTPUT);
    gpio_set_level(RS, 0);

    gpio_reset_pin(RW);
    gpio_set_direction(RW, GPIO_MODE_OUTPUT);
    gpio_set_level(RW, 0);

    gpio_reset_pin(E);
    gpio_set_direction(E, GPIO_MODE_OUTPUT);
    gpio_set_level(E, 0);

    gpio_reset_pin(DB4);
    gpio_reset_pin(DB5);
    gpio_reset_pin(DB6);
    gpio_reset_pin(DB7);

    gpio_set_direction(DB4, GPIO_MODE_OUTPUT);
    gpio_set_direction(DB5, GPIO_MODE_OUTPUT);
    gpio_set_direction(DB6, GPIO_MODE_OUTPUT);
    gpio_set_direction(DB7, GPIO_MODE_OUTPUT);
}

void LCD_print_char(char letter){
    int letter_ascii_nb = (unsigned int)letter;
    LCD_send_command(1, 0, letter_ascii_nb, 40); //37us
}

//0 0 1 DL N F - -
void LCD_function_set(LCD_DataLength DL, LCD_LineNumber N, LCD_FontSize F){
    uint8_t mask = 0;
    uint8_t command = 0b00100000;

    mask |= (DL << 4);
    mask |= (N << 3);
    mask |= (F << 2);

    command |= mask;
    LCD_send_command(0, 0, command, 40);
}

// 0 0 0 0 1 display cursor cursor_blinking
void LCD_display_control(bool display, bool cursor, bool cursor_blinking){
    uint8_t command = 0b00001000;
    uint8_t mask = 0;

    mask |= (int)display << 2;
    mask |= (int)cursor << 1;
    mask |= (int)cursor_blinking;
    
    command |= mask;
    LCD_send_command(0, 0, command, 40);
}

LCD_entry_mode(LCD_CursorDirection cursor_direction, bool display_shift){
    
}

//0 0 0 1 S/C R/L X X
void LCD_shift(LCD_ShiftControl shift_control, LCD_Direction direction){
    uint8_t command = 0b00010000;
    uint8_t mask = 0;

    mask |= shift_control << 3;
    mask |= direction << 2;

    command |= mask;
    LCD_send_command(0, 0, command, 40);
}

//LCD known state
void LCD_initialize(){
    esp_rom_delay_us(15000); //power on delay

    gpio_set_level(RS, 0);
    gpio_set_level(RW, 0);

    LCD_send_nibble(0b0011); //make sure 8bit mode is set
    esp_rom_delay_us(4100);
    LCD_send_nibble(0b0011);
    esp_rom_delay_us(100);
    LCD_send_nibble(0b0011);
    esp_rom_delay_us(40);

    LCD_4bit_mode();
    LCD_function_set(LCD_4BIT, LCD_LINE_2, LCD_FONT_5X8);
    //LCD_send_command(0, 0, 0b00001010, 40); //screen off, cursor on not blinking (display ON/OFF control)
    LCD_display_control(false, true, false);
    LCD_clear_display();
    LCD_send_command(0, 0, 0b00000110, 40); //entry mode set
    LCD_send_command(0, 0, 0b00001110, 40); //screen on, cursor on not blinking (display ON/OFF control)
}

void app_main(void)
{   
    setup();
    LCD_initialize();

    LCD_print_char('A');
    LCD_shift(LCD_CURSOR, LCD_RIGHT);
    LCD_print_char('A');
    esp_rom_delay_us(2000000);
    LCD_shift(LCD_SCREEN, LCD_RIGHT);

}

//GIT
//librairie
//readme file
//pilote propre
//shell lcd :)
//test git