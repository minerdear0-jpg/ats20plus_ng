#pragma once

#include <stdint.h>

void twiInit();
void twiSetClock(uint32_t hz);
bool twiStart(uint8_t addr7);
bool twiStartRead(uint8_t addr7);
bool twiWrite(uint8_t data);
void twiStop();
uint8_t twiReadAck();
uint8_t twiReadNack();
void oledCmdDataStart(uint8_t x, uint8_t page);
