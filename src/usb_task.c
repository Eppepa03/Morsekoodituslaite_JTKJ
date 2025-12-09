#include "usb_task.h"
#include "pico/stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include "queue.h"
#include "event.h"
#include "state_machine.h"
// Lisää TKJ-HAT SDK LED-funktioita varten
#include "tkjhat/sdk.h" 

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;
extern QueueHandle_t usbRxQ;

// USB vastaanotto callback, jota kutsutaan, kun CDC-liittymään tulee dataa
// void tud_cdc_rx_cb(uint8_t interface) {
//     printf("RX callback triggered\r\n");
//     uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE + 1];

//     uint32_t count = tud_cdc_n_read(interface, buf, sizeof(buf));

//     if (interface == 1) {
//         xQueueSend(usbRxQ, buf, 0);

//         // Vastataan OK
//         tud_cdc_n_write(interface, (uint8_t const *) "OK\n", 3);
//         tud_cdc_n_write_flush(interface);
//     }

//     if (count < sizeof(buf)) buf[count] = '\0';
// }

void usbTask(void *args) {
    symbol_ev_t morseChar;
    char result[32] = "";
    char message[8];
    
    for (;;) {
        tud_task(); // Käytetään TinyUSB:tä. Tämän pitää kutsua tehtävän alussa, jotta USB stack pysyy pystyssä.
        
        // Luetaan jonosta morseaakkosia
        if (xQueueReceive(morseQ, &morseChar, 0)) {

            // Lähetetään aakkoset TinyUSB:n avulla serial monitorille
            if (tud_cdc_connected() && currentState == STATE_USB_SEND) {

                switch (morseChar) {
                    case DOT: strcpy(message, "."); printf("Recorded: %s  \n", message); break;
                    case DASH: strcpy(message, "-"); printf("Recorded: %s  \n", message); break;
                    case GAP_CHAR: strcpy(message, " "); printf("Recorded: Character gap  \n"); break;
                    case GAP_WORD: strcpy(message, "  "); printf("Recorded: Word gap  \n"); break;
                    case END_MSG: strcpy(message, "  \n"); printf("Recorded: End message  \n"); break;
                }

                // Lisää message lopulliseen jonoon
                strncat(result, message, sizeof(result) - strlen(result) - 1);

                // Jos END_MSG, lähetä koko viesti ja tyhjennä puskuri
                if (morseChar == END_MSG) {
                    tud_cdc_write(result, strlen(result));
                    tud_cdc_write_flush();
                    result[0] = '\0';
                }

                // Ledin vlikautus end_msg:in kohdalla
                if (morseChar == END_MSG) {
                    
                    // Led päälle (true)
                    set_red_led_status(true); 
                    
                    // Odotetaan lyhyt aika (50 ms)
                    vTaskDelay(pdMS_TO_TICKS(50));
                    
                    // LED POIS PÄÄLTÄ (false)
                    set_red_led_status(false); 
                }
            }
        }
        // Vastaanotetaan morsekoodia
        if (tud_cdc_connected() && currentState == STATE_USB_RECEIVE) {
            int interface = 0;
            while (tud_cdc_n_available(interface)) {
                char ch = tud_cdc_n_read_char(interface);
                xQueueSend(usbRxQ, &ch, portMAX_DELAY);

                // Vastataan OK
                // tud_cdc_n_write(interface, (uint8_t const *) "OK  \n", 3);
                // tud_cdc_n_write_flush(interface);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));

    }
}
