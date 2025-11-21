#include "usb_task.h"
#include "pico/stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include "queue.h"
#include "event.h"
#include "state_machine.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

void usbTask(void *args) {
    symbol_ev_t morseChar;
    for (;;) {
        tud_task(); // Käytetään TinyUSB:tä. Tämän pitää kutsua tehtävän alussa, jotta USB stack pysyy pystyssä.
        
        // Luetaan jonosta morseaakkosia
        if (xQueueReceive(morseQ, &morseChar, 0)) {

            // Lähetetään aakkoset TinyUSB:n avulla serial monitorille
            if (tud_cdc_connected()) {

                // Decoodataan eventit
                switch(morseChar) {
                    case DOT: putchar('.'); break;
                    case DASH: putchar('-'); break;
                    case GAP_CHAR: putchar(' '); break;
                    case GAP_WORD: printf("  "); break;
                    case END_MSG: printf("   \r\n"); break; // tarkista vielä ovatko nämä oikeat "komennot"! (siis tarkoittaako tuo "   \r\n" lopetusta)
                }

                tud_cdc_write(&morseChar, 1);
                tud_cdc_write_flush();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
