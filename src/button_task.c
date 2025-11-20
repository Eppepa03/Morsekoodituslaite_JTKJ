#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"    
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/sdk.h"
#include "button_task.h"
#include "event.h"
#include "state_machine.h"

<<<<<<< HEAD
// Haetaan ulkopuoliset jonot ja tila
=======
>>>>>>> beea2da792bfef0a2e7709efe10d975cb967005f
extern QueueHandle_t morseQ;
<<<<<<< HEAD
extern QueueHandle_t uiQ;
extern volatile State_t currentState;
=======
>>>>>>> beea2da792bfef0a2e7709efe10d975cb967005f

// ---- Asetukset ----
#define SCAN_MS       20      // Kuinka usein luetaan (ms)
#define DEBOUNCE_MS   40      // Häiriönpoistoaika (ms)
#define DOUBLE_MS     350     // Tuplaklikin aikaraja (ms)
#define DASH_THRES_MS 200     // Kynnys DOT/DASH erotteluun (ms)

// Muutetaan ms tickeiksi
#define SCAN_TICKS    (pdMS_TO_TICKS(SCAN_MS))
#define DEBOUNCE_TKS  (pdMS_TO_TICKS(DEBOUNCE_MS))
#define DOUBLE_TKS    (pdMS_TO_TICKS(DOUBLE_MS))

<<<<<<< HEAD
// Pinnit (ellei määritelty SDK:ssa)
#ifndef SW1_PIN
#define SW1_PIN 6
#endif
#ifndef SW2_PIN
#define SW2_PIN 7
#endif

// APUFUNKTIO: Lukee pinnin tilan
// Oletus: Active Low (Pull-up). Painettu = 0 (false), Vapaa = 1 (true).
// Palauttaa true jos painettu.
static bool is_pressed(unsigned int pin) {
    return !gpio_get(pin);
=======
// Helpers: send events to the queue (and print for debug)
static inline void send_letter_gap(void)
{
    // symbol_ev_t ev = GAP_CHAR;
    char gap_char[] = " ";
    xQueueSend(morseQ, &gap_char, 0);
    // putchar(' ');
    // printf(" (letter)\r\n");
>>>>>>> beea2da792bfef0a2e7709efe10d975cb967005f
}

<<<<<<< HEAD
// APUFUNKTIO: Lähettää Morse-symbolin
static void send_morse_symbol(symbol_ev_t sym) {
    xQueueSend(morseQ, &sym, 0);
=======
static inline void send_word_gap(void)
{
    // symbol_ev_t ev = GAP_WORD;
    char gap_word[] = "  ";
    xQueueSend(morseQ, &gap_word, 0);
    // putchar(' ');
    // putchar(' ');
    // printf(" (word)\r\n");
>>>>>>> beea2da792bfef0a2e7709efe10d975cb967005f
}

<<<<<<< HEAD
// APUFUNKTIO: Lähettää UI-komennon
static void send_ui_cmd(ui_cmd_t cmd) {
    xQueueSend(uiQ, &cmd, 0);
=======
static inline void send_end_msg(void)
{
    // symbol_ev_t ev = END_MSG;
    char end_msg[]= "   \n";
    xQueueSend(morseQ, &end_msg, 0);
    // putchar(' ');
    // putchar(' ');
    // putchar(' ');
    // printf(" (END)\r\n");
>>>>>>> beea2da792bfef0a2e7709efe10d975cb967005f
}

// --- PÄÄTASKI ---
void buttonTask(void *pvParameters)
{
    (void)pvParameters;

    // Alustetaan pinnit
    gpio_init(SW1_PIN); gpio_set_dir(SW1_PIN, GPIO_IN); gpio_pull_up(SW1_PIN);
    gpio_init(SW2_PIN); gpio_set_dir(SW2_PIN, GPIO_IN); gpio_pull_up(SW2_PIN);

    printf("Button Task started. SW1=%d, SW2=%d\n", SW1_PIN, SW2_PIN);

    // --- SW1 MUUTTUJAT (Scroll / Key) ---
    bool       sw1_phys_prev = false;   // Edellinen raaka arvo
    bool       sw1_stable    = false;   // Debouncattu tila
    TickType_t sw1_last_edge = 0;       // Aika jolloin tila muuttui
    TickType_t sw1_press_start = 0;     // Aika jolloin painallus alkoi
    
    bool       sw1_pending_double = false; // Odotetaanko toista painallusta?
    TickType_t sw1_release_time = 0;       // Milloin eka painallus loppui

    // --- SW2 MUUTTUJAT (Select / Gap) ---
    bool       sw2_phys_prev = false;
    bool       sw2_stable    = false;
    TickType_t sw2_last_edge = 0;

    while (1) {
        TickType_t now = xTaskGetTickCount();

        // ============================================================
        // 1. BUTTON 1 KÄSITTELY (Scroll / Morse Key)
        // ============================================================
        bool sw1_raw = is_pressed(SW1_PIN);
        
        // A. Debounce logiikka
        if (sw1_raw != sw1_phys_prev) {
            sw1_last_edge = now;
        }
        sw1_phys_prev = sw1_raw;

        // Jos signaali on vakaa DEBOUNCE-ajan...
        if ((now - sw1_last_edge) >= DEBOUNCE_TKS) {
            
            if (sw1_raw != sw1_stable) {
                // TILA VAIHTUI (Reuna)
                sw1_stable = sw1_raw;

                if (sw1_stable) { 
                    // --- PAINETTU (Falling Edge) ---
                    sw1_press_start = now; 
                }
                else {
                    // --- VAPAUTETTU (Rising Edge) ---
                    uint32_t duration = (now - sw1_press_start) * portTICK_PERIOD_MS;

                    // HAARAUTUMINEN: UI VAI MORSE?
                    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                        // --- UI-MOODI ---
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Tuplaklikki tuli! -> BACK
                            send_ui_cmd(UI_CMD_SCROLL_BACK);
                            sw1_pending_double = false; 
                        } else {
                            // Eka klikki -> Odotetaan
                            sw1_pending_double = true;
                            sw1_release_time = now;
                        }
                    } 
                    else {
                        // --- MORSE-MOODI ---
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Tuplaklikki -> END MESSAGE / EXIT
                            send_morse_symbol(END_MSG);
                            sw1_pending_double = false;
                        } 
                        else {
                            // Normaali Morse-merkki
                            sw1_pending_double = true;
                            sw1_release_time = now;

                            if (duration > DASH_THRES_MS) send_morse_symbol(DASH);
                            else                          send_morse_symbol(DOT);
                        }
                    }
                }
            }
        }

        // B. Timeout (UI:ssa Scroll tapahtuu vasta jos tuplaklikkiä ei tule)
        if (sw1_pending_double && (now - sw1_release_time) > DOUBLE_TKS) {
            sw1_pending_double = false;
            
            if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                // Aika loppui -> Se olikin vain yksi SCROLL
                send_ui_cmd(UI_CMD_SCROLL);
            }
            // Morsessa merkki lähetettiin jo heti painalluksesta, ei tehdä mitään
        }

        // ============================================================
        // 2. BUTTON 2 KÄSITTELY (Select / Word Gap)
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
                        // Morse: Word Gap
                        send_morse_symbol(GAP_WORD);
                    }
                }
            }
        }

        vTaskDelay(SCAN_TICKS);
    }
}