# ATS-20 Plus NG (ats20plus_ng)

Next-generation firmware for **ATS-20 / ATS-20+** receivers (**ATmega328** + **Si4735**).

**Version:** 2.0C  
**Repository:** https://github.com/minerdear0-jpg/ats20plus_ng  
**Download (.hex):** https://github.com/minerdear0-jpg/ats20plus_ng/releases/tag/v2.0C

Lineage: **Goshante ATS_EX v1.18** (PU2CLR) → hardware services from **diqezit** → new OLED UI and radio core polish for **328PB / 32 KB flash**.

## UI (main screen)

Firmware **2.0C** on **ATS-20+** hardware.

![SSB main screen: 7150.00 kHz LSB, 3.0K bandwidth, S7 S-meter, SNR aux](img/ats_20_final1.png)

*SSB (LSB), 7150 kHz — Icom-style VFO, idle MODE/BW (Karat), 7-cell S-meter wall, GOST S-label, SNR (dB) in aux slot*

![WFM main screen: 101.00 MHz, TUNED, S9+ S-meter, stereo pilot](img/ats_20_final2.png)

*WFM, 101 MHz — TUNED latch, stereo pictogram, S9+, AUTO bandwidth*

## User guide (Russian)

**[docs/USER_GUIDE.ru.md](docs/USER_GUIDE.ru.md)** — full manual: controls, overlays, settings cave, S-meter, FM FREQOFF, differences vs Goshante v1.18 and diqezit.

## Build

```bash
./build.sh uno              # hex → build/uno/ATS_EX.ino.hex (328P, not in git)
./build.sh 328pb            # hex → build/328pb/ATS_EX.ino.hex (real 328PB silicon)
./build.sh --fast 328pb
./build.sh --upload 328pb     # CH340 / ttyUSB, Optiboot 328P @ 115200
```

Target: **Arduino Uno FQBN** (32256 B flash). The DUT is populated with an
**ATmega328PB** (ISP signature `1E 95 16`, `-p m328pb`); the on-board Optiboot
still answers as 328P over USB. Use **`328pb`** to build the microcode for the
actual chip — it retargets the compiler/linker at `-mmcu=atmega328pb` so the
hand-rolled TWI driver sees the PB register map, and relaxes the link
(`-Wl,--relax`) to absorb the PB's larger vector table within the flash budget.
The `uno` target stays byte-identical to earlier releases.

## Flashing

