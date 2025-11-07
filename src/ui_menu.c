#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "icons.h"
#include "ui_menu.h"

// Fontin mitat
#define FONT_W 8
#define FONT_H 8

// Valikon kohteet
static const char* main_items[] = {"Connect", "Setup", "Shutdown"};
static const int main_count = 3;

static const char* connect_items[] = {"USB", "Wireless"};
static const int connect_count = 2;

static const char* confirm_items[] = {"Yes", "No"};
static const int confirm_count = 2;

// Nykyinen tila ja valinnat
static volatile ui_state_t g_state = UI_STATE_MAIN_MENU;
static volatile int sel_main = 0;
static volatile int sel_connect = 0;
static volatile int sel_confirm = 1;
static bool need_redraw = true;

static ssd1306_t* g_disp = NULL;
static ui_menu_callbacks_t g_cbs = {0};

// I2C-portin valinta
#if UI_I2C_PORT_IS_I2C0
#define UI_I2C i2c0
#else
#define UI_I2C i2c1
#endif

// --- Apufunktiot grafiikkaan ---

static inline void putp(ssd1306_t *p, int x, int y, uint8_t c) {
    ssd1306_draw_pixel(p, x, y, c);
}

static void draw_hline(ssd1306_t *p, int x, int y, int w, uint8_t c) {
    for (int i = 0; i < w; i++) putp(p, x + i, y, c);
}

static void draw_filled_rect(ssd1306_t *p, int x, int y, int w, int h, uint8_t c) {
    for (int j = 0; j < h; j++) draw_hline(p, x, y + j, w, c);
}

// 1-bittinen bitmap-piirto
typedef enum { BM_TRANSPARENT_BG = 0, BM_DRAW_BG = 1 } bm_bg_mode_t;

static inline void ssd1306_draw_bitmap_1bpp(ssd1306_t *d,
                                            int x, int y, int w, int h,
                                            const uint8_t *bits,
                                            uint8_t fg, uint8_t bg,
                                            bm_bg_mode_t mode) {
    int stride = (w + 7) >> 3;
    for (int yy = 0; yy < h; yy++) {
        const uint8_t *row = bits + yy * stride;
        int xx = 0;
        for (int b = 0; b < stride; b++) {
            uint8_t byte = row[b];
            for (int k = 7; k >= 0; k--) {
                if (xx >= w) break;
                int px = x + xx;
                int py = y + yy;
                uint8_t bit = (byte >> k) & 1u;
                if (bit) {
                    ssd1306_draw_pixel(d, px, py, fg);
                } else if (mode == BM_DRAW_BG) {
                    ssd1306_draw_pixel(d, px, py, bg);
                }
                xx++;
            }
        }
    }
}

static void draw_menu_icon(ssd1306_t* d, int x, int y, int idx, bool selected) {
    const uint8_t* bmp =
        (idx == 0) ? ICON_CONNECT :
        (idx == 1) ? ICON_SETUP :
                     ICON_POWER;
    uint8_t fg = selected ? 0 : 1;
    uint8_t bg = selected ? 1 : 0;
    bm_bg_mode_t mode = selected ? BM_DRAW_BG : BM_TRANSPARENT_BG;
    ssd1306_draw_bitmap_1bpp(d, x, y, ICON_W, ICON_H, bmp, fg, bg, mode);
}

// Tekstiapuja
static inline int text_chars(const char *s) {
    int n = 0;
    while (s && s[n])
        n++;
    return n;
}

static inline int text_pixel_width_chars(int chars, int scale) {
    return chars * FONT_W * scale;
}

static inline int pick_scale_to_fit(const char *s, int max_scale, int max_w) {
    int sc = max_scale;
    while (sc > 1 && text_pixel_width_chars(text_chars(s), sc) > max_w)
        sc--;
    if (text_pixel_width_chars(text_chars(s), sc) > max_w)
        sc = 1;
    return sc;
}

static void draw_centered_string_scaled(ssd1306_t *disp, int y, int scale, const char *txt, uint8_t color) {
    int w = text_pixel_width_chars(text_chars(txt), scale);
    int x = (UI_OLED_W - w) / 2;
    if (x < 0)
        x = 0;
    ssd1306_draw_string(disp, x, y, scale, txt, color);
}

static void draw_header(ssd1306_t *disp, const char *txt) {
    draw_filled_rect(disp, 0, 0, UI_OLED_W, 14, 1);
    int scale = pick_scale_to_fit(txt, 1, UI_OLED_W -4);
    draw_centered_string_scaled(disp, 3, scale, txt, 0);
}

