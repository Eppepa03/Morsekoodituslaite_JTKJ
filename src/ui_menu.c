#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tkjhat/ssd1306.h"
#include "icons.h"
#include "ui_menu.h"

#define FONT_W 8
#define FONT_H 8
#define LINE_H 16

// Päävalikko
static const char* main_items[] = {"Connect", "Setup", "Shutdown"};
static const int main_count = 3;

// Connect-valikko
static const char* connect_items[] = {"USB", "Wireless"};
static const int connect_count = 2;

// USB-valikko
static const char* usb_items[] = {"Send", "Receive"};
static const int usb_count = 2;

// Shutdown-valikko
static const char* confirm_items[] = {"Yes", "No"};
static const int confirm_count = 2;

// Tilamuuttujat
static volatile ui_state_t g_state = UI_STATE_MAIN_MENU;
static volatile int sel_main = 0;
static volatile int sel_connect = 0;
static volatile int sel_usb = 0;
static volatile int sel_confirm = 1;
static bool need_redraw = true;

static ssd1306_t* g_disp = NULL;
static ui_menu_callbacks_t g_cbs = {0};

static bool g_scroll_active_low = true;
static bool g_select_active_low = true;

// ---------- Invert-apurit ----------
static void ssd1306_invert_rect(ssd1306_t *p, int x, int y, int w, int h){
    if (!p || !p->buffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > p->width)  w = p->width  - x;
    if (y + h > p->height) h = p->height - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = 0; yy < h; yy++){
        int py = y + yy;
        size_t row = (py >> 3) * p->width;
        uint8_t mask = 1u << (py & 7);
        size_t base = row + x;
        for (int xx = 0; xx < w; xx++){
            p->buffer[base + xx] ^= mask;
        }
    }
}

// ---------- Perusgrafiikka ----------
static inline void putp(ssd1306_t *p, int x, int y, uint8_t c) {
    if (c) ssd1306_draw_pixel(p, (uint32_t)x, (uint32_t)y);
    else   ssd1306_clear_pixel(p, (uint32_t)x, (uint32_t)y);
}
static void draw_hline(ssd1306_t *p, int x, int y, int w, uint8_t c) {
    for (int i = 0; i < w; i++) putp(p, x + i, y, c);
}
static void draw_filled_rect(ssd1306_t *p, int x, int y, int w, int h, uint8_t c) {
    for (int j = 0; j < h; j++) draw_hline(p, x, y + j, w, c);
}

static inline void ssd1306_draw_bitmap_1bpp(ssd1306_t *d,int x,int y,int w,int h,const uint8_t *bits){
    int stride = (w + 7) >> 3;
    for (int yy = 0; yy < h; yy++) {
        const uint8_t *row = bits + yy * stride;
        int xx = 0;
        for (int b = 0; b < stride; b++) {
            uint8_t byte = row[b];
            for (int k = 7; k >= 0; k--) {
                if (xx >= w) break;
                if ((byte >> k) & 1u) ssd1306_draw_pixel(d, (uint32_t)(x+xx), (uint32_t)(y+yy));
                xx++;
            }
        }
    }
}

static void draw_menu_icon(ssd1306_t* d, int x, int y, int idx) {
    const uint8_t* bmp = (idx == 0) ? ICON_CONNECT : (idx == 1) ? ICON_SETUP : ICON_POWER;
    ssd1306_draw_bitmap_1bpp(d, x, y, ICON_W, ICON_H, bmp);
}

static inline int text_chars(const char *s){ int n=0; while (s && s[n]) n++; return n; }
static inline int text_pixel_width_chars(int chars, int scale){ return chars * FONT_W * scale; }
static inline int center_x_for(const char *s, int scale, int screen_w){
    int w = text_pixel_width_chars(text_chars(s), scale);
    int x = (screen_w - w) / 2; return x < 0 ? 0 : x;
}

// ---------- Nappiapu ----------
static inline bool btn_pressed_with_pol(uint pin, bool active_low){
    bool l = gpio_get(pin);
    return active_low ? (l == 0) : (l == 1);
}
static bool read_button_press_pin(uint pin, bool active_low){
    if (btn_pressed_with_pol(pin, active_low)) {
        vTaskDelay(pdMS_TO_TICKS(UI_DEBOUNCE_MS));
        if (btn_pressed_with_pol(pin, active_low)) {
            while (btn_pressed_with_pol(pin, active_low)) vTaskDelay(pdMS_TO_TICKS(1));
            return true;
        }
    }
    return false;
}
static int poll_scroll_click_event(void){
    static bool last_read_pressed = false;
    static bool stable_pressed = false;
    static uint32_t last_edge_ms = 0;
    static uint32_t last_release_ms = 0;
    static int click_count = 0;

    int event = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool pressed_now = btn_pressed_with_pol(UI_BTN_SCROLL_PIN, g_scroll_active_low);

    if (pressed_now != last_read_pressed) { last_read_pressed = pressed_now; last_edge_ms = now; }
    if ((now - last_edge_ms) >= UI_DEBOUNCE_MS && pressed_now != stable_pressed) {
        stable_pressed = pressed_now;
        if (!stable_pressed) { click_count++; last_release_ms = now; }
    }
    if (click_count >= 2) { event = 2; click_count = 0; }
    else if (click_count == 1) {
        if ((now - last_release_ms) > UI_DOUBLE_CLICK_MS) { event = 1; click_count = 0; }
    }
    return event;
}

