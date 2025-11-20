#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/sdk.h"   // init_sw1/init_sw2, SW1_PIN/SW2_PIN
#include "button_task.h"
#include "event.h"        // symbol_ev_t { DOT, DASH, GAP_CHAR, GAP_WORD, END_MSG }

extern QueueHandle_t morseQ;

// ---- Tunables ----
#define SCAN_MS       20      // scan period
#define DEBOUNCE_MS   40      // ignore edges closer than this
#define DOUBLE_MS     350     // second click must arrive within this window

#define SCAN_TICKS    (pdMS_TO_TICKS(SCAN_MS))
#define DEBOUNCE_TKS  (pdMS_TO_TICKS(DEBOUNCE_MS))
#define DOUBLE_TKS    (pdMS_TO_TICKS(DOUBLE_MS))

// Helpers: send events to the queue (and print for debug)
static inline void send_letter_gap(void)
{
    symbol_ev_t ev = GAP_CHAR;
    xQueueSend(morseQ, &ev, 0);
    putchar(' ');
    printf(" (letter)\r\n");
}

static inline void send_word_gap(void)
{
    symbol_ev_t ev = GAP_WORD;
    xQueueSend(morseQ, &ev, 0);
    putchar(' ');
    putchar(' ');
    printf(" (word)\r\n");
}

static inline void send_end_msg(void)
{
    symbol_ev_t ev = END_MSG;
    xQueueSend(morseQ, &ev, 0);
    putchar(' ');
    putchar(' ');
    putchar(' ');
    printf(" (END)\r\n");
}

void buttonTask(void *pvParameters)
{
    (void)pvParameters;

    init_sw1();
    init_sw2();

    // SW1 click / double-click state
    bool      sw1_prev          = false;
    TickType_t sw1_last_change  = 0;
    TickType_t sw1_last_release = 0;
    bool      sw1_single_pending = false;   // waiting to see if a second click comes

    // SW2 simple click
    bool      sw2_prev         = false;
    TickType_t sw2_last_change = 0;

    while (1) {
        TickType_t now = xTaskGetTickCount();

        // --- read buttons (active-high per SDK docs) ---
        bool sw1 = gpio_get(SW1_PIN);
        bool sw2 = gpio_get(SW2_PIN);

        // --- SW1: single-click vs double-click on release ---
        if (sw1 != sw1_prev && (now - sw1_last_change) >= DEBOUNCE_TKS) {
            sw1_last_change = now;

            if (!sw1 && sw1_prev) { // RELEASE edge
                if (sw1_single_pending && (now - sw1_last_release) <= DOUBLE_TKS) {
                    // Double-click: END message
                    sw1_single_pending = false;
                    send_end_msg();
                } else {
                    // Start single-click pending window
                    sw1_single_pending = true;
                    sw1_last_release   = now;
                }
            }
            sw1_prev = sw1;
        }

        // If single-click pending and window elapsed with no second click → letter gap
        if (sw1_single_pending && (now - sw1_last_release) > DOUBLE_TKS) {
            sw1_single_pending = false;
            send_letter_gap();
        }

        // --- SW2: simple click on release → word gap ---
        if (sw2 != sw2_prev && (now - sw2_last_change) >= DEBOUNCE_TKS) {
            sw2_last_change = now;

            if (!sw2 && sw2_prev) { // RELEASE edge
                send_word_gap();
            }
            sw2_prev = sw2;
        }

        vTaskDelay(SCAN_TICKS);
    }
}