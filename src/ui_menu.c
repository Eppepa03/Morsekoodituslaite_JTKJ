#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "tkjhat/ssd1306.h"
#include "ui_menu.h"
#include "icons.h"
#include "FreeRTOS.h"
#include "tkjhat/sdk.h"
#include "state_machine.h"

#define FONT_W 8
#define FONT_H 8
#define LINE_H 16

// Päävalikko
static const char* main_items[] = {"Connect", "Setup", "Shutdown"};
static const int main_count = 3;

// Connect-valikko
static const char* connect_items[] = {"USB", "Wireless"};
static const int connect_count = 2;

// Setup-valikko
// "Back" poistettu, teksti pidennetty.
static const char* setup_items[] = {"Screen Orientation"}; 
static const int setup_count = 1;

// Orientation-valikko
static const char* orient_items[] = {"Normal", "Flipped"};
static const int orient_count = 2;

// USB-valikko
static const char* usb_items[] = {"Send", "Receive"};
static const int usb_count = 2;

// Shutdown-valikko
static const char* confirm_items[] = {"Yes", "No"};
static const int confirm_count = 2;

// Tilamuuttujat
static volatile ui_state_t g_state = UI_STATE_IDLE;
static volatile int sel_main = 0;
static volatile int sel_connect = 0;
static volatile int sel_setup = 0;
static volatile int sel_orient = 0;
static volatile int sel_usb = 0;
static volatile int sel_confirm = 1;
static bool need_redraw = true;

static ssd1306_t* g_disp = NULL;
static ui_menu_callbacks_t g_cbs = {0};

// USB Receive Buffer
static char rx_buffer[128] = ""; // Simple buffer for received text

void ui_menu_add_rx_string(const char *str) {
    printf("Recieved: %s\r\n", str);
    size_t len = strlen(rx_buffer);
    size_t add_len = strlen(str);
    if (len + add_len < sizeof(rx_buffer) - 1) {
        strncat(rx_buffer, str, sizeof(rx_buffer) - strlen(rx_buffer) - 1);
    } else {
        // Shift left to make room
        size_t shift = (len + add_len) - (sizeof(rx_buffer) - 1);
        memmove(rx_buffer, rx_buffer + shift, len - shift);
        strncat(rx_buffer, str, sizeof(rx_buffer) - strlen(rx_buffer) - 1);
    }
    need_redraw = true;
}


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

// ---------- Grafiikka ----------
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
            if (i == sel_main) { 
                ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H); 
            }
        }

    } else if (g_state == UI_STATE_CONNECT_MENU) {
        draw_header(disp, "Connect");
        for (int i = 0; i < connect_count; i++) {
            int y = 16 + i * LINE_H;
            ssd1306_draw_string(disp, 28, y + 4, 1, connect_items[i]);
            if (i == sel_connect) { 
                ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H); 
            }
        }

    } else if (g_state == UI_STATE_SETUP_MENU) {
        draw_header(disp, "Setup");
        for (int i = 0; i < setup_count; i++) {
            int y = 16 + i * LINE_H;
            // PIIRRETÄÄN X=2 ETTÄ PITKÄ TEKSTI MAHTUU
            ssd1306_draw_string(disp, 2, y + 4, 1, setup_items[i]);
            if (i == sel_setup) { 
                ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H); 
            }
        }

    } else if (g_state == UI_STATE_ORIENT_MENU) {
        draw_header(disp, "Orientation");
        for (int i = 0; i < orient_count; i++) {
            int y = 16 + i * LINE_H;
            ssd1306_draw_string(disp, 28, y + 4, 1, orient_items[i]);
            if (i == sel_orient) { 
                ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H); 
            }
        }

    } else if (g_state == UI_STATE_USB_MENU) {
        draw_header(disp, "USB");
        for (int i = 0; i < usb_count; i++) {
            int y = 16 + i * LINE_H;
            ssd1306_draw_string(disp, 28, y + 4, 1, usb_items[i]);
            if (i == sel_usb) { 
                ssd1306_invert_rect(disp, 0, y, UI_OLED_W, LINE_H); 
            }
        }

    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        ssd1306_draw_string(disp, 10, 10, 1, "Are you sure?");
        int start_y = 35;
        for (int i = 0; i < confirm_count; i++) {
            int y = start_y + i * 12;
            ssd1306_draw_string(disp, 20, y, 1, confirm_items[i]);
            if (i == sel_confirm) { 
                ssd1306_invert_rect(disp, 18, y - 2, 40, 12); 
            }
        }
    } else if (g_state == UI_STATE_USB_RECEIVING) {
        draw_header(disp, "Receiving...");
        // Draw the received text. 
        // Simple implementation: draw as much as fits.
        // For better multiline support, we'd need more logic, but this is a start.
        int y = 20;
        int x = 2;
        int max_chars_per_line = UI_OLED_W / FONT_W;
        
        // Draw buffer content wrapped
        int len = strlen(rx_buffer);
        int drawn = 0;
        while (drawn < len && y < UI_OLED_H) {
            char line[20] = "";
            int chunk = len - drawn;
            if (chunk > max_chars_per_line) chunk = max_chars_per_line + 1;
            
            strncpy(line, rx_buffer + drawn, chunk);
            line[chunk] = '\0';
            
            ssd1306_draw_string(disp, x, y, 1, line);
            y += LINE_H;
            drawn += chunk;
        }
    }

    ssd1306_show(disp);
}

