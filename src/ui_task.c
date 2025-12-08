/*
 * AI DISCLAIMER:
 * This file was generated with the assistance of Google Gemini 3 Pro.
 * * Prompt used to generate the logic:
 * "Create a main FreeRTOS task for the UI that initializes the SSD1306 display 
 * and runs an event loop. The loop should check a FreeRTOS queue for UI commands 
 * and pass them to the menu processing logic. Also, implement the specific 
 * callback functions (on_usb, on_wireless, on_shutdown) defined in the menu header 
 * to control the main system state queues."
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tkjhat/ssd1306.h"
#include "pico/stdio.h"
#include "ui_menu.h"
#include "tkjhat/sdk.h"
#include "event.h"
#include "state_machine.h"
#include <stdio.h>
#include <string.h>

// -------------------------------------------------------------------------
// External Queue Handles
// -------------------------------------------------------------------------
// These handles refer to queues created in main.c (or another file).
// 'extern' tells the compiler that these variables exist somewhere else.
// Queues are thread-safe pipes used to send data/signals between FreeRTOS tasks.

extern QueueHandle_t stateQ;     // Controls the high-level application state (e.g., MENU, IDLE)
extern QueueHandle_t busStateQ;  // Controls the hardware/bus state (e.g., READING_SENSOR)
extern QueueHandle_t morseQ;     // Queue for Morse code events (unused in this file)
extern QueueHandle_t uiQ;        // Receives user input events (button presses)
extern QueueHandle_t usbRxQ;     // Receives data coming in from USB

// -------------------------------------------------------------------------
// Private Variables
// -------------------------------------------------------------------------
// The display driver instance. 'static' keeps it private to this file.
static ssd1306_t disp;

// -------------------------------------------------------------------------
// UI Callback Functions
// -------------------------------------------------------------------------
// These functions are linked to specific menu actions in the 'ui_menu.c' logic.
// When a user selects an item, the corresponding function here is executed.

/**
 * @brief Called when the system wakes up from IDLE mode.
 * Transitions the system state to MENU and enables UI updates.
 */
static void on_wake_up(void) {
    printf("UI Woken up\n"); 
    
    // 1. Tell the hardware task to start updating the UI
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
    
    // 2. Tell the main state machine to enter the MENU state
    main_state nextState = STATE_MENU;
    xQueueSend(stateQ, &nextState, 0);
    
    // 3. Reset the visual menu selection
    ui_wakeup(&disp);
}

/**
 * @brief Called when "USB Send" is selected.
 * Switches the bus state to read sensors (so data can be sent out).
 */
static void on_usb_send(void) { 
    printf("USB Send Selected\n"); 
    bus_state nextBusState = BUS_READ_SENSOR;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_USB_SEND;
    xQueueSend(stateQ, &nextState, 0);
}

/**
 * @brief Called when "USB Receive" is selected.
 * Ensures the bus is ready to update the UI with incoming text.
 */
static void on_usb_receive(void) { 
    printf("USB Receive Selected\n");
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_USB_RECEIVE;
    xQueueSend(stateQ, &nextState, 0);
}

/**
 * @brief Called when "USB" connection type is chosen.
 * Switches the main application state to USB_CONNECTED.
 */
static void on_usb(void) { 
    printf("USB Connection Selected\n"); 
    main_state nextState = STATE_USB_CONNECTED;
    xQueueSend(stateQ, &nextState, 0);  
}

/**
 * @brief Called when "Wireless" connection type is chosen.
 */
static void on_wireless(void) { 
    printf("Wireless Selected\n");
    main_state nextState = STATE_WIFI_CONNECTED;
    xQueueSend(stateQ, &nextState, 0); 
}

/**
 * @brief Called when "Shutdown" is confirmed.
 * Sets both the hardware bus and main application to IDLE (low power/sleep).
 */
static void on_shutdown(void) { 
    printf("Shutdown Selected\n");
    
    // Stop hardware activity
    bus_state nextBusState = BUS_IDLE;
    xQueueSend(busStateQ, &nextBusState, 0);
    
    // Enter logical idle state
    main_state nextState = STATE_IDLE;
    xQueueSend(stateQ, &nextState, 0);
}

/**
 * @brief Generic return handler (e.g., from sub-menus).
 * returns to the main menu state.
 */
