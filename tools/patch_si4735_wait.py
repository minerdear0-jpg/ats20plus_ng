#!/usr/bin/env python3
"""Idempotent bounded waitToSend() for PU2CLR SI4735.cpp (not a Mini/diqezit stack)."""
import pathlib
import sys

MARKER = "si4735OnBusFail"
OLD = """void SI4735::waitToSend()
{
    do
    {
        delayMicroseconds(MIN_DELAY_WAIT_SEND_LOOP); // Need check the minimum value.
        Wire.requestFrom(deviceAddress, 1);
    } while (!(Wire.read() & 0B10000000));
}"""
NEW = """extern "C" void si4735OnBusFail(void) __attribute__((weak));

void SI4735::waitToSend()
{
    // ~50 ms ceiling (167 * 300 us). Happy path: same 300 us then one requestFrom.
    uint8_t n = 0;
    for (;;)
    {
        delayMicroseconds(MIN_DELAY_WAIT_SEND_LOOP);
        if (Wire.requestFrom(deviceAddress, 1) == 0)
        {
            if (si4735OnBusFail)
                si4735OnBusFail();
            return;
        }
        if (Wire.read() & 0B10000000)
            return;
        if (++n >= 167)
        {
            if (si4735OnBusFail)
                si4735OnBusFail();
            return;
        }
    }
}"""


def main():
    path = pathlib.Path(sys.argv[1])
    text = path.read_text()
    if MARKER in text and "n >= 167" in text:
        print("si4735 waitToSend: already bounded")
        return 0
    if OLD not in text:
        print("si4735 waitToSend: unexpected source, not patching", file=sys.stderr)
        return 1
    path.write_text(text.replace(OLD, NEW, 1))
    print("si4735 waitToSend: applied bounded CTS / NACK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