static int wrap_text_lines(const char *s, int scale, int max_w,
                           char lines[][32], int max_lines, int *out_longest_chars) {
    int max_chars = max_w / (FONT_W * scale);
    if (max_chars < 1)
        return 0;
    int line_idx = 0, line_len = 0, longest = 0;
    lines[0][0] = '\0';
    const char *p = s;
    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        const char *word = p;
        int wlen = 0;
        while (word[wlen] && word[wlen] != ' ')
            wlen++;
        if (wlen > max_chars)
            return 0;

        int need = wlen + (line_len > 0 ? 1 : 0);
        if (line_len + need > max_chars) {
            if (++line_idx >= max_lines)
                return 0;
            line_len = 0;
            lines[line_idx][0] = '\0';
        }

        int pos = line_len;
        if (line_len > 0) {
            lines[line_idx][pos++] = ' ';
            line_len++;
        }
        for (int i = 0; i < wlen && pos < 31; i++)
            lines[line_idx][pos++] = word[i];
        lines[line_idx][pos] = '\0';
        line_len += wlen;
        if (line_len > longest)
            longest = line_len;
        p += wlen;
        while (*p == ' ')
            p++;
    }
    *out_longest_chars = longest;
    return (lines[line_idx][0] != '\0') ? (line_idx + 1) : line_idx;
}

static void draw_centered_wrapped(ssd1306_t *disp, const char *txt, int max_scale, int max_w, uint8_t color) {
    int scale = max_scale;
    char lines[3][32];
    int longest = 0;
    int line_count = 0;
    while (scale >= 1) {
        line_count = wrap_text_lines(txt, scale, max_w, lines, 3, &longest);
        if (line_count > 0)
            break;
        scale--;
    }
    if (line_count <= 0) {
        scale = 1;
        int y = (UI_OLED_H - (FONT_H * scale)) / 2;
        int w = text_pixel_width_chars(text_chars(txt), scale);
        int x = (UI_OLED_W - w) / 2;
        if (x < 0)
            x = 0;
        ssd1306_draw_string(disp, x, y, scale, txt, color);
        return;
    }
    int line_h = FONT_H * scale;
    int vgap = 2;
    int block_h = line_count * line_h + (line_count - 1) * vgap;
    int y0 = (UI_OLED_H - block_h) / 2;
    if (y0 < 0)
        y0 = 0;
    for (int i = 0; i < line_count; i++) {
        int chars = text_chars(lines[i]);
        int w = text_pixel_width_chars(chars, scale);
        int x = (UI_OLED_W - w) / 2;
        if (x < 0)
            x = 0;
        int y = y0 + i * (line_h + vgap);
        ssd1306_draw_string(disp, x, y, scale, lines[i], color);
    }
}

// --- Nappien käsittely ---

static bool read_button_press(uint pin) {
    if (gpio_get(pin) == 0) {
        sleep_ms(UI_DEBOUNCE_MS);
        if (gpio_get(pin) == 0) {
            while (gpio_get(pin) == 0)
                tight_loop_contents();
            return true;
        }
    }
    return false;
}

// Scroll napin tapahtumien tulkinta - single tai double click
static int poll_scroll_click_event(void) {
    static bool last_read_level = true;
    static bool stable_level = true;
    static uint32_t last_edge_ms = 0;
    static uint32_t last_release_ms = 0;
    static int click_count = 0;
    int event = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool level = gpio_get(UI_BTN_SCROLL_PIN);

    if (level != last_read_level) {
        last_read_level = level;
        last_edge_ms = now;
    }

    if ((now - last_edge_ms) >= UI_DEBOUNCE_MS && level != stable_level) {
        stable_level = level;
        if (stable_level == true) {
            click_count++;
            last_release_ms = now;
        }
    }

    if (click_count >= 2) {
        event = 2; // double-click
        click_count = 0;
    } else if (click_count == 1) {
        if ((now - last_release_ms) > UI_DOUBLE_CLICK_MS) {
            event = 1; // single-click
            click_count = 0;
        }
    }
    return event;
}

// --- Näytön piirtäminen ---

static void draw_ui(ssd1306_t *disp) {
    ssd1306_clear(disp, 0);

    if (g_state == UI_STATE_MAIN_MENU) {
        draw_header(disp, "MorseMaster9000");
        int line_height = 16;
        for (int i = 0; i < main_count; i++) {
            int y_pos = 16 + i * line_height;
            bool selected = (i == sel_main);
            if (selected) {
                draw_filled_rect(disp, 0, y_pos, UI_OLED_W, line_height, 1);
                draw_menu_icon(disp, 4, y_pos, i, true);
                ssd1306_draw_string(disp, 28, y_pos + 4, 1, main_items[i], 0);
            } else {
                draw_menu_icon(disp, 4, y_pos, i, false);
                ssd1306_draw_string(disp, 28, y_pos + 4, 1, main_items[i], 1);
            }
        }
    } else if (g_state == UI_STATE_CONNECT_MENU) {
        draw_header(disp, "Connect");
        int line_height = 16;
        for (int i = 0; i < connect_count; i++) {
            int y_pos = 16 + i * line_height;
            bool selected = (i == sel_connect);
            if (selected) {
                draw_filled_rect(disp, 0, y_pos, UI_OLED_W, line_height, 1);
                ssd1306_draw_string(disp, 28, y_pos + 4, 1, connect_items[i], 0);
            } else {
                ssd1306_draw_string(disp, 28, y_pos + 4, 1, connect_items[i], 1);
            }
        }
    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        ssd1306_draw_string(disp, 10, 10, 1, "Are you sure?", 1);
        int start_y = 35;
        for (int i = 0; i < confirm_count; i++) {
            if (i == sel_confirm) {
                draw_filled_rect(disp, 18, start_y + i * 12 - 2, 40, 12, 1);
                ssd1306_draw_string(disp, 20, start_y + i * 12, 1, confirm_items[i], 0);
            } else {
                ssd1306_draw_string(disp, 20, start_y + i * 12, 1, confirm_items[i], 1);
            }
        }
    }

    ssd1306_show(disp);
}

