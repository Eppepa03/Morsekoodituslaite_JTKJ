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

typedef enum {
    UI_STATE_MAIN_MENU = 0,
    UI_STATE_CONNECT_MENU,
    UI_STATE_SETUP_MENU,        // UUSI: Setup-päävalikko
    UI_STATE_ORIENT_MENU,       // UUSI: Orientation-alavalikko
    UI_STATE_USB_MENU,
    UI_STATE_CONFIRM_SHUTDOWN
} ui_state_t;

typedef struct {
    void (*on_connect_wireless)(void);
    void (*on_usb_send)(void);
    void (*on_usb_receive)(void);
    
    // UUDET CALLBACKIT
    void (*on_orient_normal)(void);
    void (*on_orient_flipped)(void);
    
    void (*on_shutdown)(void);
} ui_menu_callbacks_t;

void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs);
void ui_menu_process_cmd(ui_cmd_t cmd);
void ui_menu_force_redraw(void);

ui_state_t ui_menu_get_state(void);
int ui_menu_get_main_selection(void);
int ui_menu_get_connect_selection(void);
int ui_menu_get_setup_selection(void); // UUSI
int ui_menu_get_orient_selection(void); // UUSI
int ui_menu_get_usb_selection(void);
int ui_menu_get_confirm_selection(void);

#ifdef __cplusplus
}
#endif