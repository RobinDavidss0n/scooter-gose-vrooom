#include "expander.h"

#include <Wire.h>

#include "config.h"

Adafruit_MCP23X17 mcp;

void expander_init()
{
    Wire.begin(MCP23017_SDA_PIN, MCP23017_SCL_PIN);
    mcp.begin_I2C();
}
