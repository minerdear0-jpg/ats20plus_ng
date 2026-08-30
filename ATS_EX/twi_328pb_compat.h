#pragma once
// ----------------------------------------------------------------------
// ATmega328PB register-name compatibility shim.
//
// The ATS-20Plus_next DUT is usually populated with an ATmega328PB. That
// part has TWO TWI blocks, so its I/O header (<avr/io.h> -> iom328pb.h)
// exposes the first one under the "...0" family: TWBR0 / TWSR0 / TWAR0 /
// TWDR0 / TWCR0 / TWAMR0. The plain 328P names (TWBR, TWSR, ...) are simply
// absent, which breaks twi_fast.cpp and the TW_STATUS macro in <util/twi.h>.
//
// TWI0 on the 328PB occupies the exact same addresses (0xB8..0xBD) and the
// exact same bit layout (TWINT/TWEN/TWSTA/TWSTO/TWEA/TWWC/TWS3..7/TWPS0..1
// stay defined by iom328pb.h), so the legacy names are precise aliases and
// this shim is a no-op behaviour change. It only ever activates when the
// build actually targets -mmcu=atmega328pb; the Uno/Nano (328P) builds are
// untouched because __AVR_ATmega328PB__ is not defined there.
//
// Include this AFTER <avr/io.h> and BEFORE <util/twi.h>.
// ----------------------------------------------------------------------

#if defined(__AVR_ATmega328PB__)

#ifndef TWBR
#define TWBR   TWBR0
#endif
#ifndef TWSR
#define TWSR   TWSR0
#endif
#ifndef TWAR
#define TWAR   TWAR0
#endif
#ifndef TWDR
#define TWDR   TWDR0
#endif
#ifndef TWCR
#define TWCR   TWCR0
#endif
#ifndef TWAMR
#define TWAMR  TWAMR0
#endif

#endif // __AVR_ATmega328PB__