// ---------- Piirto ----------
static void draw_header(ssd1306_t *disp, const char *txt){
    int x = center_x_for(txt, 1, UI_OLED_W);
    ssd1306_draw_string(disp, (uint32_t)x, 3, 1, txt);
    ssd1306_invert_rect(disp, 0, 0, UI_OLED_W, 14);
}

static void draw_ui(ssd1306_t *disp){
    ssd1306_clear(disp);

    if (g_state == UI_STATE_MAIN_MENU) {
        draw_header(disp, "MorseMaster9000");
        for (int i = 0; i < main_count; i++) {
            int y = 16 + i * LINE_H;
            draw_menu_icon(disp, 4, y, i);
            ssd1306_draw_string(disp, 28, y + 4, 1, main_items[i]);
            if (i == sel_main) ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H);
        }

    } else if (g_state == UI_STATE_CONNECT_MENU) {
        draw_header(disp, "Connect");
        for (int i = 0; i < connect_count; i++) {
            int y = 16 + i * LINE_H;
            ssd1306_draw_string(disp, 28, y + 4, 1, connect_items[i]);
            if (i == sel_connect) ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H);
        }

    } else if (g_state == UI_STATE_USB_MENU) {
        draw_header(disp, "USB");
        for (int i = 0; i < usb_count; i++) {
            int y = 16 + i * LINE_H;
            ssd1306_draw_string(disp, 28, y + 4, 1, usb_items[i]);
            if (i == sel_usb) ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H);
        }

    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        ssd1306_draw_string(disp, 10, 10, 1, "Are you sure?");
        int start_y = 35;
        for (int i = 0; i < confirm_count; i++) {
            int y = start_y + i * 12;
            ssd1306_draw_string(disp, 20, y, 1, confirm_items[i]);
            if (i == sel_confirm) ssd1306_invert_rect(disp, 18, y - 2, 40, 12);
        }
    }

    ssd1306_show(disp);
}

// --- KORJATTU FUNKTIO: Automaattinen skaalaus ---
static void show_selection_and_return(ssd1306_t *disp, const char *msg, ui_state_t ret){
    ssd1306_clear(disp);
    
    int len = text_chars(msg);
    int scale = 2; // Oletus: iso fontti
    
    // Jos teksti on liian leveä isolle fontille (yli 8 merkkiä), vaihda pieneen fonttiin
    if (text_pixel_width_chars(len, scale) > UI_OLED_W) {
        scale = 1;
    }

    int w = text_pixel_width_chars(len, scale);
    int x = (UI_OLED_W - w) / 2; if (x < 0) x = 0;
    int y = (UI_OLED_H - FONT_H * scale) / 2;
    
    ssd1306_draw_string(disp, (uint32_t)x, (uint32_t)y, scale, msg);
    ssd1306_invert_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H);
    ssd1306_show(disp);
    vTaskDelay(pdMS_TO_TICKS(900));

    g_state = ret;
    ssd1306_clear(disp);
    ssd1306_show(disp);
    need_redraw = true;
}

// --- KORJATTU FUNKTIO: Automaattinen skaalaus ---
static void perform_shutdown(ssd1306_t *disp){
    ssd1306_clear(disp);
    const char* msg = "Shutting down...";
    
    int len = text_chars(msg);
    int scale = 2;
    if (text_pixel_width_chars(len, scale) > UI_OLED_W) {
        scale = 1;
    }

    int w = text_pixel_width_chars(len, scale);
    int x = (UI_OLED_W - w) / 2; if (x < 0) x = 0;
    int y = (UI_OLED_H - FONT_H * scale) / 2;

    ssd1306_draw_string(disp, (uint32_t)x, (uint32_t)y, scale, msg);
    ssd1306_invert_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H);
    ssd1306_show(disp);
    
    if (g_cbs.on_shutdown) g_cbs.on_shutdown();
}