static void show_selection(ssd1306_t *disp, const char *msg){
    ssd1306_clear(disp);
    
    int len = text_chars(msg);
    int scale = 2;
    if (text_pixel_width_chars(len, scale) > UI_OLED_W) scale = 1;

    int w = text_pixel_width_chars(len, scale);
    int x = (UI_OLED_W - w) / 2; if (x < 0) x = 0;
    int y = (UI_OLED_H - FONT_H * scale) / 2;
    
    ssd1306_draw_string(disp, (uint32_t)x, (uint32_t)y, scale, msg);
    ssd1306_invert_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H);
    ssd1306_show(disp);
}

static void perform_shutdown(ssd1306_t *disp){
    ssd1306_clear(disp);
    ssd1306_draw_string(disp, 10, 28, 1, "Shutting down...");
    ssd1306_invert_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H);
    ssd1306_show(disp);
    if (g_cbs.on_shutdown) {
        g_cbs.on_shutdown(); 
    }
}

// ---------- Julkiset ----------
void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs){
    if (cbs) { 
        g_cbs = *cbs; 
    }
    g_disp = disp;
    g_state = UI_STATE_IDLE;
}

void ui_wakeup(ssd1306_t* disp) {
    g_state = UI_STATE_MAIN_MENU;

    sel_main = 0; 
    sel_connect = 0; 
    sel_setup = 0; 
    sel_orient = 0;
    sel_usb = 0; 
    sel_confirm = 1;
    need_redraw = true;
}

