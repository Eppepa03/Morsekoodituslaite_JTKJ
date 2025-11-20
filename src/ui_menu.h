#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "tkjhat/ssd1306.h"
#include "event.h" // ui_cmd_t

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UI_OLED_W
#define UI_OLED_W 128
#endif

#ifndef UI_OLED_H
#define UI_OLED_H 64
#endif

// ... (Poistettu PIN-määritykset ja makrot, koska button_task hoitaa ne) ...

typedef enum {
  UI_STATE_MAIN_MENU = 0,
  UI_STATE_CONNECT_MENU,
  UI_STATE_USB_MENU,
  UI_STATE_CONFIRM_SHUTDOWN
} ui_state_t;

typedef struct {
  void (*on_connect_wireless)(void);
  void (*on_usb_send)(void);    // UUSI: Send
  void (*on_usb_receive)(void); // UUSI: Receive
  void (*on_shutdown)(void);
} ui_menu_callbacks_t;

// Init ei ota enää pinnejä
void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs);

// Korvaa ui_menu_poll funktion tällä:
void ui_menu_process_cmd(ui_cmd_t cmd);

void ui_menu_force_redraw(void);

ui_state_t ui_menu_get_state(void);
int ui_menu_get_main_selection(void);
int ui_menu_get_connect_selection(void);
int ui_menu_get_usb_selection(void);
int ui_menu_get_confirm_selection(void);

#ifdef __cplusplus
}
#endif