/*
 * AI DISCLAIMER:
 * This file was generated with the assistance of Google Gemini 3 Pro.
 * * Prompt used to generate the logic:
 * "Write a FreeRTOS task in C to handle two physical buttons on a Raspberry Pi Pico. 
 * Implement robust software debouncing (approx 40ms) to filter noise. 
 * The task should be able to distinguish between a single click and a double click 
 * (within a 350ms window) for the first button, and send the resulting events 
 * (SCROLL, SELECT, BACK) to a FreeRTOS queue."
 */

#include <stdio.h>
#include <stdbool.h>

// --- HARDWARE & SDK DEPENDENCIES ---
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tkjhat/sdk.h"  // Defines BUTTON1 and BUTTON2 specific to the board
// ------------------------------------------------

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "button_task.h"
#include "event.h"
#include "state_machine.h"

// -------------------------------------------------------------------------
// External Queue Handles
// -------------------------------------------------------------------------
// These queues act as communication pipes to other tasks (Morse logic, Main State, UI).
extern QueueHandle_t morseQ;
extern QueueHandle_t stateQ;
extern QueueHandle_t uiQ;

// -------------------------------------------------------------------------
// Pin Configuration
// -------------------------------------------------------------------------
// Robust pin definition: If the SDK hasn't defined BUTTON1/2, use fallback GPIO numbers.
// This ensures the code compiles even if the SDK configuration is missing.
#ifndef SW1_PIN
  #ifdef BUTTON1
    #define SW1_PIN BUTTON1
  #else
    #define SW1_PIN 7 // Default GPIO for Button 1 on JTKJ board
  #endif
#endif

#ifndef SW2_PIN
  #ifdef BUTTON2
    #define SW2_PIN BUTTON2
  #else
    #define SW2_PIN 8 // Default GPIO for Button 2 on JTKJ board
  #endif
#endif

// -------------------------------------------------------------------------
// Timing Constants
// -------------------------------------------------------------------------
// DEBOUNCE_MS: Mechanical switches "bounce" when pressed, creating rapid on/off signals.
// We ignore changes faster than this threshold (40ms) to read a clean signal.
#define DEBOUNCE_MS   40      

// DOUBLE_MS: The maximum time window allowed between two clicks to register as a "Double Click".
#define DOUBLE_MS     350         

// Convert milliseconds to FreeRTOS "Ticks" (the internal unit of time for the OS).
#define DEBOUNCE_TKS  (pdMS_TO_TICKS(DEBOUNCE_MS))
#define DOUBLE_TKS    (pdMS_TO_TICKS(DOUBLE_MS))

// -------------------------------------------------------------------------
// Helper Functions
// -------------------------------------------------------------------------

/**
 * @brief Reads the physical state of a button pin.
 * * The buttons on the JTKJ board are "Active Low".
 * - Default state (Pull-up): HIGH (1) -> Button NOT pressed.
 * - Pressed state (Connected to GND): LOW (0) -> Button PRESSED.
 * * We invert the result (!gpio_get) so the logic is intuitive: 
 * Returns true if pressed, false if released.
 */
static bool is_pressed(unsigned int pin) {
    return !gpio_get(pin);
}

// --- Wrappers for sending events to queues ---
// 'xQueueSend' places an item into the queue. The '0' as the last argument
// means "Non-blocking": if the queue is full, don't wait, just return immediately.

static void morse_send_character_gap(void) {
    symbol_ev_t ev = GAP_CHAR;
    xQueueSend(morseQ, &ev, 0);
}
static void morse_send_word_gap(void) {
    symbol_ev_t ev = GAP_WORD;
    xQueueSend(morseQ, &ev, 0);
}
static void morse_send_end_msg(void) {
    symbol_ev_t ev = END_MSG;
    xQueueSend(morseQ, &ev, 0);
}

static void send_ui_cmd(ui_cmd_t cmd) {
    xQueueSend(uiQ, &cmd, 0);
}

// -------------------------------------------------------------------------
// MAIN BUTTON TASK
// -------------------------------------------------------------------------
/**
 * @brief FreeRTOS task to handle button input.
 * * This task runs a software debounce algorithm and detects single/double clicks.
 * It acts as the "Input Driver" layer of the application.
 */
