/* AI DISCLAIMER:
 * This file was generated with the assistance of Google Gemini 3 Pro.
 * * Prompt used to generate the definitions:
 * "Define C enums for a project that combines a UI menu and a Morse code reader. 
 * Create an enum 'ui_cmd_t' for navigation (Scroll, Select, Back) and 
 * another enum 'symbol_ev_t' for Morse events (Dot, Dash, Gap, End of Message)."
 */

#pragma once
// "Pragma once" ensures this header file is included only once during compilation.

// -------------------------------------------------------------------------
// Morse Code Event Definitions
// -------------------------------------------------------------------------
/**
 * @brief Enumeration representing the decoded atoms of Morse code communication.
 * * This enum lists the specific events that the signal processing logic generates
 * after analyzing the timing of button presses. Instead of dealing with raw 
 * timings (milliseconds), the rest of the program works with these logical symbols.
 */
typedef enum { 
    DOT,        
    DASH,       
    GAP_CHAR,   
    GAP_WORD,   
    END_MSG     
} symbol_ev_t;

// -------------------------------------------------------------------------
// User Interface Command Definitions
// -------------------------------------------------------------------------
/**
 * @brief Enumeration representing abstract User Interface commands.
 * * This abstraction decouples the hardware (physical buttons/encoders) from the 
 * software logic. The menu system doesn't need to know *which* button was pressed, 
 * only *what* action the user intends to take.
 */
typedef enum {
    UI_CMD_NONE = 0,     // No action. Used as a safe default or initialization value.
    
    UI_CMD_SCROLL,       // Move the selection cursor to the next item.
                         // Typically triggered by a single button click or encoder turn.
    
    UI_CMD_SCROLL_BACK,  // Return to the previous menu or move selection up.
                         // Typically triggered by a double-click or long press.
    
    UI_CMD_SELECT        // Confirm the current selection (Enter).
                         // Typically triggered by a specific button action.
} ui_cmd_t;