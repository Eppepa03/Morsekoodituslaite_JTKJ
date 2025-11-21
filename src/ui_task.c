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
// Ulkopuoliset jonot ja tila (Siistitty merge-konfliktit)
=======
// Haetaan ulkopuoliset jonot ja tila
>>>>>>> 9da05f532726fd636a0efa88e60d6f5942b81538
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

static ssd1306_t disp;

// UUDET CALLBACKIT
static void on_usb_send(void)    { 
    printf("USB Send valittu\n"); 
    changeState(STATE_USB_CONNECTED); 
}
static void on_usb_receive(void) { 
    printf("USB Receive valittu\n"); 
    changeState(STATE_USB_CONNECTED); 
}

static void on_wireless(void) { printf("Wireless valittu\n"); }
static void on_shutdown(void) { printf("Shutdown valittu\n"); }

<<<<<<< HEAD
// --- KORJATUT KÄÄNTÖFUNKTIOT ---
static void on_orient_normal(void) {
    printf("Screen: Normal\n");
    ssd1306_rotate(&disp, false); // 0 astetta
    ssd1306_show(&disp);          // TÄRKEÄ: Lähetä komento näytölle heti
}

static void on_orient_flipped(void) {
    printf("Screen: Flipped\n");
    ssd1306_rotate(&disp, true);  // 180 astetta
    ssd1306_show(&disp);          // TÄRKEÄ: Lähetä komento näytölle heti
}
// ----------------------------

static void on_shutdown(void) { 
    printf("Shutdown sequence\n"); 
}

=======
>>>>>>> 9da05f532726fd636a0efa88e60d6f5942b81538
void ui_task(void *params) {
    (void)params;

    bool ok = false;
    for (int i = 0; i < 3 && !ok; ++i) {
        ok = ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
        if (!ok) vTaskDelay(pdMS_TO_TICKS(50));
    }
    printf("SSD1306 init(retry): %d\n", ok);
    if (!ok) { printf("OLED init fail @0x3C I2C0\n"); vTaskDelete(NULL); }

<<<<<<< HEAD
    // Oletusorientaatio alussa
    ssd1306_rotate(&disp, false);
=======
    ssd1306_rotate(&disp, false);
>>>>>>> 9da05f532726fd636a0efa88e60d6f5942b81538

    ssd1306_poweroff(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_poweron(&disp);
    vTaskDelay(pdMS_TO_TICKS(5));
    ssd1306_contrast(&disp, 0xFF);

    // Splash
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "    TERVETULOA!");
    ssd1306_show(&disp);
    vTaskDelay(pdMS_TO_TICKS(1500)); 

    // UI käyntiin
    ui_menu_callbacks_t callbacks = {
        .on_usb_send = on_usb_send,       
        .on_usb_receive = on_usb_receive, 
        .on_connect_wireless = on_wireless,
<<<<<<< HEAD
        .on_shutdown = on_shutdown,
        
        .on_orient_normal = on_orient_normal,
        .on_orient_flipped = on_orient_flipped
=======
        .on_shutdown = on_shutdown
>>>>>>> 9da05f532726fd636a0efa88e60d6f5942b81538
    };
    ui_menu_init(&disp, &callbacks);

    ui_cmd_t cmd;
    while (1) {
        if (xQueueReceive(uiQ, &cmd, portMAX_DELAY) == pdTRUE) {
            ui_menu_process_cmd(cmd);
        }
    }
}