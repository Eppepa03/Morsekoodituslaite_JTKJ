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
    char result[32] = "";
    char message[8];

    for (;;) {
        tud_task(); // Käytetään TinyUSB:tä. Tämän pitää kutsua tehtävän alussa, jotta USB stack pysyy pystyssä.
        
        // Luetaan jonosta morseaakkosia
        if (xQueueReceive(morseQ, &morseChar, 0)) {

            // Lähetetään aakkoset TinyUSB:n avulla serial monitorille
            if (tud_cdc_connected() && currentState == STATE_USB_CONNECTED) {

                switch (morseChar) {
                    case DOT: strcpy(message, "."); printf("Recorded: %s\n", message); break;
                    case DASH: strcpy(message, "-"); printf("Recorded: %s\n", message); break;
                    case GAP_CHAR: strcpy(message, " "); printf("Recorded: Character gap\n"); break;
                    case GAP_WORD: strcpy(message, "  "); printf("Recorded: Word gap\n"); break;
                    case END_MSG: strcpy(message, "  \n"); printf("Recorded: End message\n"); break;
                }

                // Lisää message lopulliseen jonoon
                strncat(result, message, sizeof(result) - strlen(result) - 1);

                // Jos END_MSG, lähetä koko viesti ja tyhjennä puskuri
                if (morseChar == END_MSG) {
                    tud_cdc_write(result, strlen(result));
                    tud_cdc_write_flush();
                    result[0] = '\0';
                }
            }
        }
    }
}