static void show_selection_and_return(ssd1306_t *disp, const char *msg, ui_state_t ret) {
    ssd1306_clear(disp, 0);
    draw_filled_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H, 1);
    draw_centered_wrapped(disp, msg, 2, UI_OLED_W - 4, 0);
    ssd1306_show(disp);
    sleep_ms(900);
    g_state = ret;
    ssd1306_clear(disp, 0);
    ssd1306_show(disp);
    need_redraw = true;
}

static void perform_shutdown(ssd1306_t *disp) {
    ssd1306_clear(disp, 0);
    draw_filled_rect(disp, 0, 0, UI_OLED_W, UI_OLED_H, 1);
    draw_centered_wrapped(disp, "Shutting down...", 2, UI_OLED_W - 4, 0);
    ssd1306_show(disp);
    if (g_cbs.on_shutdown)
        g_cbs.on_shutdown();
    // Sammutus kesken
}

// --- Julkiset funktiot ---

void ui_menu_init(ssd1306_t* disp, const ui_menu_callbacks_t* cbs) {
    g_disp = disp;
    if (cbs)
        g_cbs = *cbs;

    // Nappien (GPIO) asetukset
    gpio_init(UI_BTN_SCROLL_PIN);
    gpio_set_dir(UI_BTN_SCROLL_PIN, GPIO_IN);
    gpio_pull_up(UI_BTN_SCROLL_PIN);

    gpio_init(UI_BTN_SELECT_PIN);
    gpio_set_dir(UI_BTN_SELECT_PIN, GPIO_IN);
    gpio_pull_up(UI_BTN_SELECT_PIN);

    g_state = UI_STATE_MAIN_MENU;
    sel_main = 0;
    sel_connect = 0;
    sel_confirm = 1;
    need_redraw = true;
}

void ui_menu_poll(void) {
    if (!g_disp)
        return;

    ui_state_t state_before = g_state;
    int main_before = sel_main;
    int confirm_before = sel_confirm;
    int connect_before = sel_connect;

    int scroll_evt = poll_scroll_click_event();

    if (g_state == UI_STATE_MAIN_MENU) {
        if (scroll_evt == 1) {
            sel_main = (sel_main + 1) % main_count;
        }
        if (read_button_press(UI_BTN_SELECT_PIN)) {
            if (sel_main == 0) {
                g_state = UI_STATE_CONNECT_MENU;
                sel_connect = 0;
            } else if (sel_main == 2) {
                g_state = UI_STATE_CONFIRM_SHUTDOWN;
                sel_confirm = 1;
            } else {
                show_selection_and_return(g_disp, "Setup selected", UI_STATE_MAIN_MENU);
            }
        }
    } else if (g_state == UI_STATE_CONNECT_MENU) {
        if (scroll_evt == 2) {
            g_state = UI_STATE_MAIN_MENU;
        } else if (scroll_evt == 1) {
            sel_connect = (sel_connect + 1) % connect_count;
        }
        if (read_button_press(UI_BTN_SELECT_PIN)) {
            if (sel_connect == 0) {
                if (g_cbs.on_connect_usb)
                    g_cbs.on_connect_usb();
                show_selection_and_return(g_disp, "USB selected", UI_STATE_MAIN_MENU);
            } else {
                if (g_cbs.on_connect_wireless)
                    g_cbs.on_connect_wireless();
                show_selection_and_return(g_disp, "Wireless selected", UI_STATE_MAIN_MENU);
            }
        }
    } else if (g_state == UI_STATE_CONFIRM_SHUTDOWN) {
        if (scroll_evt == 2) {
            g_state = UI_STATE_MAIN_MENU;
        } else if (scroll_evt == 1) {
            sel_confirm = (sel_confirm + 1) % confirm_count;
        }
        if (read_button_press(UI_BTN_SELECT_PIN)) {
            if (sel_confirm == 0) {
                perform_shutdown(g_disp);
            } else {
                g_state = UI_STATE_MAIN_MENU;
            }
        }
    }

    if (need_redraw || state_before != g_state || main_before != sel_main || confirm_before != sel_confirm || connect_before != sel_connect) {
        draw_ui(g_disp);
        need_redraw = false;
    }
}

// Getterit tilojen tarkastamiseen (optionaaliset)
ui_state_t ui_menu_get_state(void) {
    return g_state;
}

int ui_menu_get_main_selection(void) {
    return sel_main;
}

int ui_menu_get_connect_selection(void) {
    return sel_connect;
}

int ui_menu_get_confirm_selection(void) {
    return sel_confirm;
}
