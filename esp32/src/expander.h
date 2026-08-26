#pragma once

#include <Adafruit_MCP23X17.h>

// ---------------------------------------------------------------------------
// Single shared MCP23017 instance (I2C addr 0x20). Front light, blinkers,
// and turn-signal buttons all live on it — see wiring/diagram.mmd.
// ---------------------------------------------------------------------------
extern Adafruit_MCP23X17 mcp;

// ---------------------------------------------------------------------------
// Starts the I2C bus and the MCP23017 chip. Call once from setup(), before
// any other module configures pins on `mcp`.
// ---------------------------------------------------------------------------
void expander_init();