static void on_return(void) { 
    printf("Return Selected\n");
    bus_state nextBusState = BUS_UI_UPDATE;
    xQueueSend(busStateQ, &nextBusState, 0);
    main_state nextState = STATE_MENU;
    xQueueSend(stateQ, &nextState, 0);
}

// --- Screen Orientation Callbacks ---

static void on_orient_normal(void) {
    printf("Screen: Normal\n");
    ssd1306_rotate(&disp, false); // Set rotation to 0 degrees
    ssd1306_show(&disp);          // CRITICAL: Push the command to the display immediately
}

static void on_orient_flipped(void) {
    printf("Screen: Flipped\n");
    ssd1306_rotate(&disp, true);  // Set rotation to 180 degrees
    ssd1306_show(&disp);          // CRITICAL: Push the command to the display immediately
}

// -------------------------------------------------------------------------
// Callback Registration
// -------------------------------------------------------------------------
// This struct maps the function pointers above to the events defined in ui_menu.h.
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

// -------------------------------------------------------------------------
// Helper Functions
// -------------------------------------------------------------------------

/**
 * @brief Displays a welcome message and transitions to IDLE.
 * * This runs once during startup to verify the display works.
 */
static void say_hi_and_go_idle(void) {
    // 1. Clear buffer and draw text
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "    TERVETULOA!");
    
    // 2. Send buffer to physical display
    ssd1306_show(&disp);
    
    // 3. Block this task for 1500ms so user can read the text
    vTaskDelay(pdMS_TO_TICKS(1500));

    // 4. Set system to IDLE mode
    bus_state nextBusState = BUS_IDLE;
    xQueueSend(busStateQ, &nextBusState, 0);
}

// -------------------------------------------------------------------------
// Main UI Task
// -------------------------------------------------------------------------
/**
 * @brief The main FreeRTOS task for the User Interface.
 * * Handles display initialization, draws the UI, and processes input events.
 * * @param params Unused FreeRTOS task parameter.
 */
void ui_task(void *params) {
    (void)params; // Silence "unused variable" compiler warning

    // --- Hardware Initialization ---
    // Try to initialize the OLED display up to 3 times.
    // I2C peripherals sometimes need a moment to stabilize on power-up.
    bool ok = false;
    for (int i = 0; i < 3 && !ok; ++i) {
        ok = ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
        if (!ok) vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50ms before retrying
    }
    printf("SSD1306 init(retry): %d\n", ok);
    
    // If display fails, delete this task (prevent crash loop).
    if (!ok) { printf("OLED init fail @0x3C I2C0\n"); vTaskDelete(NULL); }

    // Set default orientation
    ssd1306_rotate(&disp, false);

    // Show splash screen
    say_hi_and_go_idle();

    // Initialize the Menu System with our callbacks
    ui_menu_init(&disp, &callbacks);
    ui_cmd_t cmd;
    char last_rx_ch;
    char rx_ch;
    char temp[8];
    char rx_string[32];

    // --- Main Event Loop ---
    while (1) {
        // 1. Check for User Input (Buttons/Encoder)
        // xQueueReceive checks 'uiQ'. If data is available, it goes into 'cmd'.
        // Wait up to 10 ticks. This serves as a small delay to prevent this loop 
        // from hogging 100% CPU, while still staying responsive.
        if (xQueueReceive(uiQ, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            ui_menu_process_cmd(cmd); // Update menu logic based on button press
        }

        // 2. Check for Incoming USB Data (for Receive Screen)
        // Use 0 wait time here (non-blocking) so we don't slow down the UI
        // if there is no USB data.
        if (xQueueReceive(usbRxQ, &rx_ch, 0) == pdTRUE) {
            
            switch (rx_ch) {
                case '.': strcpy(temp, "."); break;
                case '-': strcpy(temp, "-"); break;
            }

            if (rx_ch != ' ') {
                strncat(rx_string, temp, sizeof(rx_string) - strlen(rx_string) - 1);
                printf("Received: %s\n", temp);
                printf("State of string: %s\n", rx_string);
            } else if (last_rx_ch == ' ') {
                printf("End and send: %s\n", rx_string);
                rx_string[strlen(rx_string)] = '\0';
                ui_menu_add_rx_string(rx_string); // Send string to the scrolling text buffer
                rx_string[0] = '\0';
            }
            last_rx_ch = rx_ch;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

   
}