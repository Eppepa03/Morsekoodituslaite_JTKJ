#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ssd1306.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UI_I2C_PORT_IS_I2C0
#define UI_I2C_PORT_IS_I2C0 1
#endif

#ifndef UI_I2C_ADDR
#define UI_I2C_ADDR 0x3C
#endif

#ifndef UI_OLED_W
#define UI_OLED_W 128
#endif

#ifndef UI_OLED_H
#define UI_OLED_H 64
#endif

#ifndef UI_BTN_SCROLL_PIN
#define UI_BTN_SCROLL_PIN 6
#endif

#ifndef UI_BTN_SELECT_PIN
#define UI_BTN_SELECT_PIN 7
#endif

#ifndef UI_DEBOUNCE_MS
#define UI_DEBOUNCE_MS 50
#endif

#ifndef UI_DOUBLE_CLICK_MS
#define UI_DOUBLE_CLICK_MS 350
#endif

typedef enum {
  UI_STATE_MAIN_MENU = 0,
  UI_STATE_CONNECT_MENU,
  UI_STATE_CONFIRM_SHUTDOWN
} ui_state_t;

typedef struct {
  void (*on_connect_usb)(void);
  void (*on_connect_wireless)(void);
  void (*on_shutdown)(void);
} ui_menu_callbacks_t;

void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs);
void ui_menu_poll(void);
void ui_menu_force_redraw(void);
ui_state_t ui_menu_get_state(void);
int ui_menu_get_main_selection(void);
int ui_menu_get_connect_selection(void);
int ui_menu_get_confirm_selection(void);

#ifdef __cplusplus
}
#endif
