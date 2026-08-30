#include <avr/io.h>
#include "twi_328pb_compat.h"   // 328PB: alias TWBR0/TWSR0/... -> legacy names
#include <util/twi.h>
#include "twi_fast.h"
#include "twi_fast_wire.h"
#include "defs.h"

TwiFastWire Wire;


#ifndef SSD1306
#define SSD1306 0x3C
#endif

static void twiWait()
{
    while (!(TWCR & _BV(TWINT)))
        ;
}

void twiInit()
{
    DDRC &= (uint8_t)~(_BV(4) | _BV(5));
    PORTC |= _BV(4) | _BV(5);
    TWSR = 0;
    TWBR = (uint8_t)(((F_CPU / I2C_RUN_HZ) - 16) / 2);
    TWCR = _BV(TWEN);
}

// Runtime bus-speed change -- SI4735's setI2CFastModeCustom()/setI2CStandardMode()
// call this directly (e.g. to drop to 100kHz while the SSB patch loads, then
// back up for normal operation).
void twiSetClock(uint32_t hz)
{
    TWBR = (uint8_t)(((F_CPU / hz) - 16) / 2);
}

bool twiStart(uint8_t addr7)
{
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    twiWait();
    uint8_t st = TW_STATUS;
    if (st != TW_START && st != TW_REP_START)
        return false;
    TWDR = (uint8_t)(addr7 << 1);
    TWCR = _BV(TWINT) | _BV(TWEN);
    twiWait();
    return TW_STATUS == TW_MT_SLA_ACK;
}

// SLA+R variant -- twiStart() above only ever sends SLA+W (direction bit
// hardcoded 0), which was invisible while the OLED (write-only) was the
// sole consumer. Any read transaction (SI4735 status/RSSI/RDS/CTS-poll)
// needs this instead.
bool twiStartRead(uint8_t addr7)
{
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    twiWait();
    uint8_t st = TW_STATUS;
    if (st != TW_START && st != TW_REP_START)
        return false;
    TWDR = (uint8_t)((addr7 << 1) | 1);
    TWCR = _BV(TWINT) | _BV(TWEN);
    twiWait();
    return TW_STATUS == TW_MR_SLA_ACK;
}

bool twiWrite(uint8_t data)
{
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);
    twiWait();
    return TW_STATUS == TW_MT_DATA_ACK;
}

void twiStop()
{
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
    while (TWCR & _BV(TWSTO))
        ;
    TWCR = _BV(TWEN);   // idle — как в twiInit(). TWIE тут не нужен: это
                         // синхронный поллинг-драйвер без ISR(TWI_vect).
                         // Раньше висящий TWIE ловил чужой ISR() Wire и
                         // гонялся с этим же twiWait() за TWCR/TWINT.
}

uint8_t twiReadAck()
{
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
    twiWait();
    return TWDR;
}

uint8_t twiReadNack()
{
    TWCR = _BV(TWINT) | _BV(TWEN);
    twiWait();
    return TWDR;
}

void oledCmdDataStart(uint8_t x, uint8_t page)
{
    twiStart(SSD1306);
    twiWrite(0x80);
    twiWrite((uint8_t)(0xB0 | (page & 7)));
    twiWrite(0x80);
    twiWrite((uint8_t)(0x10 | (x >> 4)));
    twiWrite(0x80);
    twiWrite((uint8_t)(x & 0x0F));
    twiWrite(0x40);
}
