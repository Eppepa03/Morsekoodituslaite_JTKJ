#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/ssd1306.h"
#include "ui_menu.h"
#include "tkjhat/sdk.h"
#include "event.h"
#include "state_machine.h"
#include <stdio.h>
#include "tusb.h"

// Haetaan ulkopuoliset jonot ja tila
extern QueueHandle_t stateQ;
extern QueueHandle_t busStateQ;
extern QueueHandle_t morseQ;
extern QueueHandle_t uiQ;
extern QueueHandle_t usbRxQ;

static ssd1306_t disp;

// UUDET CALLBACKIT
static void on_wake_up(void) {
    // UI käyntiin
    printf("UI herätetty\n"); 
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_MENU;
    xQueueSend(stateQ, &nextState, 0);
    ui_wakeup(&disp);
}
static void on_usb_send(void) { 
    printf("USB Send valittu\n"); 
    bus_state nextBusState = BUS_READ_SENSOR;
    xQueueSend(busStateQ, &nextBusState, 0);
}
static void on_usb_receive(void) { 
    printf("USB Receive valittu\n");
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
}
static void on_usb(void) { 
    printf("USB valittu\n"); 
    main_state nextState = STATE_USB_CONNECTED;
    xQueueSend(stateQ, &nextState, 0);  
}
static void on_wireless(void) { 
    printf("Wireless valittu\n");
    main_state nextState = STATE_WIFI_CONNECTED;
    xQueueSend(stateQ, &nextState, 0); 
}
static void on_shutdown(void) { 
    printf("Shutdown valittu\n");
    bus_state nextBusState = BUS_IDLE;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_IDLE;
    xQueueSend(stateQ, &nextState, 0);
}
static void on_return(void) { 
    printf("Return valittu\n");
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_MENU;
    xQueueSend(stateQ, &nextState, 0);
}

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

ui_menu_callbacks_t callbacks = {
    .on_wake_up = on_wake_up,
    .on_usb_send = on_usb_send,       
    .on_usb_receive = on_usb_receive,
    .on_connect_usb = on_usb, 
    .on_connect_wireless = on_wireless,
    .on_shutdown = on_shutdown,
    .on_return = on_return,
    
    .on_orient_normal = on_orient_normal,
    .on_orient_flipped = on_orient_flipped
};

// APUFUNKTIOT
static void say_hi_and_go_idle(void) {
    // Splash
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "    TERVETULOA!");
    ssd1306_show(&disp);
    vTaskDelay(pdMS_TO_TICKS(1500));

    bus_state nextBusState = BUS_IDLE;
    xQueueSend(busStateQ, &nextBusState, 0);
}

void ui_task(void *params) {
    (void)params;

    bool ok = false;
    for (int i = 0; i < 3 && !ok; ++i) {
        ok = ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
        if (!ok) vTaskDelay(pdMS_TO_TICKS(50));
    }
    printf("SSD1306 init(retry): %d\n", ok);
    if (!ok) { printf("OLED init fail @0x3C I2C0\n"); vTaskDelete(NULL); }

    ssd1306_rotate(&disp, false);

    say_hi_and_go_idle();

    ui_menu_init(&disp, &callbacks);

    ui_cmd_t cmd;
    char rx_char[32];

    while (1) {
        // Poll UI commands (non-blocking or short wait)
        if (xQueueReceive(uiQ, &cmd, 10) == pdTRUE) {
            ui_menu_process_cmd(cmd);
        }

        // Poll USB Receive queue
        char rx_buf[CFG_TUD_CDC_RX_BUFSIZE + 1];
        if (xQueueReceive(usbRxQ, rx_buf, 0) == pdTRUE) {
            printf("jono");
            // ui_menu_add_rx_char(rx_char);
        }
    }
}