#pragma once
// Drop-in replacement for the ~6 TwoWire methods PU2CLR_SI4735 actually
// calls (begin, beginTransmission, write x2, endTransmission, requestFrom,
// read) -- built on twi_fast's synchronous polling driver instead of
// Wire/twi.c's interrupt-driven state machine + 32-byte tx/rx ring buffers.
//
// Only #include this INSTEAD of <Wire.h> (see tools/patch_si4735_wire.py) --
// having both linked in at once means two drivers fighting over the same
// physical TWCR/TWDR/TWSR registers.
//
// Usage pattern this mirrors is exactly what SI4735.cpp does everywhere:
//   Wire.beginTransmission(addr); Wire.write(b0); Wire.write(b1); ...; Wire.endTransmission();
//   Wire.requestFrom(addr, n); for (i<n) buf[i] = Wire.read();
// requestFrom() is called with n <= 13 everywhere in SI4735.cpp (RDS status
// is the largest single read); 16 gives headroom without wasting RAM.

#include <stdint.h>
#include <stddef.h>
#include "twi_fast.h"

#define TWI_FAST_WIRE_RXBUF 16

class TwiFastWire
{
public:
    void begin() { twiInit(); }

    void setClock(uint32_t hz) { twiSetClock(hz); }

    void beginTransmission(uint8_t addr)
    {
        _ok = twiStart(addr);
    }

    size_t write(uint8_t b)
    {
        if (!_ok) return 0;
        _ok = twiWrite(b);
        return _ok ? 1 : 0;
    }

    size_t write(const uint8_t* buf, size_t len)
    {
        size_t n = 0;
        for (size_t i = 0; i < len; i++)
            n += write(buf[i]);
        return n;
    }

    uint8_t endTransmission()
    {
        twiStop();
        return _ok ? 0 : 2;   // 2 == NACK on address, same code TwoWire
                               // returns; SI4735's addr auto-probe checks
                               // "!= 0" only, so the exact value otherwise
                               // doesn't matter to callers here.
    }

    uint8_t requestFrom(uint8_t addr, uint8_t len)
    {
        _rxPos = 0;
        _rxLen = 0;
        if (len > TWI_FAST_WIRE_RXBUF)
            len = TWI_FAST_WIRE_RXBUF;
        if (len == 0 || !twiStartRead(addr))
        {
            twiStop();
            return 0;
        }
        for (uint8_t i = 0; i < len; i++)
            _rxBuf[_rxLen++] = (i == (uint8_t)(len - 1)) ? twiReadNack() : twiReadAck();
        twiStop();
        return _rxLen;
    }

    int read()
    {
        if (_rxPos >= _rxLen)
            return -1;
        return _rxBuf[_rxPos++];
    }

private:
    bool _ok = true;
    uint8_t _rxBuf[TWI_FAST_WIRE_RXBUF];
    uint8_t _rxLen = 0;
    uint8_t _rxPos = 0;
};

extern TwiFastWire Wire;
