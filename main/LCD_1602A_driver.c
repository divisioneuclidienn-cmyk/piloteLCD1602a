#include <stdio.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>

#define DDRAM_SECOND_LIGN_STARTING_ADRESS 0x40

//vu par l'utilisateur
typedef struct Pinout_struct {
    gpio_num_t E;
    gpio_num_t RS;
    gpio_num_t RW;

    gpio_num_t DB[8];
} LCD_Pinout;

/*
Enums non fusionnés car:
-paramètres attendus moins explicites
-erreurs possibles = vérifications strictes
*/
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
    LCD_DISPLAY = 1,
    LCD_CURSOR = 0
} LCD_ShiftControl;

typedef enum{
    LCD_RIGHT = 1,
    LCD_LEFT = 0
} LCD_Direction;

//stocker plutot état de tous les paramètres enum avec leurs types
typedef struct LCD_state_struct{
    
    uint8_t AC_adress;

    //function set command
    LCD_DataLength data_length;
    LCD_LineNumber line_number;
    LCD_FontSize font_size;

    //display control command
    bool display;
    bool cursor;
    bool cursor_blinking;

    //entry mode command
    LCD_Direction cursor_direction;
    bool entire_display_shift;

} LCD_State;

static LCD_State LCD_STATE;

static void LCD_setup(const LCD_Pinout *LCD_CONFIG, uint8_t DB_nb){

    gpio_reset_pin(LCD_CONFIG -> RS);
    gpio_set_direction(LCD_CONFIG -> RS, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_CONFIG -> RS, 0);

    gpio_reset_pin(LCD_CONFIG -> RW);
    gpio_set_direction(LCD_CONFIG -> RW, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_CONFIG -> RW, 0);

    gpio_reset_pin(LCD_CONFIG -> E);
    gpio_set_direction(LCD_CONFIG -> E, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_CONFIG -> E, 0);

    
    for(int i = 0; i < DB_nb; i++) gpio_reset_pin(LCD_CONFIG -> DB[i]);
    for(int i = 0; i < DB_nb; i++) gpio_set_direction(LCD_CONFIG -> DB[i], GPIO_MODE_OUTPUT);
}

//hardware API

static void LCD_send_nibble(const LCD_Pinout *LCD_CONFIG, uint8_t nibble){
    int offset_nibble;
    int gpio_value;

    for (int i = 3; i > -1; i--){
        offset_nibble = nibble >> i;
        gpio_value = offset_nibble & 1;
        gpio_set_level(LCD_CONFIG -> DB[i], gpio_value);
    }

    //stable data wait
    esp_rom_delay_us(1);

    //sending data
    gpio_set_level(LCD_CONFIG -> E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_CONFIG -> E, 0);
}

static void LCD_send_byte(const LCD_Pinout *LCD_CONFIG, uint8_t byte){
    uint8_t gpio_value;

    //i also used as an offset
    for(int i = 0; i < 8; i++){
        gpio_value = byte >> i & 1;
        gpio_set_level(LCD_CONFIG -> DB[i], gpio_value);
    }

    esp_rom_delay_us(1);

    gpio_set_level(LCD_CONFIG -> E, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_CONFIG -> E, 0);
}

static void LCD_send_command(const LCD_Pinout *LCD_CONFIG ,uint8_t RS_value, uint8_t RW_value, uint8_t command, int delay_us){

    gpio_set_level(LCD_CONFIG -> RS, RS_value);
    gpio_set_level(LCD_CONFIG -> RW, RW_value);

    if(LCD_STATE.data_length == LCD_4BIT){
        uint8_t nibble1 = command >> 4; 
        uint8_t nibble2 = command & 0b00001111;

        LCD_send_nibble(LCD_CONFIG, nibble1);
        LCD_send_nibble(LCD_CONFIG, nibble2);
    }

    //8bit mode
    else{
        LCD_send_byte(LCD_CONFIG, command);
    }

    esp_rom_delay_us(delay_us);
}

//LCD API

static void LCD_4bit_mode_nibble_cmd(const LCD_Pinout *LCD_CONFIG){
    gpio_set_level(LCD_CONFIG -> RS, 0);
    LCD_send_nibble(LCD_CONFIG ,0b0010);
    esp_rom_delay_us(40); //39us
}

