#pragma once

//If you set this def to 0 project will be compiled without RDS 
//and everything related to RDS will be excluded from build
#define USE_RDS 0

#define EEPROM_APP_ID				235
#define EEPROM_DATA_START_ADDRESS	1
#define EEPROM_VERSION_ADDRESS      1000
#define EEPROM_APP_ID_ADDRESS       0

// Splash / about string. APP_VERSION below is EEPROM layout, not this.
#define FW_VERSION_STR "2.0b"

//EEPROM Settings
#define STORE_TIME 10000 // Inactive time to save our settings

// OLED Const values
#define DEFAULT_FONT FONT8X16POB
#define RST_PIN -1
#define RESET_PIN 12

//Battery charge monitoring analog pin (Voltage divider 10-10 KOhm directly from battery)
#define BATTERY_VOLTAGE_PIN A2

// Encoder
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
// Unconsumed ticks between loop() passes. Caps int8_t, still lets a fast spin count as many steps.
#define ENCODER_MAX_BURST 20

// Buttons
#define MODE_SWITCH       4 
#define BANDWIDTH_BUTTON  5
#define VOLUME_BUTTON     6
#define AVC_BUTTON        7
#define BAND_BUTTON       8 
#define SOFTMUTE_BUTTON   9
#define AGC_BUTTON       11
#define STEP_BUTTON      10

#define ENCODER_BUTTON   14

// Default values
#define MIN_ELAPSED_TIME 100
#define MIN_ELAPSED_RSSI_TIME 150
// After the last encoder tick, wait this long before committing SI4735 (OLED already shows requested freq).
#define FREQ_COMMIT_MS 70
#define BACKGROUND_UI_MS 1000
// Runtime I2C after SSB patch download (patch itself stays at 500 kHz).
// Default 100 kHz while listening. Do not leave 400 kHz on; see reports/ATS-20_I2C_burst.txt.
#define I2C_RUN_HZ 100000L

// Auto OLED off: 0 = never, 1..4 = 15s, 30s, 60s, 120s
#define DISPLAY_OFF_MAX 4

// IARU S-meter: Si473x RSSI is dBµV. S9 = 50 µV = 34 dBµV, 6 dB per S-unit.
#define S9_DBUV 34
#define SMETER_MAX_OVER_S9 60
#define SMETER_SEGMENTS 16
#define DEFAULT_VOLUME 25
#define ADJUSTMENT_ACTIVE_TIMEOUT 1500

// Sweet Spot pages: header/transient 0-1, 7-seg 2-4, S-meter 5, secondary 6-7.
#define UI_PAGE_FREQ 2
#define UI_PAGE_SIGNAL 5
#define UI_PAGE_SECONDARY 6

// Band settings
#define SW_LIMIT_LOW		1710
#define SW_LIMIT_HIGH		30000
#define LW_LIMIT_LOW		153
#define CB_LIMIT_LOW		26200
#define CB_LIMIT_HIGH		28000

#define BAND_DELAY                 2
#define VOLUME_DELAY               1 

#define buttonEvent                NULL