#pragma once
// "Pragma once" ensures this file is processed only once by the compiler, 
// even if it is included multiple times. This prevents duplicate defination errors.

// -------------------------------------------------------------------------
// FreeRTOS Task Definition
// -------------------------------------------------------------------------
/**
 * @brief The main entry point for the User Interface task (thread).
 * * In the FreeRTOS operating system, every task must be a function that returns 
 * 'void' and takes a 'void*' pointer as a parameter. This specific signature 
 * is required so it can be passed to the 'xTaskCreate' function.
 * * This function acts as the "main()" for the UI. It usually contains an 
 * infinite loop (while(1)) that constantly checks for button presses and 
 * updates the screen.
 * * @param params A generic pointer used to pass arguments to the task when it 
 * starts. Often set to NULL if the task needs no external data.
 */
void ui_task(void *params);

// -------------------------------------------------------------------------
// Helper Functions
// -------------------------------------------------------------------------
/**
 * @brief Displays the startup splash screen and resets the system state.
 * * This helper function handles the initialization sequence:
 * 1. Clears the screen.
 * 2. Draws a welcome message (e.g., "TERVETULOA").
 * 3. Waits for a moment so the user can see it.
 * 4. Sets the system state to IDLE (waiting for input).
 * * Note: 'static' means this function is intended to be private to the file 
 * where it is defined, preventing other parts of the program from calling it by mistake.
 */
static void say_hi_and_go_idle(void);