static int LCD_set_AC_cmd(const LCD_Pinout *LCD_CONFIG, uint8_t adress){
    if(adress > 80) { return 1; } //adress not valid (0-80)

    uint8_t command = 0b10000000 | adress;
    LCD_send_command(LCD_CONFIG, 0, 0, command, 40);
    LCD_STATE.AC_adress = adress;
    ESP_LOGI("LCD", "AC adress = %d\n", adress);

    return 0;
}

//0 0 1 DL N F - -
static void LCD_function_set_cmd(const LCD_Pinout *LCD_CONFIG, LCD_DataLength DL, LCD_LineNumber N, LCD_FontSize F){
    uint8_t mask = 0;
    uint8_t command = 0b00100000;

    mask |= (DL << 4);
    mask |= (N << 3);
    mask |= (F << 2);

    command |= mask;
    LCD_send_command(LCD_CONFIG, 0, 0, command, 40);

    LCD_STATE.data_length = DL;
    LCD_STATE.line_number = N;
    LCD_STATE.font_size = F;

}

// 0 0 0 0 1 display cursor cursor_blinking
static void LCD_display_control_cmd(const LCD_Pinout *LCD_CONFIG, bool display, bool cursor, bool cursor_blinking){
    uint8_t command = 0b00001000;
    uint8_t mask = 0;

    mask |= (int)display << 2;
    mask |= (int)cursor << 1;
    mask |= (int)cursor_blinking;
    
    command |= mask;
    LCD_send_command(LCD_CONFIG, 0, 0, command, 40);
    
    LCD_STATE.display = display;
    LCD_STATE.cursor = cursor;
    LCD_STATE.cursor_blinking = cursor_blinking;
}

// 0 0 0 0 0 1 I/D SH
static void LCD_entry_mode_cmd(const LCD_Pinout *LCD_CONFIG, LCD_Direction cursor_direction, bool entire_display_shift){
    uint8_t command = 0b00000100;
    uint8_t mask = 0;

    mask |= cursor_direction << 1;
    mask |= entire_display_shift;

    command |= mask;
    LCD_send_command(LCD_CONFIG, 0, 0, command, 40);
    
    LCD_STATE.cursor_direction = cursor_direction;
    LCD_STATE.entire_display_shift = entire_display_shift;
}

//0 0 0 1 S/C R/L X X
static void LCD_shift_cmd(const LCD_Pinout *LCD_CONFIG, LCD_ShiftControl shift_control, LCD_Direction direction){
    uint8_t command = 0b00010000;
    uint8_t mask = 0;

    mask |= shift_control << 3;
    mask |= direction << 2;

    command |= mask;
    LCD_send_command(LCD_CONFIG, 0, 0, command, 40);
}

//user API

//refonte du système de mémoire de l'état du LCD (LCD_STATE) avec Enums utilisés par les fonctions commandesf
/*
void LCD_data_lenght(const LCD_Pinout *LCD_CONFIG, LCD_DataLength data_lenght){
    LCD_LineNumber line_number_state = LCD_STATE.function_set
    LCD_function_set_cmd(LCD_CONFIG, data_lenght,)
}*/

void LCD_clear_display(const LCD_Pinout *LCD_CONFIG){
    LCD_send_command(LCD_CONFIG, 0, 0, 0b00000001, 1550);
}

void LCD_shift_display(const LCD_Pinout *LCD_CONFIG, LCD_Direction direction){
    LCD_shift_cmd(LCD_CONFIG, LCD_DISPLAY, direction);
}

void LCD_shift_cursor(const LCD_Pinout *LCD_CONFIG, LCD_Direction direction){
    LCD_shift_cmd(LCD_CONFIG, LCD_CURSOR, direction);
}

void LCD_print_char(const LCD_Pinout *LCD_CONFIG, char letter){
    uint8_t letter_ascii_nb = (unsigned char)letter;
    LCD_send_command(LCD_CONFIG, 1, 0, letter_ascii_nb, 45);
}