void ui_menu_process_cmd(ui_cmd_t cmd){
    if (!g_disp) { 
        return; 
    }

    ui_state_t state_before = g_state;
    int main_before = sel_main;
    int setup_before = sel_setup;
    int orient_before = sel_orient;
    int confirm_before = sel_confirm;
    int connect_before = sel_connect;
    int usb_before = sel_usb;

    if (g_state == UI_STATE_IDLE) {
        if (cmd == UI_CMD_SELECT) {
            // Wake up
            if (g_cbs.on_wake_up) {
                g_cbs.on_wake_up();
            }
        }
        
    } else if (g_state == UI_STATE_MAIN_MENU) {
        if (cmd == UI_CMD_SCROLL) { 
            sel_main = (sel_main + 1) % main_count; 
        } else if (cmd == UI_CMD_SELECT) {
            if (sel_main == 0) { 
                g_state = UI_STATE_CONNECT_MENU; 
                sel_connect = 0; 
            } else if (sel_main == 1) { 
                g_state = UI_STATE_SETUP_MENU; 
                sel_setup = 0; 
            } else if (sel_main == 2) { 
                g_state = UI_STATE_CONFIRM_SHUTDOWN; 
                sel_confirm = 1; 
            }
        }

    } else if (g_state == UI_STATE_CONNECT_MENU) {

        if (cmd == UI_CMD_SCROLL_BACK) { 
            g_state = UI_STATE_MAIN_MENU; 
        } else if (cmd == UI_CMD_SCROLL) { 
            sel_connect = (sel_connect + 1) % connect_count; 
        } else if (cmd == UI_CMD_SELECT) {
            if (sel_connect == 0) { 
                g_state = UI_STATE_USB_MENU; 
                if (g_cbs.on_connect_usb) { 
                    g_cbs.on_connect_usb(); // Change main state to STATE_USB_CONNECTED
                }
                sel_usb = 0; 
            } else { 
                // Tämän voisi vielä muokata omakseen, kuten UI_STATE_USB_MENU
                if (g_cbs.on_connect_wireless) { 
                    g_cbs.on_connect_wireless(); 
                }
                show_selection(g_disp, "Coming soon!"); 
            }
        }
    
    } else if (g_state == UI_STATE_SETUP_MENU) { 
        if (cmd == UI_CMD_SCROLL_BACK) { 
            g_state = UI_STATE_MAIN_MENU;
        } else if (cmd == UI_CMD_SCROLL) { 
            sel_setup = (sel_setup + 1) % setup_count; 
        } else if (cmd == UI_CMD_SELECT) {
            // Ainoa vaihtoehto on Screen Orientation
            g_state = UI_STATE_ORIENT_MENU;
            sel_orient = 0;
        }

    } else if (g_state == UI_STATE_ORIENT_MENU) {
        if (cmd == UI_CMD_SCROLL_BACK) { 
            g_state = UI_STATE_SETUP_MENU; 
        } else if (cmd == UI_CMD_SCROLL) { 
            sel_orient = (sel_orient + 1) % orient_count; 
        } else if (cmd == UI_CMD_SELECT) {
            if (sel_orient == 0) {
                if (g_cbs.on_orient_normal) g_cbs.on_orient_normal();
                show_selection(g_disp, "Set: Normal");
            } else {
                if (g_cbs.on_orient_flipped) g_cbs.on_orient_flipped();
                show_selection(g_disp, "Set: Flipped");
            }
            sleep_ms(50); // Pieni viive, jotta käyttäjä ehtii nähdä vahvistuksen
            g_state = UI_STATE_SETUP_MENU; // Palataan Setuppiin
            need_redraw = true;
        }

    } else if (g_state == UI_STATE_USB_MENU) {
        if (cmd == UI_CMD_SCROLL_BACK) { 
            g_state = UI_STATE_CONNECT_MENU;
            if (g_cbs.on_return) {
                g_cbs.on_return(); // Vaihdetaan tilaksi BUS_UI_UPDATE ja STATE_MENU
            }
        } else if (cmd == UI_CMD_SCROLL) { 
            sel_usb = (sel_usb + 1) % usb_count; 
        } else if (cmd == UI_CMD_SELECT) {
            if (sel_usb == 0) {
                show_selection(g_disp, "Sending...");
                if (g_cbs.on_usb_send) { 
                    g_cbs.on_usb_send(); // Change bus state to BUS_READ_SENSOR and main state to STATE_USB_SEND
                }
            } else {
                // show_selection(g_disp, "Receiving...");
                if (g_cbs.on_usb_receive) { 
                    g_cbs.on_usb_receive(); // Change bus state to BUS_UI_UPDATE (or keep it) and main state to STATE_USB_RECEIVE
                }
                g_state = UI_STATE_USB_RECEIVING;
            }
        }

    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        if (cmd == UI_CMD_SCROLL_BACK) { 
            g_state = UI_STATE_MAIN_MENU;
        } else if (cmd == UI_CMD_SCROLL) { 
            sel_confirm = (sel_confirm + 1) % confirm_count; 
        } else if (cmd == UI_CMD_SELECT) {
            if (sel_confirm == 0) { 
                perform_shutdown(g_disp);
                g_state = UI_STATE_IDLE;
            } else { 
                g_state = UI_STATE_MAIN_MENU; 
            }
        }
    } else if (g_state == UI_STATE_USB_RECEIVING) {
        if (cmd == UI_CMD_SCROLL_BACK) {
            g_state = UI_STATE_USB_MENU;
        }
    }

    if (need_redraw ||
        state_before != g_state ||
        main_before != sel_main ||
        confirm_before != sel_confirm ||
        connect_before != sel_connect ||
        setup_before != sel_setup ||
        orient_before != sel_orient ||
        usb_before != sel_usb) {
        
        draw_ui(g_disp);
        need_redraw = false;
    }
}

void ui_menu_force_redraw(void){ need_redraw = true; }
ui_state_t ui_menu_get_state(void){ return g_state; }
int ui_menu_get_main_selection(void){ return sel_main; }
int ui_menu_get_connect_selection(void){ return sel_connect; }
int ui_menu_get_setup_selection(void){ return sel_setup; }
int ui_menu_get_orient_selection(void){ return sel_orient; }
int ui_menu_get_usb_selection(void){ return sel_usb; }
int ui_menu_get_confirm_selection(void){ return sel_confirm; }