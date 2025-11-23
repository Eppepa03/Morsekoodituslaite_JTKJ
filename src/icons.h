/*
 * AI DISCLAIMER:
 * This file was generated with the assistance of Google Gemini 3 Pro.
 * * Prompt used to generate the data:
 * "Generate C byte arrays (uint8_t) representing 16x16 monochrome bitmap icons 
 * for an OLED display."
 */

#pragma once
#include <stdint.h>

// -------------------------------------------------------------------------
// Icon Configuration & format Explanation
// -------------------------------------------------------------------------
// The icons are stored as 1-bit bitmaps (monochrome).
// - 1 (Bit is Set)   = Pixel ON (White on OLED)
// - 0 (Bit is Clear) = Pixel OFF (Black on OLED)
//
// Data Layout:
// The bits are read from Left (MSB) to Right (LSB).
// Since the icons are 16 pixels wide, each row requires exactly 16 bits.
// 16 bits = 2 Bytes per row.
//
// Total Size: 16 rows * 2 bytes/row = 32 bytes per icon.
// Used https://www.lddgo.net/en/string/xbm-editor to create the bitmaps.

#define ICON_W 16 // Width in pixels
#define ICON_H 16 // Height in pixels

/* * Icon: Plug (Connect)
 * Represents a physical connector or USB cable.
 * * Visual breakdown of the hex data:
 * Top:    Wire leading down
 * Middle: Thick connector body
 * Bottom: Two distinct pins
 *
 * ASCII Art logic below: '.' = 0, '#' = 1
 */
static const uint8_t ICON_CONNECT[] = {
    // Byte 1  Byte 2    // Binary Representation (16 pixels wide)
    0x01,0x80, // .......##....... (Top wire)
    0x01,0x80, // .......##.......
    0x01,0x80, // .......##.......
    0x0F,0xF0, // ....########.... (Connector body start - 0x0F=00001111, 0xF0=11110000)
    0x0F,0xF0, // ....########....
    0x0F,0xF0, // ....########....
    0x0F,0xF0, // ....########....
    0x0F,0xF0, // ....########....
    0x0F,0xF0, // ....########....
    0x0F,0xF0, // ....########....
    0x09,0x90, // ....#..##..#.... (Pins appearing)
    0x09,0x90, // ....#..##..#....
    0x09,0x90, // ....#..##..#....
    0x09,0x90, // ....#..##..#....
    0x00,0x00, // ................ (Empty space at bottom)
    0x00,0x00  // ................
};

/* * Icon: Gear (Setup/Settings)
 * Represents configuration or mechanical settings.
 * * Visual features:
 * - Protruding teeth on the outside.
 * - A hollow hole in the center.
 * * Note: Unlike the other icons, this data is compacted into fewer lines,
 * but the logic is identical: 2 bytes per row, 16 rows total.
 */
static const uint8_t ICON_SETUP[] = {
  0x01,0x80, 0x03,0xC0, 0x7F,0xFE, 0x3C,0x3C,
  0x18,0x18, 0x3C,0x3C, 0x7E,0x7E, 0x18,0x18,
  0x3C,0x3C, 0x7F,0xFE, 0x03,0xC0, 0x01,0x80,
  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00

};

/* * Icon: Power Button (Shutdown)
 * Standard IEC 5009 standby symbol.
 * * Visual breakdown:
 * - A vertical line in the center top.
 * - A circle surrounding it, broken at the top.
 */
static const uint8_t ICON_POWER[] = {
    // Byte 1  Byte 2    // Binary Visual
    0x01,0x80, // .......##....... (Top of the vertical line)
    0x01,0x80, // .......##.......
    0x01,0x80, // .......##.......
    0x01,0x80, // .......##.......
    0x01,0x80, // .......##.......
    0x39,0x9C, // ..###..##..###.. (Top of the curve starts appearing)
    0x70,0x0E, // .###........###. (Curve widens)
    0x60,0x06, // .##..........##.
    0x60,0x06, // .##..........##.
    0x60,0x06, // .##..........##.
    0x60,0x06, // .##..........##.
    0x70,0x0E, // .###........###. (Curve starts closing)
    0x38,0x1C, // ..###......###..
    0x1F,0xF8, // ...##########... (Bottom of the circle)
    0x07,0xE0, // .....######.....
    0x00,0x00  // ................
};