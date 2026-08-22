#!/usr/bin/env bash
# ATS-20Plus_next: compile ATS_EX with arduino-cli.
# Default target is Uno (32256 flash). Hex stays under build/ (not for git).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="${ROOT}/ATS_EX"

CLEAN=1
UPLOAD_PORT=""
TARGET="uno"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options] [uno|nano]

  uno     Arduino Uno / ATmega328P (default, this branch)
  nano    Arduino Nano old bootloader (ATmega328)

Options:
  --fast         Skip --clean (incremental)
  --upload PORT  Upload after a successful compile (e.g. /dev/ttyUSB0)
  -h, --help     This text

Outputs:
  build/<target>/ATS_EX.ino.hex                 USB / avrdude
  build/<target>/ATS_EX.ino.with_bootloader.hex ISP
EOF
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
      [[ $# -ge 2 ]] || { echo "build.sh: --upload needs a port" >&2; exit 1; }
      UPLOAD_PORT="$2"
      shift 2
      ;;
    uno|nano)
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

case "${TARGET}" in
  uno)
    FQBN="arduino:avr:uno"
    ;;
  nano)
    FQBN="arduino:avr:nano:cpu=atmega328old"
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

CMD=(arduino-cli compile --fqbn "${FQBN}" --library "${PATCHED_SI4735}" --build-path "${TMP}" --output-dir "${OUT}" "${SKETCH}")
if [[ "${CLEAN}" -eq 1 ]]; then
  CMD+=(--clean)
fi

echo ">> ${CMD[*]}"
"${CMD[@]}"

ELF="${OUT}/ATS_EX.ino.elf"
if [[ -f "${ELF}" ]] && command -v avr-size >/dev/null; then
  echo
  avr-size -C --mcu=atmega328p "${ELF}" || avr-size "${ELF}"
fi

echo
echo "hex:  ${OUT}/ATS_EX.ino.hex"
echo "isp:  ${OUT}/ATS_EX.ino.with_bootloader.hex"

if [[ -n "${UPLOAD_PORT}" ]]; then
  echo ">> arduino-cli upload -p ${UPLOAD_PORT} --fqbn ${FQBN} --input-dir ${OUT}"
  arduino-cli upload -p "${UPLOAD_PORT}" --fqbn "${FQBN}" --input-dir "${OUT}"
fi
