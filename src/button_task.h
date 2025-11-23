#pragma once
// "Pragma once" is a preprocessor directive. It tells the compiler to include 
// this file only one time during the build process. This prevents errors that 
// happen if a file is accidentally imported multiple times (like "Redefinition of symbol X").

// -------------------------------------------------------------------------
// Task Prototype Definition
// -------------------------------------------------------------------------

/**
 * @brief The main entry point for the Button Input Task.
 * * In a Real-Time Operating System (FreeRTOS), distinct jobs are split into "Tasks" 
 * (similar to threads on a PC). This function contains the logic for reading 
 * the physical buttons.
 * * @details
 * This task typically runs in an infinite loop (while(1)) and performs the following:
 * 1. Reads the raw voltage state of GPIO pins (High/Low).
 * 2. Performs "Debouncing" (ignoring rapid electrical noise when a switch clicks).
 * 3. Detects specific patterns like Single Click, Double Click, or Long Press.
 * 4. Sends resulting events (like "UI_CMD_SELECT") to a Queue for other tasks to process.
 * * @param pvParameters 
 * A generic pointer (void*) required by the FreeRTOS `xTaskCreate` function signature.
 * It allows passing data into the task when it starts, though it is often Unused (NULL) 
 * for simple hardware tasks.
 */
void buttonTask(void *pvParameters);