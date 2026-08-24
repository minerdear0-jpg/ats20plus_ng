#pragma once

#include "twi_fast.h"

static bool oledBusWrite(uint8_t byte)
{
    return twiWrite(byte);
}

static bool oledBusStart(void)
{
    return twiStart(SSD1306);
}

static uint8_t oledBusStop(void)
{
    twiStop();
    return 0;
}

static void oledBusBegin(void)
{
    twiInit();
}

SSD1306PrintDevice oled(&oledBusBegin, &oledBusStart, &oledBusWrite, &oledBusStop);
