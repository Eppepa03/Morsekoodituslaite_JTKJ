#include <stdio.h>
#include <stdbool.h>

// --- NÄMÄ TARVITAAN SDK:N JA HARDWAREN KANSSA ---
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tkjhat/sdk.h"  // Sisältää BUTTON1 ja BUTTON2 määritykset
// ------------------------------------------------

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "button_task.h"
#include "event.h"
#include "state_machine.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

// Määritellään pinnit käyttäen SDK:n vakioita.
// Jos SDK ei jostain syystä määrittele niitä, käytetään varavaihtoehtoja.
#ifndef SW1_PIN
  #ifdef BUTTON1
    #define SW1_PIN BUTTON1
  #else
    #define SW1_PIN 7 // JTKJ-laudan yleinen Button 1 (tarkista jos ei toimi)
  #endif
#endif

#ifndef SW2_PIN
  #ifdef BUTTON2
    #define SW2_PIN BUTTON2
  #else
    #define SW2_PIN 8 // JTKJ-laudan yleinen Button 2 (tarkista jos ei toimi)
  #endif
#endif

// ---- Asetukset ----
#define DEBOUNCE_MS   40      
#define DOUBLE_MS     350         

#define DEBOUNCE_TKS  (pdMS_TO_TICKS(DEBOUNCE_MS))
#define DOUBLE_TKS    (pdMS_TO_TICKS(DOUBLE_MS))

// ---------------------------------------------------------
// APUFUNKTIOT (Nämä pitää olla ENNEN buttonTask-funktiota)
// ---------------------------------------------------------

// 1. Lukee napin tilan
// JTKJ-laudalla napit ovat yleensä Active Low (GND kun painettu).
static bool is_pressed(unsigned int pin) {
    return !gpio_get(pin);
}

// 2. Lähettää Morse-jonoon
static void morse_send_character_gap(void) {
    symbol_ev_t ev = GAP_CHAR;
    xQueueSend(morseQ, &ev, 0);
}
static void morse_send_word_gap(void) {
    symbol_ev_t ev = GAP_WORD;
    xQueueSend(morseQ, &ev, 0);
}
static void morse_send_end_msg(void) {
    symbol_ev_t ev = END_MSG;
    xQueueSend(morseQ, &ev, 0);
}

// 3. Lähettää UI-jonoon
static void send_ui_cmd(ui_cmd_t cmd) {
    xQueueSend(uiQ, &cmd, 0);
}

// ---------------------------------------------------------
// PÄÄTASKI
// ---------------------------------------------------------
void buttonTask(void *pvParameters)
{
    (void)pvParameters;

    // SDK:n init_hat_sdk() on todennäköisesti ajettu mainissa,
    // mutta varmistetaan pinnien tila tässä.
    gpio_init(SW1_PIN); gpio_set_dir(SW1_PIN, GPIO_IN); gpio_pull_up(SW1_PIN);
    gpio_init(SW2_PIN); gpio_set_dir(SW2_PIN, GPIO_IN); gpio_pull_up(SW2_PIN);

    printf("Button Task started. Using SDK Pins: SW1=%d, SW2=%d\n", SW1_PIN, SW2_PIN);

    // --- SW1 MUUTTUJAT (Scroll / Key) ---
    bool       sw1_phys_prev = false;
    bool       sw1_stable    = false;
    TickType_t sw1_last_edge = 0;
    TickType_t sw1_press_start = 0;
    
    bool       sw1_pending_double = false;
    TickType_t sw1_release_time = 0;

    // --- SW2 MUUTTUJAT (Select / Gap) ---
    bool       sw2_phys_prev = false;
    bool       sw2_stable    = false;
    TickType_t sw2_last_edge = 0;

    while (1) {
        TickType_t now = xTaskGetTickCount();

        // ============================================================
        // 1. BUTTON 1 KÄSITTELY (SW1)
        // ============================================================
        bool sw1_raw = is_pressed(SW1_PIN);
        
        // Debounce: Jos raaka tila muuttui, talleta aika
        if (sw1_raw != sw1_phys_prev) {
            sw1_last_edge = now;
        }
        sw1_phys_prev = sw1_raw;

        // Jos tila on vakaa DEBOUNCE-ajan...
        if ((now - sw1_last_edge) >= DEBOUNCE_TKS) {
            
            if (sw1_raw != sw1_stable) {
                // TILA VAIHTUI
                sw1_stable = sw1_raw;

                if (sw1_stable) { 
                    // --- PAINETTU (Falling Edge) ---
                    sw1_press_start = now; 
                }
                else {
                    // --- VAPAUTETTU (Rising Edge) ---
                    uint32_t duration = (now - sw1_press_start) * portTICK_PERIOD_MS;

                    // PÄÄTÖS: OLLAANKO VALIKOSSA VAI MORSETETAANKO?
                    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                        // --- UI-TILA ---
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Tuplaklikki -> BACK
                            send_ui_cmd(UI_CMD_SCROLL_BACK);
                            sw1_pending_double = false; 
                        } else {
                            // Eka klikki -> Odotetaan hetki
                            sw1_pending_double = true;
                            sw1_release_time = now;
                        }
                    } 
                    else {
                        // --- MORSE-TILA ---
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Tuplaklikki -> Morse END_MSG
                            morse_send_end_msg();
                            sw1_pending_double = false;
                        } 
                        else {
                            // Yksittäinen painallus -> Morse GAP_CHAR
                            sw1_pending_double = true;
                            sw1_release_time = now;

                            morse_send_character_gap();
                        }
                    }
                }
            }
        }

        // UI-TILAN TIMEOUT (Jos tuplaklikkiä ei kuulunutkaan)
        if (sw1_pending_double && (now - sw1_release_time) > DOUBLE_TKS) {
            sw1_pending_double = false;
            
            if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                // Aika loppui -> Se oli yksi SCROLL
                send_ui_cmd(UI_CMD_SCROLL);
            }
        }

        // ============================================================
        // 2. BUTTON 2 KÄSITTELY (SW2)
        // ============================================================
        bool sw2_raw = is_pressed(SW2_PIN);

        if (sw2_raw != sw2_phys_prev) {
            sw2_last_edge = now;
        }
        sw2_phys_prev = sw2_raw;

        if ((now - sw2_last_edge) >= DEBOUNCE_TKS) {
            if (sw2_raw != sw2_stable) {
                sw2_stable = sw2_raw;

                // Reagoidaan vasta kun nappi nousee ylös
                if (!sw2_stable) {
                    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                        // UI: Select
                        send_ui_cmd(UI_CMD_SELECT);
                    } 
                    else {
                        // -> Morse GAP_WORD
                        morse_send_word_gap();
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}