int LCD_set_cursor(const LCD_Pinout *LCD_CONFIG, int row, int column){
    if(row > 2 || row < 1 || column > 16 || column < 1) { return 1; }
    uint8_t AC_adress;

    if (row == 1){
        AC_adress = column - 1;
    }
    else {
        AC_adress = (DDRAM_SECOND_LIGN_STARTING_ADRESS + column) - 1;
    }

    return LCD_set_AC_cmd(LCD_CONFIG, AC_adress);
}

void LCD_cursor_auto_direction(const LCD_Pinout *LCD_CONFIG, LCD_Direction direction){
    LCD_entry_mode_cmd(LCD_CONFIG, direction, LCD_STATE.entire_display_shift);
}

void LCD_display(const LCD_Pinout *LCD_CONFIG, bool display){
    LCD_display_control_cmd(LCD_CONFIG, display, LCD_STATE.cursor, LCD_STATE.cursor_blinking);
}

void LCD_cursor(const LCD_Pinout *LCD_CONFIG, bool cursor){
    LCD_display_control_cmd(LCD_CONFIG, LCD_STATE.display, cursor, LCD_STATE.cursor_blinking);
}

void LCD_cursor_blink(const LCD_Pinout *LCD_CONFIG, bool cursor_mode){
    LCD_display_control_cmd(LCD_CONFIG, LCD_STATE.display, LCD_STATE.cursor, cursor_mode);
}

int LCD_print(const LCD_Pinout *LCD_CONFIG, char *buffer){
    uint8_t buffer_size;
    bool string_end_found = false;

    for(int i = 0; i < 16; i++){
        if(*(buffer + i) == '\0'){
            buffer_size = i;
            string_end_found = true;
            break;
        }
    }

    if(!string_end_found){
        ESP_LOGI("LCD", "string too long passed in LCD_print, 16chars max");
        return 1;
    }

    for(int i = 0; i < buffer_size; i++){
        LCD_print_char(LCD_CONFIG, *(buffer + i));
    }
    
    return 0;
}

void LCD_init(const LCD_Pinout *LCD_CONFIG, LCD_DataLength DL){
    uint8_t DB_nb;
    
    if(DL == LCD_4BIT) DB_nb = 4;
    else DB_nb = 8;
    
    LCD_STATE.data_length = DL;

    ESP_LOGI("LCD", "E=%d RS=%d RW=%d", LCD_CONFIG -> E, LCD_CONFIG -> RS, LCD_CONFIG -> RW);
    for(int i = 0; i < DB_nb; i++){
        ESP_LOGI("LCD", "DB[%d]=%d", i, LCD_CONFIG -> DB[i]);
    }

    LCD_setup(LCD_CONFIG, DB_nb);
    
    esp_rom_delay_us(15000); //power on delay (délai matériel trop long)

    gpio_set_level(LCD_CONFIG -> RS, 0);
    gpio_set_level(LCD_CONFIG -> RW, 0);

    LCD_send_nibble(LCD_CONFIG, 0b0011); //make sure 8bit mode is set
    esp_rom_delay_us(4100); //délai matériel trop long
    LCD_send_nibble(LCD_CONFIG, 0b0011);
    esp_rom_delay_us(100);
    LCD_send_nibble(LCD_CONFIG, 0b0011);
    esp_rom_delay_us(40);

    LCD_4bit_mode_nibble_cmd(LCD_CONFIG);
    LCD_function_set_cmd(LCD_CONFIG, LCD_4BIT, LCD_LINE_2, LCD_FONT_5X8);
    LCD_display_control_cmd(LCD_CONFIG, false, true, false);
    LCD_clear_display(LCD_CONFIG);
    LCD_entry_mode_cmd(LCD_CONFIG, LCD_RIGHT, false);
    LCD_display_control_cmd(LCD_CONFIG, true, true, false);
}

//renamed functions, fixed LCD_print_char timing issue, fixed wrong file name in CMakeLists.txt, added LCD commands as functions
//updated API structure with LCD_PINOUT struct to support multiple displays

//GIT
//librairie
//readme file
//pilote propre
//shell lcd :)

//fonctions commandes mais je devrais avoir des fonctions enable très simple pour l'utilisateur
//délais matériels trop longs (downtime)