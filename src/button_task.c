#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/sdk.h"   // init_sw1/init_sw2, SW1_PIN/SW2_PIN
#include "button_task.h"
#include "event.h"        // symbol_ev_t (Morse) ja ui_cmd_t (UI)
#include "state_machine.h" // currentState

extern QueueHandle_t morseQ;

// ---- Tunables ----
#define SCAN_MS       20      // Kuinka usein pinnejä luetaan (10ms on hyvä)
#define DEBOUNCE_MS   40      // Häiriönpoisto
#define DOUBLE_MS     350     // Aikaikkuna tuplaklikille
#define DASH_THRES_MS 200     // Kynnys: alle tämän on DOT, yli on DASH

#define SCAN_TICKS    (pdMS_TO_TICKS(SCAN_MS))
#define DEBOUNCE_TKS  (pdMS_TO_TICKS(DEBOUNCE_MS))
#define DOUBLE_TKS    (pdMS_TO_TICKS(DOUBLE_MS))

// Helpers: send events to the queue (and print for debug)
static inline void send_letter_gap(void)
{
    // symbol_ev_t ev = GAP_CHAR;
    char gap_char[] = " ";
    xQueueSend(morseQ, &gap_char, 0);
    // putchar(' ');
    // printf(" (letter)\r\n");
}

static inline void send_word_gap(void)
{
    // symbol_ev_t ev = GAP_WORD;
    char gap_word[] = "  ";
    xQueueSend(morseQ, &gap_word, 0);
    // putchar(' ');
    // putchar(' ');
    // printf(" (word)\r\n");
}

static inline void send_end_msg(void)
{
    // symbol_ev_t ev = END_MSG;
    char end_msg[]= "   \n";
    xQueueSend(morseQ, &end_msg, 0);
    // putchar(' ');
    // putchar(' ');
    // putchar(' ');
    // printf(" (END)\r\n");
}

void buttonTask(void *pvParameters)
{
    (void)pvParameters;

    // Alustetaan pinnit
    gpio_init(SW1_PIN); gpio_set_dir(SW1_PIN, GPIO_IN); gpio_pull_up(SW1_PIN);
    gpio_init(SW2_PIN); gpio_set_dir(SW2_PIN, GPIO_IN); gpio_pull_up(SW2_PIN);

    printf("Button Task started. Pins: %d, %d\n", SW1_PIN, SW2_PIN);

    // SW1 Tilamuuttujat (Scroll / Key)
    bool       sw1_phys_prev = false;     // Fyysinen tila viime kierroksella
    bool       sw1_stable    = false;     // Debouncattu tila
    TickType_t sw1_last_edge = 0;         // Milloin tila muuttui viimeksi
    TickType_t sw1_press_start = 0;       // Milloin painallus alkoi (Morse kestoa varten)
    
    // Tuplaklikki-logiikka SW1:lle
    bool       sw1_pending_double = false; // Odotetaanko toista painallusta?
    TickType_t sw1_release_time = 0;       // Milloin eka painallus loppui

    // SW2 Tilamuuttujat (Select / Gap)
    bool       sw2_phys_prev = false;
    bool       sw2_stable    = false;
    TickType_t sw2_last_edge = 0;

    while (1) {
        TickType_t now = xTaskGetTickCount();

        // --------------------------------------------
        // 1. LUE PINNIT JA DEBOUNCE (SW1)
        // --------------------------------------------
        bool sw1_raw = is_pressed(SW1_PIN);
        
        // Jos tila muuttui, nollataan debounce-ajastin
        if (sw1_raw != sw1_phys_prev) {
            sw1_last_edge = now;
        }
        sw1_phys_prev = sw1_raw;

        // Jos tila on pysynyt samana DEBOUNCE ajan, hyväksytään se
        if ((now - sw1_last_edge) >= DEBOUNCE_TKS) {
            
            if (sw1_raw != sw1_stable) {
                // TILA MUUTTUI VIRALLISESTI (Reuna)
                sw1_stable = sw1_raw;

                // --- SW1 PAINETTU (Falling Edge) ---
                if (sw1_stable) { 
                    sw1_press_start = now; // Talteen aika mittausta varten
                }
                // --- SW1 VAPAUTETTU (Rising Edge) ---
                else {
                    // Lasketaan painalluksen kesto
                    uint32_t press_duration = (now - sw1_press_start) * portTICK_PERIOD_MS;

                    // HAARAUTUMINEN: OLLAANKO MENU VAI MORSE TILASSA?
                    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                        // --- UI TILA ---
                        // Tarkistetaan tuplaklikki
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Toinen klikki tuli ajoissa! -> BACK
                            send_ui_cmd(UI_CMD_SCROLL_BACK);
                            sw1_pending_double = false; // Nollataan tilanne
                        } else {
                            // Ensimmäinen klikki -> Odotetaan hetki
                            sw1_pending_double = true;
                            sw1_release_time = now;
                        }
                    } 
                    else {
                        // --- MORSE TILA ---
                        // Tarkistetaan onko tämä tuplaklikki (Lopetus) vai normaali merkki
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Tuplaklikki Morsessa -> END_MSG (Palaa valikkoon)
                            send_morse_symbol(END_MSG);
                            sw1_pending_double = false;
                        } 
                        else {
                            // Ei vielä varmaa onko tupla, mutta Morsessa pitää reagoida heti
                            // JOTEN: Lähetetään merkki, mutta jätetään "portti auki" END_MSG:lle
                            
                            // Tässä logiikassa: Tuplaklikki on END, yksittäiset on DOT/DASH
                            sw1_pending_double = true;
                            sw1_release_time = now;

                            if (press_duration > DASH_THRES_MS) {
                                send_morse_symbol(DASH);
                            } else {
                                send_morse_symbol(DOT);
                            }
                        }
                    }
                }
            }
        }

        // UI-TILAN AIKAKATKAISU (TIMEOUT)
        // Jos odotellaan tuplaklikkiä, mutta aika meni umpeen -> Se olikin ykkösklikki
        if (sw1_pending_double && (now - sw1_release_time) > DOUBLE_TKS) {
            sw1_pending_double = false;
            
            if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                // Aika loppui UI-tilassa -> SCROLL
                send_ui_cmd(UI_CMD_SCROLL);
            } 
            else {
                // Morse-tilassa merkki lähetettiin jo painalluksesta, 
                // joten timeout ei tee tässä mitään (paitsi nollaa flagin).
            }
        }

        // --------------------------------------------
        // 2. LUE PINNIT JA DEBOUNCE (SW2)
        // --------------------------------------------
        bool sw2_raw = is_pressed(SW2_PIN);

        if (sw2_raw != sw2_phys_prev) {
            sw2_last_edge = now;
        }
        sw2_phys_prev = sw2_raw;

        if ((now - sw2_last_edge) >= DEBOUNCE_TKS) {
            if (sw2_raw != sw2_stable) {
                sw2_stable = sw2_raw;

                // --- SW2 VAPAUTETTU (Rising Edge) ---
                // Reagoidaan vasta vapautukseen, tuntuu mukavammalta valikoissa
                if (!sw2_stable) {
                    
                    if (currentState == STATE_IDLE || currentState == STATE_MENU) {
                        // --- UI TILA: SELECT ---
                        send_ui_cmd(UI_CMD_SELECT);
                    } 
                    else {
                        // --- MORSE TILA: GAP ---
                        send_morse_symbol(GAP_WORD);
                    }
                }
            }
        }

        vTaskDelay(SCAN_TICKS);
    }
}