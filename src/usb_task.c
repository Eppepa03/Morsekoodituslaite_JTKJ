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
    char message[8];
    for (;;) {
        tud_task(); // Käytetään TinyUSB:tä. Tämän pitää kutsua tehtävän alussa, jotta USB stack pysyy pystyssä.
        
        // Luetaan jonosta morseaakkosia
        if (xQueueReceive(morseQ, &morseChar, 0)) {

            // Lähetetään aakkoset TinyUSB:n avulla serial monitorille
            if (tud_cdc_connected() && currentState == STATE_USB_CONNECTED) {

                switch (morseChar) {
                    case DOT: strcpy(message, "."); break;
                    case DASH: strcpy(message, "-"); break;
                    case GAP_CHAR: strcpy(message, " "); break;
                    case GAP_WORD: strcpy(message, "  "); break;
                    case END_MSG: strcpy(message, "\r\n"); break;
                }

                tud_cdc_write(&message, strlen(message));
                tud_cdc_write_flush();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
