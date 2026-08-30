#!/usr/bin/env bash
# ATS-20Plus_next: compile ATS_EX with arduino-cli.
# Compile as Uno (flash cap 32256). Silicon is ATmega328PB; USB Optiboot
# answers as 328P (1E 95 0F). ISP (USBasp) sees the real 328PB (1E 95 16).
# Hex stays under build/ (not for git).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="${ROOT}/ATS_EX"

CLEAN=1
UPLOAD_PORT=""
TARGET="uno"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options] [uno|328pb|nano]

  uno     Arduino Uno FQBN (default). Builds for ATmega328P. The DUT's
          Optiboot answers as 328P over USB, so this hex flashes fine there.
  328pb   Same Uno FQBN, but the microcode is built for the real silicon
          (-mmcu=atmega328pb): the ATS-20Plus_next DUT is populated with an
          ATmega328PB (ISP signature 1E 95 16). Needed to compile the
          hand-rolled TWI driver against the PB register map, and to get a
          vector table / CRT that matches the chip. Linker relaxation
          (-Wl,--relax) offsets the PB's larger vector table so the image
          still fits the 32256 B Optiboot budget.
  nano    Arduino Nano old bootloader (ATmega328P, 57600)

Options:
  --fast           Skip --clean (incremental)
  --upload [PORT]  After compile, flash via USB-UART (CH340 / ttyUSB)
                   Default target: avrdude -p m328p -c arduino -b 115200
                   (Optiboot signature 1E 95 0F). PORT defaults to the only
                   /dev/ttyUSB* or /dev/ttyACM*. ISP still needs -p m328pb.
  -h, --help       This text

Outputs:
  build/<target>/ATS_EX.ino.hex                 USB / avrdude
  build/<target>/ATS_EX.ino.with_bootloader.hex ISP
EOF
}

pick_usb_port() {
  local -a ports=()
  local p
  for p in /dev/ttyUSB* /dev/ttyACM*; do
    [[ -e "$p" ]] || continue
    ports+=("$p")
  done
  if [[ ${#ports[@]} -eq 0 ]]; then
    echo "build.sh: no /dev/ttyUSB* or /dev/ttyACM* (plug the receiver USB)" >&2
    exit 1
  fi
  if [[ ${#ports[@]} -gt 1 ]]; then
    echo "build.sh: several serial ports, pass one: ${ports[*]}" >&2
    exit 1
  fi
  printf '%s\n' "${ports[0]}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --fast)
      CLEAN=0
      shift
      ;;
    --upload)
      shift
      if [[ $# -ge 1 && "$1" == /dev/* ]]; then
        UPLOAD_PORT="$1"
        shift
      else
        UPLOAD_PORT="auto"
      fi
      ;;
    uno|nano|328pb)
      TARGET="$1"
      shift
      ;;
    *)
      echo "build.sh: unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

# Per-target: FQBN, avr-size MCU, and any --build-property overrides.
BUILD_PROPS=()
case "${TARGET}" in
  uno)
    FQBN="arduino:avr:uno"
    SIZE_MCU="atmega328p"
    ;;
  328pb)
    # Uno core/bootloader, but retarget the compiler+linker at the real
    # ATmega328PB. build.mcu flows into every -mmcu= in the Arduino recipes;
    # -Wl,--relax buys back the ~136 B the PB's bigger vector table costs so
    # the app still clears the 32256 B Optiboot ceiling.
    FQBN="arduino:avr:uno"
    SIZE_MCU="atmega328pb"
    BUILD_PROPS=(
      --build-property build.mcu=atmega328pb
      --build-property "compiler.c.elf.extra_flags=-Wl,--relax"
    )
    ;;
  nano)
    FQBN="arduino:avr:nano:cpu=atmega328old"
    SIZE_MCU="atmega328p"
    ;;
  *)
    echo "build.sh: bad target ${TARGET}" >&2
    exit 1
    ;;
esac

command -v arduino-cli >/dev/null || {
  echo "build.sh: arduino-cli not on PATH" >&2
  exit 1
}

OUT="${ROOT}/build/${TARGET}"
TMP="${OUT}/tmp"
mkdir -p "${OUT}" "${TMP}"

SI4735_SRC="${HOME}/Arduino/libraries/PU2CLR_SI4735"
PATCHED_SI4735="${OUT}/lib/PU2CLR_SI4735"
if [[ ! -d "${SI4735_SRC}" ]]; then
  echo "build.sh: PU2CLR SI4735 not in ${SI4735_SRC}" >&2
  exit 1
fi
rm -rf "${PATCHED_SI4735}"
mkdir -p "${OUT}/lib"
cp -a "${SI4735_SRC}" "${PATCHED_SI4735}"
python3 "${ROOT}/tools/patch_si4735_wait.py" "${PATCHED_SI4735}/src/SI4735.cpp"
cp -a "${SKETCH}/twi_fast.h" "${SKETCH}/twi_fast_wire.h" "${PATCHED_SI4735}/src/"
python3 "${ROOT}/tools/patch_si4735_wire.py" "${PATCHED_SI4735}/src"

CMD=(arduino-cli compile --fqbn "${FQBN}" --library "${PATCHED_SI4735}" --build-path "${TMP}" --output-dir "${OUT}" "${SKETCH}")
if [[ ${#BUILD_PROPS[@]} -gt 0 ]]; then
  CMD+=("${BUILD_PROPS[@]}")
fi
if [[ "${CLEAN}" -eq 1 ]]; then
  CMD+=(--clean)
fi

echo ">> ${CMD[*]}"
"${CMD[@]}"

ELF="${OUT}/ATS_EX.ino.elf"
if [[ -f "${ELF}" ]] && command -v avr-size >/dev/null; then
  echo
  avr-size -C --mcu="${SIZE_MCU}" "${ELF}" || avr-size "${ELF}"
fi

HEX="${OUT}/ATS_EX.ino.hex"
echo
echo "hex:  ${HEX}"
echo "isp:  ${OUT}/ATS_EX.ino.with_bootloader.hex"

if [[ -z "${UPLOAD_PORT}" ]]; then
  exit 0
fi

if [[ "${UPLOAD_PORT}" == auto ]]; then
  UPLOAD_PORT="$(pick_usb_port)"
fi

if [[ "${TARGET}" == uno || "${TARGET}" == 328pb ]]; then
  command -v avrdude >/dev/null || {
    echo "build.sh: avrdude not on PATH" >&2
    exit 1
  }
  # USB talks to the on-board Optiboot, which reports 328P even on 328PB
  # silicon. Keep -p m328p here regardless of target; ISP (USBasp) is the
  # only path that needs -p m328pb.
  echo ">> avrdude -c arduino -P ${UPLOAD_PORT} -b 115200 -p m328p -D -U flash:w:${HEX}:i"
  avrdude -c arduino -P "${UPLOAD_PORT}" -b 115200 -p m328p -D -U "flash:w:${HEX}:i"
else
  echo ">> arduino-cli upload -p ${UPLOAD_PORT} --fqbn ${FQBN} --input-dir ${OUT}"
  arduino-cli upload -p "${UPLOAD_PORT}" --fqbn "${FQBN}" --input-dir "${OUT}"
fi
