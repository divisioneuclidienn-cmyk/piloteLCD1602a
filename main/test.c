#include <stdio.h>

void LCD_print(char *buffer){
    printf("%s\n", buffer);
}

int main(void){
    int buffer[3] = {1,2,3};
    int *buffer_ptr = &buffer[0];
    int size_buffer = sizeof(buffer);

    char str_buffer[10] = "test";
    char *str_buffer_ptr = &str_buffer[0];

    printf("buffer size = %d\n", size_buffer);
    printf("buffer memory adress = %p\n", buffer_ptr);
    printf("void ptr, buffer memory adress = %p\n", (void *)buffer_ptr);

    printf("\n");

    printf("%d\n", *buffer_ptr);
    printf("%d\n", *(buffer_ptr + 1));
    printf("%d\n", *(buffer_ptr + 2));
    printf("%d\n", *(buffer_ptr + 3));

    printf("%s\n", str_buffer_ptr);

    printf("%c\n", *str_buffer_ptr[1]);
    
}