void buttonTask(void *pvParameters) {
    (void)pvParameters; // Suppress "unused parameter" compiler warning

    // --- Hardware Initialization ---
    // Enable the GPIO pins for input and turn on internal Pull-Up resistors.
    // Pull-Ups are crucial: they keep the pin at 3.3V when the button is open,
    // preventing the pin from "floating" (reading random noise).
    gpio_init(SW1_PIN); gpio_set_dir(SW1_PIN, GPIO_IN); gpio_pull_up(SW1_PIN);
    gpio_init(SW2_PIN); gpio_set_dir(SW2_PIN, GPIO_IN); gpio_pull_up(SW2_PIN);

    printf("Button Task started. Using SDK Pins: SW1=%d, SW2=%d\n", SW1_PIN, SW2_PIN);

    // --- State Variables for Button 1 (Complex Logic) ---
    bool       sw1_phys_prev = false; // The raw reading from the previous loop
    bool       sw1_stable    = false; // The debounced, "official" state of the button
    TickType_t sw1_last_edge = 0;     // Timestamp of the last raw change
    TickType_t sw1_press_start = 0;   // Timestamp when the button was pressed down
    
    // Variables for Double-Click detection
    bool       sw1_pending_double = false; // Are we waiting to see if a second click happens?
    TickType_t sw1_release_time = 0;       // Timestamp when the first click was released

    // --- State Variables for Button 2 (Simple Logic) ---
    bool       sw2_phys_prev = false;
    bool       sw2_stable    = false;
    TickType_t sw2_last_edge = 0;

    // --- Infinite Task Loop ---
    while (1) {
        // Get current system time (in ticks)
        TickType_t now = xTaskGetTickCount();

        // ============================================================
        // 1. BUTTON 1 PROCESSING (SW1) - Scroll / Morse Key
        // ============================================================
        
        // Step A: Read raw hardware state
        bool sw1_raw = is_pressed(SW1_PIN);
        
        // Step B: Noise Filtering (Debouncing)
        // If the raw state changed since last loop, reset the timer.
        if (sw1_raw != sw1_phys_prev) {
            sw1_last_edge = now;
        }
        sw1_phys_prev = sw1_raw;

        // Only accept the new state if it has been stable for DEBOUNCE_MS
        if ((now - sw1_last_edge) >= DEBOUNCE_TKS) {
            
            // Check if the stable state has actually changed
            if (sw1_raw != sw1_stable) {
                // Update the official stable state
                sw1_stable = sw1_raw;

                if (sw1_stable) { 
                    // --- FALLING EDGE (Button Pressed Down) ---
                    sw1_press_start = now; 
                } else {
                    // --- RISING EDGE (Button Released) ---
                    // Calculate how long it was held (useful for long-press logic, if needed)
                    uint32_t duration = (now - sw1_press_start) * portTICK_PERIOD_MS;

                    // Step C: Determine Action based on System Mode
                    // Logic splits depending on whether we are in Menu mode or Sensor/Morse mode.
                    
                    if (currentBusState != BUS_READ_SENSOR) {
                        // --- UI MENU MODE ---
                        // Check for Double Click
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // A second click occurred within the time window!
                            // Action: Go Back
                            send_ui_cmd(UI_CMD_SCROLL_BACK);
                            sw1_pending_double = false; // Reset logic
                        } else {
                            // This is the first click.
                            // Don't act yet; wait to see if a second click follows.
                            sw1_pending_double = true;
                            sw1_release_time = now;
                        }
                    } else if (currentBusState == BUS_READ_SENSOR) {
                        // --- MORSE SENSOR MODE ---
                        if (sw1_pending_double && (now - sw1_release_time) <= DOUBLE_TKS) {
                            // Double Click in Morse mode -> End Message
                            morse_send_end_msg();
                            send_ui_cmd(UI_CMD_SCROLL_BACK); // Also exit the screen
                            sw1_pending_double = false;
                        } else {
                            // Single Click -> Send Character Gap
                            // Note: We flag pending double, but we send GAP immediately.
                            // This logic assumes Gap is safe to send even if followed by double-click exit.
                            sw1_pending_double = true;
                            sw1_release_time = now;

                            morse_send_character_gap();
                        }
                    }
                }
            }
        }

        // Step D: Timeout Handler for Single Click
        // If we were waiting for a second click but time ran out...
        if (sw1_pending_double && (now - sw1_release_time) > DOUBLE_TKS) {
            sw1_pending_double = false;
            
            if (currentBusState != BUS_READ_SENSOR) {
                // Time expired, so it was definitely just a Single Click.
                // Action: Scroll Menu
                send_ui_cmd(UI_CMD_SCROLL);
            }
            // In Morse mode, single click action (GAP) was already handled on release.
        }

        // ============================================================
        // 2. BUTTON 2 PROCESSING (SW2) - Select / Word Gap
        // ============================================================
        // Same debounce logic as SW1, but simpler action logic (no double click).
        
        bool sw2_raw = is_pressed(SW2_PIN);

        if (sw2_raw != sw2_phys_prev) {
            sw2_last_edge = now;
        }
        sw2_phys_prev = sw2_raw;

        if ((now - sw2_last_edge) >= DEBOUNCE_TKS) {
            if (sw2_raw != sw2_stable) {
                sw2_stable = sw2_raw;

                // Trigger action on RELEASE (Rising Edge)
                if (!sw2_stable) {
                    if (currentBusState != BUS_READ_SENSOR) {
                        // In Menu: Select Item
                        send_ui_cmd(UI_CMD_SELECT);
                    } else {
                        // In Morse Mode: Insert Word Gap
                        morse_send_word_gap();
                    }
                }
            }
        }

        // Delay to yield CPU to other tasks. 
        // 10ms polling rate is fast enough for human input but saves energy.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}