// ---------- Julkiset ----------
void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs){
    g_disp = disp;
    if (cbs) g_cbs = *cbs;

    gpio_init(UI_BTN_SCROLL_PIN);
    gpio_set_function(UI_BTN_SCROLL_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UI_BTN_SCROLL_PIN, GPIO_IN);

    gpio_init(UI_BTN_SELECT_PIN);
    gpio_set_function(UI_BTN_SELECT_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UI_BTN_SELECT_PIN, GPIO_IN);

#if UI_DETECT_BUTTON_POLARITY
#ifndef UI_SCROLL_ACTIVE_LOW
    gpio_disable_pulls(UI_BTN_SCROLL_PIN); vTaskDelay(pdMS_TO_TICKS(2));
    g_scroll_active_low = gpio_get(UI_BTN_SCROLL_PIN);
#endif
#ifndef UI_SELECT_ACTIVE_LOW
    gpio_disable_pulls(UI_BTN_SELECT_PIN); vTaskDelay(pdMS_TO_TICKS(2));
    g_select_active_low = gpio_get(UI_BTN_SELECT_PIN);
#endif
#endif

    if (g_scroll_active_low) gpio_pull_up(UI_BTN_SCROLL_PIN); else gpio_pull_down(UI_BTN_SCROLL_PIN);
    if (g_select_active_low) gpio_pull_up(UI_BTN_SELECT_PIN); else gpio_pull_down(UI_BTN_SELECT_PIN);

    g_state = UI_STATE_MAIN_MENU;
    sel_main = 0; sel_connect = 0; sel_usb = 0; sel_confirm = 1; need_redraw = true;
}

void ui_menu_poll(void){
    if (!g_disp) return;

    ui_state_t state_before = g_state;
    int main_before = sel_main;
    int confirm_before = sel_confirm;
    int connect_before = sel_connect;
    int usb_before = sel_usb;

    int scroll_evt = poll_scroll_click_event();

    if (g_state == UI_STATE_MAIN_MENU) {
        if (scroll_evt == 1) sel_main = (sel_main + 1) % main_count;
        if (read_button_press_pin(UI_BTN_SELECT_PIN, g_select_active_low)) {
            if (sel_main == 0) { g_state = UI_STATE_CONNECT_MENU; sel_connect = 0; }
            else if (sel_main == 2) { g_state = UI_STATE_CONFIRM_SHUTDOWN; sel_confirm = 1; }
            else { show_selection_and_return(g_disp, "Setup selected", UI_STATE_MAIN_MENU); }
        }

    } else if (g_state == UI_STATE_CONNECT_MENU) {
        if (scroll_evt == 2) g_state = UI_STATE_MAIN_MENU;
        else if (scroll_evt == 1) sel_connect = (sel_connect + 1) % connect_count;

        if (read_button_press_pin(UI_BTN_SELECT_PIN, g_select_active_low)) {
            if (sel_connect == 0) { 
                g_state = UI_STATE_USB_MENU; 
                sel_usb = 0; 
            }
            else { 
                if (g_cbs.on_connect_wireless) g_cbs.on_connect_wireless(); 
                show_selection_and_return(g_disp, "Coming soon!", UI_STATE_MAIN_MENU); 
            }
        }
    
    } else if (g_state == UI_STATE_USB_MENU) {
        if (scroll_evt == 2) g_state = UI_STATE_CONNECT_MENU;
        else if (scroll_evt == 1) sel_usb = (sel_usb + 1) % usb_count;

        if (read_button_press_pin(UI_BTN_SELECT_PIN, g_select_active_low)) {
            if (sel_usb == 0) {
                if (g_cbs.on_usb_send) g_cbs.on_usb_send();
                show_selection_and_return(g_disp, "Sending...", UI_STATE_MAIN_MENU);
            } else {
                if (g_cbs.on_usb_receive) g_cbs.on_usb_receive();
                show_selection_and_return(g_disp, "Receiving...", UI_STATE_MAIN_MENU);
            }
        }

    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        if (scroll_evt == 2) g_state = UI_STATE_MAIN_MENU;
        else if (scroll_evt == 1) sel_confirm = (sel_confirm + 1) % confirm_count;
        if (read_button_press_pin(UI_BTN_SELECT_PIN, g_select_active_low)) {
            if (sel_confirm == 0) perform_shutdown(g_disp);
            else g_state = UI_STATE_MAIN_MENU;
        }
    }

    if (need_redraw ||
        state_before != g_state ||
        main_before != sel_main ||
        confirm_before != sel_confirm ||
        connect_before != sel_connect ||
        usb_before != sel_usb) {
        draw_ui(g_disp);
        need_redraw = false;
    }
}

void ui_menu_force_redraw(void){ need_redraw = true; }
ui_state_t ui_menu_get_state(void){ return g_state; }
int ui_menu_get_main_selection(void){ return sel_main; }
int ui_menu_get_connect_selection(void){ return sel_connect; }
int ui_menu_get_usb_selection(void){ return sel_usb; }
int ui_menu_get_confirm_selection(void){ return sel_confirm; }