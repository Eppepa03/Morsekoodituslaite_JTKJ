#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/ssd1306.h"
#include "ui_menu.h"
#include "tkjhat/sdk.h"
#include "event.h"
#include "state_machine.h"
#include <stdio.h>

<<<<<<< HEAD
extern QueueHandle_t uiQ;
=======
// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;
extern volatile State_t currentState;

>>>>>>> 1f319133314931c14aab030db12c2d4aabc7740e
static ssd1306_t disp;

// --- CALLBACKIT ---

static void on_usb_send(void) { 
    printf("USB Send valittu\n"); 
    changeState(STATE_USB_CONNECTED); 
}

static void on_usb_receive(void) { 
    printf("USB Receive valittu\n"); 
    changeState(STATE_USB_CONNECTED); 
}

static void on_wireless(void) { 
    printf("Wireless placeholder\n"); 
}

// --- UUDET KÄÄNTÖFUNKTIOT ---
static void on_orient_normal(void) {
    printf("Screen: Normal\n");
    ssd1306_rotate(&disp, false); // False = Normaali (0 astetta)
}

static void on_orient_flipped(void) {
    printf("Screen: Flipped\n");
    ssd1306_rotate(&disp, true);  // True = Käännetty (180 astetta)
}
// ---------------------------

static void on_shutdown(void) { 
    printf("Shutdown sequence\n"); 
}

void ui_task(void *params) {
    (void)params;


    bool ok = false;
    for (int i = 0; i < 3 && !ok; ++i) {
        ok = ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
        if (!ok) vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (!ok) { printf("OLED init fail\n"); vTaskDelete(NULL); }

    // Oletusorientaatio alussa
    // ssd1306_rotate(&disp, false);

    ssd1306_poweroff(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_poweron(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_contrast(&disp, 0xFF);

    // Splash screen
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 10, 25, 2, "STARTING");
    ssd1306_show(&disp);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Kytketään callbackit
    ui_menu_callbacks_t callbacks = {
        .on_usb_send = on_usb_send,
        .on_usb_receive = on_usb_receive,
        .on_connect_wireless = on_wireless,
        .on_shutdown = on_shutdown,
        
        // Kytketään uudet funktiot UI:lle
        .on_orient_normal = on_orient_normal,
        .on_orient_flipped = on_orient_flipped
    };
    
    ui_menu_init(&disp, &callbacks);

    ui_cmd_t cmd;
    while (1) {
        // Nuku kunnes viesti tulee
        if (xQueueReceive(uiQ, &cmd, portMAX_DELAY) == pdTRUE) {
            ui_menu_process_cmd(cmd);
        }
    }
}