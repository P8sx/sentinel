#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <stdio.h>

/* Queue for handling inputs from ISR from Io module*/
void ui_handler_init();
void ui_oled_handling_task(void *pvParameters);
void ui_button_handling_task(void *pvParameters);


#endif