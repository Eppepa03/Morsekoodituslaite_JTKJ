#pragma once
// Morse-tapahtumat
typedef enum { DOT, DASH, GAP_CHAR, GAP_WORD, END_MSG } symbol_ev_t;

// UI-tapahtumat
typedef enum {
    UI_CMD_NONE = 0,
    UI_CMD_SCROLL,       // Scrollaus
    UI_CMD_SCROLL_BACK,  // Takaisin (tuplaklikki)
    UI_CMD_SELECT        // Valinta
} ui_cmd_t;