**Required programmer:** [**USBasp NG**](https://github.com/minerdear0-jpg/usbasp_ng) — ISP adapter for ATmega328(P/PB). Needed for first-time flash and for the full image (app + bootloader).

| Release file | Silicon | Tool | When |
|--------------|---------|------|------|
| `ats20plus_ng_v2.0C_usb.hex` | 328P / generic | USB-UART (CH340) + AVRDUDESS / avrdude | Bootloader already on the receiver; routine updates |
| `ats20plus_ng_v2.0C_with_bootloader.hex` | 328P / generic | **USBasp NG** (ISP) | Blank chip, bricked bootloader, or full reflash |
| `ats20plus_ng_v2.0C_328pb_usb.hex` | **ATmega328PB** | USB-UART (CH340) + AVRDUDESS / avrdude | PB board, bootloader present; routine updates |
| `ats20plus_ng_v2.0C_328pb_with_bootloader.hex` | **ATmega328PB** | **USBasp NG** (ISP) | PB board — blank chip, bricked bootloader, or full reflash |

The `_328pb` files are built for the real silicon (`./build.sh 328pb`); the
plain files are the 328P image and stay byte-identical to v2.0B. Either runs on
a 328PB board, but prefer the `_328pb` build there.

After first flash: hold **encoder button** at power-on for **EEPROM RESET**.

## Fuses

Correct fuses for an ATS-20 / ATS-20+ (ATmega328P, 16 MHz crystal, Optiboot at
byte `0x7E00`):

| Fuse | Value | Notes |
|------|-------|-------|
| Low (`lfuse`) | `0xFF` | full-swing crystal ≥ 8 MHz, `CKDIV8` off (no /8 prescale) |
| High (`hfuse`) | `0xDE` | `BOOTRST` on, **`BOOTSZ` = 256 words → reset vector at `0x7E00`** |
| Extended (`efuse`) | `0xFD` | BOD 2.7 V (some tools show `0x05` — same 3 BOD bits) |

Read them first:

```bash
avrdude -c usbasp -p m328p -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h
```

**Known trap — `hfuse = 0xDA`.** That is the fuse for a legacy 2 KB bootloader:
`BOOTSZ` = 1024 words, so with `BOOTRST` on the CPU starts executing at byte
`0x7800`. Every image here (and Optiboot) lives at `0x7E00`, so the reset lands
in the middle of the application → hang. Symptom: the firmware "does not start"
(black screen) on every build **except** a full 32 KB image whose `0x7800`–
`0x7DFF` happens to be blank — it slides through the `0xFF` padding into the
bootloader and boots, which masks the real cause.

Fix (leave `lfuse` / `efuse` alone):

```bash
avrdude -c usbasp -p m328p -U hfuse:w:0xDE:m
avrdude -c usbasp -p m328p -U flash:w:dist/ats20plus_ng_v2.0C_with_bootloader.hex:i
```

On **ATmega328PB** use `-p m328pb`; the `hfuse` value and the `0x7E00` boot
layout are the same (`efuse` differs — it carries the extra `CFD` bit, read and
keep it as-is).

## Chip variants — genuine ATmega328P / 328PB only

Supported silicon: **ATmega328P** (signature `1E 95 0F`) and **ATmega328PB**
(`1E 95 16`). The on-board Optiboot answers `1E 95 0F` over USB even on a PB
board, so read the real signature over ISP (`avrdude -c usbasp -p m328pb`).

**LGT8F328P (LogicGreen clone) — not supported, not planned.** Cheap boards
sold as "ATmega328P" that report signature `1E 95 0F` over ISP but are a
different device: LGTZ8 core, internal 32 MHz RC (no crystal), remapped /
extended peripherals, and **no hardware EEPROM** (flash-emulated). The AVR
images built here do not run on it.

A port is possible but not worth it. It would need the `lgt8fx` core, a
SWD / serial flashing path (**USBasp NG cannot program an LGT8F**), an
ADC reference / resolution fix (`analogRead()` uses raw `ADMUX`), write
batching for the emulated EEPROM, and code trimming — the app is already at
**32162 / 32256 B** and the LGT8F bootloader plus its EEPROM sector leave
less room. Estimated ~1.5–2 weeks of work for a receiver that gains nothing
on RF accuracy (the Si4732 has its own reference). **Economically
unjustified — use a genuine ATmega328P / 328PB.**

## vs Goshante ATS_EX v1.18 (summary)

- **UI:** Icom-style VFO, overlay chips, 7-cell S-meter wall, GOST label, aux SNR / FREQOFF cue, plain Karat idle labels.
- **Settings:** one-line **MODE long** cave — multi-page **BAND− menu removed**.
- **RDS:** compiled out (`USE_RDS 0`) to save flash.
- **Radio path:** deferred SI4735 tune, 100 kHz I2C idle / 400 kHz on frequency commit, **RADIO ERR** + retry.
- **From diqezit:** ANB, SQL, display auto-off (DIS), CW pitch (CWP).

Details: [docs/USER_GUIDE.ru.md](docs/USER_GUIDE.ru.md).

## Flash budget

~**32162 / 32256** B, SRAM ~**1087 / 2048**. Treat every UI pixel as an `avr-size` experiment.

## Credits

- **Goshante** — ATS_EX v1.18 baseline UI/controls concept  
- **PU2CLR** — Si4735 library lineage  
- **diqezit** — ANB/SQL and hardware-oriented fixes  
- **esp32-si4732/ats-mini** — architectural ideas (not a port)
