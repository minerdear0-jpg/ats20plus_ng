// ----------------------------------------------------------------------
// ATS_EX (Extended) Firmware for ATS-20 and ATS-20+ receivers.
// Based on PU2CLR sources.
// Inspired by closed-source swling.ru firmware.
// For more information check README file in my github repository:
// http://github.com/goshante/ats20_ats_ex
// ----------------------------------------------------------------------
// By Goshante, 2024. Co-authors: OOMAN and Cursor, 2026.
// http://github.com/goshante
// ----------------------------------------------------------------------

#include <SI4735.h>
#include <EEPROM.h>
#include <Tiny4kOLED_common.h>
#include "oled_bus.h"
#include "font8x16pob_ui.h"
#include "font16x24vfoIcom.h"
#include "font7x14smeter.h"
#include "Rotary.h"
#include "SimpleButton.h"

#include "defs.h"
#include "globals.h"
#include "radio.h"
#include "Utils.h"

void showStatus(bool cleanFreq = false);
void uiFlush();
void paintTransient(const char* title, const char* value);
void restoreIdleHeader();
void showRadioError();
void switchCommand(uint8_t cmd = CMD_NONE, void (*showFunction)() = 0);
void cycleModePick(int8_t dir);
void commitModePick();
void paintModeOrBand();
void displayPower(bool on);
void setMuted(bool on);
void showRadio();

int getLastStep()
{
    if (isSSB())
        return g_amTotalSteps + g_ssbTotalSteps - 1;

    if (g_bandIndex == LW_BAND_TYPE || g_bandIndex == MW_BAND_TYPE)
        return g_amTotalStepsSSB - 1;

    return g_amTotalSteps - 1;
}

// --------------------------
// ------- Main logic -------
// --------------------------

#define APP_VERSION 123

static uint8_t g_modePick = AM;
static char g_ovTitle[5];
static char g_ovVal[8];
static uint8_t g_ovX, g_ovW, g_ovVisForce, g_ovFill;
static uint8_t g_tunedDrawn;
static uint8_t g_radioSlot;
// One-line cave (ex RADIO/DISP/AUDIO/SYS). Masks: AM=1 LSB=2 USB=4 CW=8 FM=16.
static const uint8_t kRadioRing[] = {
    SettingsIndex::ATT,
    SettingsIndex::ANB,
    SettingsIndex::SQL,
    SettingsIndex::Sync,
    SettingsIndex::CutoffFilter,
    SettingsIndex::AutoVolControl,
    SettingsIndex::SoftMute,
    SettingsIndex::DeEmp,
    SettingsIndex::SVC,
    SettingsIndex::SSM,
    SettingsIndex::Brightness,
    SettingsIndex::DisplayOff,
    SettingsIndex::SWUnits,
    SettingsIndex::UnitsSwitch,
    SettingsIndex::BFO,
    SettingsIndex::ScanSwitch,
    SettingsIndex::CWSwitch,
    SettingsIndex::CWPitch
};
static const uint8_t kRadioMask[] PROGMEM = {
    0x0F, 0x0F, 0x01, 0x0E, 0x0E, 0x0F,
    0x0F, 0x10, 0x0E, 0x0E,
    0x1F, 0x1F, 0x0F, 0x1F,
    0x1F, 0x1F, 0x0F, 0x08
};
enum { kRadioN = 18 };

uint8_t radioNextSlot(uint8_t from)
{
    uint8_t s = from;
    uint8_t m = (uint8_t)(1u << g_currentMode);
    for (uint8_t i = 0; i < kRadioN; i++)
    {
        if (++s >= kRadioN)
            s = 0;
        if (pgm_read_byte(&kRadioMask[s]) & m)
            return s;
    }
    return 255;
}

//Initialize controller
// Replace Arduino wiring_digital / wiring_analog — PWM-aware tables never used here.
void pinMode(uint8_t pin, uint8_t mode)
{
    volatile uint8_t* ddr;
    volatile uint8_t* port;
    uint8_t bit;
    if (pin < 8)
    {
        ddr = &DDRD;
        port = &PORTD;
        bit = pin;
    }
    else if (pin < 14)
    {
        ddr = &DDRB;
        port = &PORTB;
        bit = pin - 8;
    }
    else
    {
        ddr = &DDRC;
        port = &PORTC;
        bit = pin - 14;
    }
    if (mode == OUTPUT)
        *ddr |= (1 << bit);
    else
    {
        *ddr &= ~(1 << bit);
        if (mode == INPUT_PULLUP)
            *port |= (1 << bit);
        else
            *port &= ~(1 << bit);
    }
}

void digitalWrite(uint8_t pin, uint8_t val)
{
    if (pin < 8)
    {
        if (val)
            PORTD |= (1 << pin);
        else
            PORTD &= ~(1 << pin);
    }
    else if (pin < 14)
    {
        pin -= 8;
        if (val)
            PORTB |= (1 << pin);
        else
            PORTB &= ~(1 << pin);
    }
    else
    {
        pin -= 14;
        if (val)
            PORTC |= (1 << pin);
        else
            PORTC &= ~(1 << pin);
    }
}

int analogRead(uint8_t pin)
{
    if (pin >= 14)
        pin -= 14;
    ADMUX = (1 << REFS0) | (pin & 7);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    return ADC;
}

void setup()
{
    //We need to save more space with this
    DDRB |=  (1 << DDB5);   //13 pin
    DDRD &= ~(1 << ENCODER_PIN_A);
    PORTD |= (1 << ENCODER_PIN_A);
    DDRD &= ~(1 << ENCODER_PIN_B);
    PORTD |= (1 << ENCODER_PIN_B);
    g_voltagePinConnnected = analogRead(BATTERY_VOLTAGE_PIN) > 300;

    // Frozen: 128x64r (C8/A1 + pump). Do not use 128x64br / 0xAD 0x30 (hum).
    oled.begin(128, 64, sizeof(tiny4koled_init_128x64r), tiny4koled_init_128x64r);
    oled.clear();
    displayPower(true);
    oled.setFont(DEFAULT_FONT);

    //Don't use digitalRead()
    //Registers save us more space
    if (!(PINC & (1 << (ENCODER_BUTTON - 14))))
    {
        saveAllReceiverInformation();
        oledPrint("  EEPROM RESET", 0, 0, DEFAULT_FONT);
        oledPrint("----------------", 0, 2, DEFAULT_FONT);
        delay(960);
    }
    oled.clear();

    //Encoder interrupts
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

    g_si4735.getDeviceI2CAddress(RESET_PIN);
    g_si4735.setup(RESET_PIN, MW_BAND_TYPE);

    delay(500);

    //Load settings from EEPROM
    if (EEPROM.read(EEPROM_VERSION_ADDRESS) == APP_VERSION && EEPROM.read(EEPROM_APP_ID_ADDRESS) == EEPROM_APP_ID)
        readAllReceiverInformation();
    else
        saveAllReceiverInformation();

    //Initialize current band settings and read frequency
    applyBandConfiguration();
    g_currentFrequency = g_previousFrequency = g_si4735.getFrequency();
    g_si4735.setVolume(g_volume);
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);

    //Draw main screen
    oled.clear();
    showStatus(true);
    if (g_radioError)
        showRadioError();
    g_lastInputMs = millis();
}

uint8_t volumeEvent(uint8_t event, uint8_t pin)
{
    if (event)
        g_lastInputMs = millis();
    if (!g_settingsActive && BUTTONEVENT_FIRSTLONGPRESS == event)
    {
        if (g_currentCmd != CMD_VOLUME)
        {
            g_volFromButtons = true;
            switchCommand(CMD_VOLUME, showVolume);
        }
        g_uiLayer = UI_LAYER_TRANSIENT;
    }
    if (g_muteVolume)
    {
        if (!BUTTONEVENT_ISDONE(event))
        {
            if ((BUTTONEVENT_SHORTPRESS != event) || (VOLUME_BUTTON == pin))
                doVolume(1);
        }
    }

    if (!g_muteVolume)
    {
#if (0 != VOLUME_DELAY)
#if (VOLUME_DELAY > 1)
        static uint8_t count;
        if (BUTTONEVENT_FIRSTLONGPRESS == event)
        {
            count = 0;
        }
#endif
        if (BUTTONEVENT_ISLONGPRESS(event))
            if (BUTTONEVENT_LONGPRESSDONE != event)
            {
#if (VOLUME_DELAY > 1)
                if (count++ == 0)
#endif
                    doVolume(VOLUME_BUTTON == pin ? 1 : -1);
#if (VOLUME_DELAY > 1)
                count = count % VOLUME_DELAY;
#endif
            }
#else
        if (BUTTONEVENT_FIRSTLONGPRESS == event)
            event = BUTTONEVENT_SHORTPRESS;
#endif
    }
    return event;
}

uint8_t simpleEvent(uint8_t event, uint8_t pin)
{
    if (event)
        g_lastInputMs = millis();
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
    return event;
}

// MODE: keep FIRSTLONGPRESS (RADIO ring). Do not remap via simpleEvent.
uint8_t modeBtnEvent(uint8_t event, uint8_t pin)
{
    if (event)
        g_lastInputMs = millis();
    return event;
}

//This looks like it's better to remove them and use only simpleEvent
//But it's a part of a hack that allows us to save more flash image size
uint8_t stepEvent(uint8_t event, uint8_t pin)
{
    return simpleEvent(event, pin);
}

uint8_t agcEvent(uint8_t event, uint8_t pin)
{
    return simpleEvent(event, pin);
}

uint8_t bandEvent(uint8_t event, uint8_t pin)
{
    if (event)
        g_lastInputMs = millis();
#if (0 != BAND_DELAY)
    static uint8_t count;
    if (BUTTONEVENT_ISLONGPRESS(event) && !g_settingsActive)
    {
        if (BUTTONEVENT_LONGPRESSDONE != event)
        {
            if (BUTTONEVENT_FIRSTLONGPRESS == event)
            {
                count = 0;
            }
            if (count++ == 0)
            {
                if (BAND_BUTTON == pin)
                {
                    if (g_bandIndex < g_lastBand)
                        bandSwitch(true);
                }
                else
                {
                    if (g_bandIndex)
                        bandSwitch(false);
                }
            }
            count = count % BAND_DELAY;
        }
    }
#else
    if (BUTTONEVENT_FIRSTLONGPRESS == event)
        event = BUTTONEVENT_SHORTPRESS;
#endif
    return event;
}

// Handle encoder direction
void rotaryEncoder()
{
    uint8_t encoderStatus = g_encoder.process();
    if (encoderStatus)
    {
        if (encoderStatus == DIR_CW)
        {
            if (g_encoderCount < ENCODER_MAX_BURST)
                g_encoderCount++;
        }
        else if (g_encoderCount > -ENCODER_MAX_BURST)
            g_encoderCount--;
        g_seekStop = true;
    }
}

//Saves more flash image size
void updateSSBCutoffFilter()
{
    // Auto mode: If SSB bandwidth 2 KHz or lower - it's better to enable cutoff filter
    if (g_Settings[SettingsIndex::CutoffFilter].param == 0 || g_currentMode == CW)
        g_si4735.setSSBSidebandCutoffFilter((g_bandwidthSSB[g_bwIndexSSB].idx == 0 || g_bandwidthSSB[g_bwIndexSSB].idx == 4 || g_bandwidthSSB[g_bwIndexSSB].idx == 5) ? 0 : 1);
    else
        g_si4735.setSSBSidebandCutoffFilter(g_Settings[SettingsIndex::CutoffFilter].param - 1);
}

//EEPROM Save
void saveAllReceiverInformation()
{
    uint8_t addr = EEPROM_DATA_START_ADDRESS;
    EEPROM.update(EEPROM_VERSION_ADDRESS, APP_VERSION);
    EEPROM.update(EEPROM_APP_ID_ADDRESS, EEPROM_APP_ID);

    EEPROM.update(addr++, g_muteVolume > 0 ? g_muteVolume : g_si4735.getVolume());
    EEPROM.update(addr++, g_bandIndex);
    EEPROM.update(addr++, g_currentMode);
    EEPROM.update(addr++, g_currentBFO >> 8);
    EEPROM.update(addr++, g_currentBFO & 0XFF);
    EEPROM.update(addr++, g_FMStepIndex);
    EEPROM.update(addr++, g_prevMode); 
    EEPROM.update(addr++, g_bwIndexSSB);
    EEPROM.update(addr++, g_stepIndexSSB);

    for (uint8_t i = 0; i <= g_lastBand; i++)
    {
        EEPROM.update(addr++, (g_bandList[i].currentFreq >> 8));
        EEPROM.update(addr++, (g_bandList[i].currentFreq & 0xFF));
        EEPROM.update(addr++, ((i != FM_BAND_TYPE && g_bandList[i].currentStepIdx >= g_amTotalSteps) ? 0 : g_bandList[i].currentStepIdx));
        EEPROM.update(addr++, g_bandList[i].bandwidthIdx);
    }

    g_Settings[SettingsIndex::CPUSpeed].param = 0;
    for (uint8_t i = 0; i < SettingsIndex::SETTINGS_MAX; i++)
        EEPROM.update(addr++, g_Settings[i].param);
}

//EEPROM Load
void readAllReceiverInformation()
{
    uint8_t addr = EEPROM_DATA_START_ADDRESS;
    int8_t bwIdx;
    g_volume = EEPROM.read(addr++);
    g_bandIndex = EEPROM.read(addr++);
    g_currentMode = EEPROM.read(addr++);
    uint8_t bfoHi = EEPROM.read(addr++);
    uint8_t bfoLo = EEPROM.read(addr++);
    g_currentBFO = (bfoHi << 8) | bfoLo;
    g_FMStepIndex = EEPROM.read(addr++);
    g_prevMode = EEPROM.read(addr++);
    g_bwIndexSSB = EEPROM.read(addr++);
    g_stepIndexSSB = EEPROM.read(addr++);
    if (g_stepIndexSSB < 0
        || g_stepIndexSSB > (int8_t)(g_amTotalSteps + g_ssbTotalSteps - 1)
        || (g_stepIndexSSB >= g_amTotalStepsSSB && g_stepIndexSSB < g_amTotalSteps))
        g_stepIndexSSB = 7;

    for (uint8_t i = 0; i <= g_lastBand; i++)
    {
        g_bandList[i].currentFreq = ((uint16_t)EEPROM.read(addr++) << 8);
        g_bandList[i].currentFreq |= EEPROM.read(addr++);
        g_bandList[i].currentStepIdx = EEPROM.read(addr++);
        g_bandList[i].bandwidthIdx = EEPROM.read(addr++);
    }

    for (uint8_t i = 0; i < SettingsIndex::SETTINGS_MAX; i++)
        g_Settings[i].param = EEPROM.read(addr++);
    g_Settings[SettingsIndex::CPUSpeed].param = 0;

    oled.setContrast(uint8_t(g_Settings[SettingsIndex::Brightness].param) * 2);

    g_previousFrequency = g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
    if (g_bandIndex == FM_BAND_TYPE)
        g_FMStepIndex = g_bandList[g_bandIndex].currentStepIdx;
    else
    {
        g_stepIndexAM = g_bandList[g_bandIndex].currentStepIdx;
        if (g_stepIndexAM >= g_amTotalSteps)
            g_stepIndexAM = 0;
    }
    bwIdx = g_bandList[g_bandIndex].bandwidthIdx;

    if (isSSB())
    {
        loadSSBPatch();
        g_si4735.setSSBAudioBandwidth(g_bandwidthSSB[g_bwIndexSSB].idx);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM)
    {
        g_bwIndexAM = bwIdx;
        g_si4735.setBandwidth(g_bandwidthAM[g_bwIndexAM].idx, 1);
    }
    else
    {
        g_bwIndexFM = bwIdx;
        g_si4735.setFmBandwidth(g_bwIndexFM);
    }

    applyBandConfiguration();
}

//For saving features
void resetEepromDelay()
{
    g_storeTime = millis();
    g_previousFrequency = 0;
}

//Draw frequency.
//BFO and main frequency produce actual frequency that is displayed on LCD
static uint8_t g_freqRightX;
static uint8_t g_stereoVis;

void showFrequency(bool cleanDisplay = false)
{
    if (g_settingsActive)
        return;

    char line[10];
    static char prevLine[10];
    static uint8_t prevOff = 255;
    static uint8_t prevMhzX = 255;
    uint16_t khzBFO = 0, tailBFO = 0;
    uint8_t i = 0;
    uint8_t len;

    line[0] = 0;

    if (g_bandIndex == FM_BAND_TYPE)
    {
        convertToChar(line, g_currentFrequency, 5, 3, '.', '/');
        len = ilen(g_currentFrequency);
    }
    else
    {
        if (!isSSB())
        {
            bool swMhz = g_Settings[SettingsIndex::SWUnits].param == 1;
            if (g_bandIndex == SW_BAND_TYPE && !swMhz)
            {
                len = ilen(g_currentFrequency);
                convertToChar(line, g_currentFrequency, len);
                line[len] = '.';
                line[len + 1] = '0';
                line[len + 2] = '0';
                line[len + 3] = 0;
            }
            else
            {
                convertToChar(line, g_currentFrequency, 5, (g_bandIndex == SW_BAND_TYPE && swMhz) ? 2 : 0, '.', '/');
                len = ilen(g_currentFrequency);
            }
        }
        else
        {
            splitFreq(khzBFO, tailBFO);
            len = ilen(khzBFO);
            uint8_t tailLen = ilen(tailBFO);
            convertToChar(line, khzBFO, len);
            line[len] = '.';
            line[len + 1] = '0';
            line[len + 2] = '0';
            convertToChar((tailLen == 1) ? &line[len + 2] : &line[len + 1], tailBFO, tailLen);
            line[len + 3] = 0;
        }
    }

    uint8_t n = 0, ink = 0;
    while (line[n])
    {
        ink = (uint8_t)(ink + oledFreqCharW(line[n]));
        n++;
    }
    uint8_t gap = 0;
    if (n >= 2)
    {
        if ((uint16_t)ink + (uint16_t)(n - 1) * FREQ_CELL_GAP <= 128)
            gap = FREQ_CELL_GAP;
        else if ((uint16_t)ink + (n - 1) <= 128)
            gap = 1;
    }
    uint8_t pixW = (n < 2) ? ink : (uint8_t)((uint16_t)ink + (uint16_t)(n - 1) * gap);
    bool showMhz = g_bandIndex == FM_BAND_TYPE
        || (g_bandIndex == SW_BAND_TYPE && g_Settings[SettingsIndex::SWUnits].param == 1 && !isSSB());
    uint8_t extra = showMhz ? (uint8_t)(FREQ_MHZ_GAP + MHZ_LABEL_W) : 0;
    uint16_t block = (uint16_t)pixW + extra;
    if (block > 128)
        block = 128;
    uint8_t off = (uint8_t)((128 - block) / 2);
    uint8_t pn = 0;
    while (prevLine[pn])
        pn++;
    bool full = cleanDisplay || prevOff != off || n != pn;

    g_freqRightX = (uint8_t)(off + pixW);
    uint8_t mhzX = (uint8_t)(g_freqRightX + FREQ_MHZ_GAP);

    if (full)
    {
        oledClear7segBand();
        prevOff = off;
        prevMhzX = 255;
    }
    else if (prevMhzX != 255 && (!showMhz || prevMhzX != mhzX))
    {
        oled.setCursor(prevMhzX, (uint8_t)(UI_PAGE_FREQ - 1));
        oled.fillLength(0, MHZ_LABEL_W);
        oled.setCursor(prevMhzX, UI_PAGE_FREQ);
        oled.fillLength(0, MHZ_LABEL_W);
        oled.setCursor(prevMhzX, UI_PAGE_FREQ + 1);
        oled.fillLength(0, MHZ_LABEL_W);
        oled.setCursor(prevMhzX, UI_PAGE_FREQ + 2);
        oled.fillLength(0, MHZ_LABEL_W);
        prevMhzX = 255;
    }

    uint8_t x = off;
    for (i = 0; i < n; i++)
    {
        if (full || line[i] != prevLine[i])
            oledBlit7segChar(x, line[i]);
        x = (uint8_t)(x + oledFreqCharW(line[i]));
        if (line[i + 1])
            x = (uint8_t)(x + gap);
    }
    i = 0;
    do
    {
        prevLine[i] = line[i];
    } while (line[i++]);

    if (showMhz)
        oledPrintMhz(mhzX);
    if (g_bandIndex == FM_BAND_TYPE)
        oledPrintStereoChip(mhzX, g_stereoVis);
    prevMhzX = showMhz ? mhzX : 255;
}

//This function is called by station seek logic
void showFrequencySeek(uint16_t freq)
{
    g_currentFrequency = freq;
    if (g_currentMode == FM)
    {
        //Fix random 10th KHz fraction
        freq = (freq / 10) * 10;
        g_currentFrequency = freq;
        g_si4735.setFrequency(g_currentFrequency);
    }
    else
        g_currentFrequency = g_si4735.getFrequency();

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    showFrequency();
}

bool checkStopSeeking()
{
    return g_seekStop || !(PINC & (1 << (ENCODER_BUTTON - 14)));
}

void doSeek()
{
    if (g_seekDirection)
        g_si4735.frequencyUp();
    else
        g_si4735.frequencyDown();

#if USE_RDS
    if (g_displayRDS)
        oledClearLine(UI_PAGE_SECONDARY);
#endif
    g_seekStop = false;
    g_si4735.seekStationProgress(showFrequencySeek, checkStopSeeking, g_seekDirection);
}

//Update and draw main screen UI. 
//basicUpdate - update minimum as possible
//cleanFreq   - force clean frequency line
void showStatus(bool cleanFreq)
{
    restoreIdleHeader();
    showFrequency(cleanFreq);
    g_sMeterDrawnVal = 255;
    if (!g_settingsActive)
        showSMeter();
#if USE_RDS
    if (g_currentMode == FM && g_displayRDS)
        showRDS();
#endif
}

void updateLowerDisplayLine()
{
    g_sMeterDrawnVal = 255;
    if (!g_settingsActive)
        showSMeter();
}

void radioValueToUI(char* buf, uint8_t idx)
{
    int8_t p = g_Settings[idx].param;
    uint8_t t = g_Settings[idx].type;
    if (idx == SettingsIndex::DisplayOff)
    {
        if (p <= 0)
        {
            buf[0] = 'O';
            buf[1] = 'F';
            buf[2] = 'F';
        }
        else if (p == 1)
        {
            buf[0] = '1';
            buf[1] = '5';
            buf[2] = 'S';
        }
        else if (p == 2)
        {
            buf[0] = '3';
            buf[1] = '0';
            buf[2] = 'S';
        }
        else if (p == 3)
        {
            buf[0] = '6';
            buf[1] = '0';
            buf[2] = 'S';
        }
        else
        {
            buf[0] = '2';
            buf[1] = 'M';
            buf[2] = ' ';
        }
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::DeEmp)
    {
        if (p == 0)
        {
            buf[0] = '5';
            buf[1] = '0';
        }
        else
        {
            buf[0] = '7';
            buf[1] = '5';
        }
        buf[2] = 'U';
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::SWUnits)
    {
        buf[0] = (p == 0) ? 'K' : 'M';
        buf[1] = 'H';
        buf[2] = 'Z';
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::SSM)
    {
        if (p == 0)
        {
            buf[0] = 'R';
            buf[1] = 'S';
            buf[2] = 'S';
        }
        else
        {
            buf[0] = 'S';
            buf[1] = 'N';
            buf[2] = 'R';
        }
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::CWSwitch)
    {
        buf[0] = (p == 0) ? 'L' : 'U';
        buf[1] = 'S';
        buf[2] = 'B';
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::CWPitch)
    {
        buf[0] = (char)('0' + p);
        buf[1] = '0';
        buf[2] = '0';
        buf[3] = 0;
        return;
    }
    if (idx == SettingsIndex::BFO)
    {
        buf[0] = (p < 0) ? '-' : '+';
        convertToChar(&buf[1], (uint16_t)((p < 0) ? -p : p), 3);
        return;
    }
    if (t == SettingType::ZeroAuto && p == 0)
        goto aut;
    if (t == SettingType::SwitchAuto)
    {
        if (p == 0)
            goto aut;
        if (p == 1)
            goto on;
        goto off;
    }
    if (t == SettingType::Switch)
    {
        if (p)
            goto on;
        goto off;
    }
    convertToChar(buf, (uint16_t)((p < 0) ? -p : (uint8_t)p), 3);
    if (p < 0)
        buf[0] = '-';
    return;
aut:
    buf[0] = 'A';
    buf[1] = 'U';
    buf[2] = 'T';
    buf[3] = 0;
    return;
on:
    buf[0] = 'O';
    buf[1] = 'N';
    buf[2] = ' ';
    buf[3] = 0;
    return;
off:
    buf[0] = 'O';
    buf[1] = 'F';
    buf[2] = 'F';
    buf[3] = 0;
}

//Draw curremt modulation
void overlayChip(bool filled)
{
    oledPrintModeBadge(g_ovVal, g_ovX, BADGE_PAD, g_ovVisForce, filled);
    g_ovFill = filled ? 1 : 0;
}

void paintTransient(const char* title, const char* value)
{
    uint8_t visForce = 0;
    if (title[0] == 'V' && title[1] == 'O')
        visForce = 2;
    else if (title[0] == 'B' && title[1] == 'R')
        visForce = 3;
    uint8_t i = 0;
    while (value[i] && i < 7)
    {
        g_ovVal[i] = value[i];
        i++;
    }
    g_ovVal[i] = 0;
    g_ovVisForce = visForce;
    uint8_t badgeW = visForce ? (uint8_t)(visForce * 8 + 2 * BADGE_PAD)
                              : oledBadgeWidth(value, BADGE_PAD);
    uint8_t badgeX = (uint8_t)(128 - badgeW);
    bool chrome = (g_uiLayer != UI_LAYER_TRANSIENT) || (badgeW != g_ovW);
    if (!chrome)
    {
        i = 0;
        while (title[i] && i < 4)
        {
            if (title[i] != g_ovTitle[i])
                chrome = true;
            i++;
        }
        if (g_ovTitle[i])
            chrome = true;
    }
    i = 0;
    while (title[i] && i < 4)
    {
        g_ovTitle[i] = title[i];
        i++;
    }
    g_ovTitle[i] = 0;
    g_ovX = badgeX;
    g_ovW = badgeW;

    if (chrome)
    {
        oled.setCursor(0, 0);
        oled.fillLength(0, 128);
        oled.setCursor(0, 1);
        oled.fillLength(0, 128);
        oledPrint(title, 0, 0, DEFAULT_FONT);
        uint8_t tw = 0;
        while (title[tw])
            tw++;
        oledSetFont(DEFAULT_FONT);
        uint8_t x = tw * 8;
        oled.setCursor(x, 0);
        while (x + 8 <= badgeX)
        {
            oled.write(pobIndex('.'));
            x += 8;
        }
    }
    overlayChip(true);
    g_uiLayer = UI_LAYER_TRANSIENT;
}

void showRadio()
{
    uint8_t idx = kRadioRing[g_radioSlot];
    char buf[5];
    radioValueToUI(buf, idx);
    paintTransient(g_Settings[idx].name, buf);
}

void radioEnter()
{
    uint8_t m = (uint8_t)(1u << g_currentMode);
    uint8_t s = 255;
    for (uint8_t i = 0; i < kRadioN; i++)
    {
        if (pgm_read_byte(&kRadioMask[i]) & m)
        {
            s = i;
            break;
        }
    }
    if (s == 255)
        return;
    g_radioSlot = s;
    switchCommand(CMD_RADIO, showRadio);
}

void showRadioError()
{
    oledPrint("RADIO ERR", 0, 0, DEFAULT_FONT, true);
}

void paintModeOrBand()
{
    if (g_currentCmd == CMD_MODE)
        paintTransient("MODE", g_bandModeDesc[g_modePick]);
    else if (g_currentCmd == CMD_BAND)
        paintTransient("BAND", (g_currentFrequency >= CB_LIMIT_LOW && g_currentFrequency < CB_LIMIT_HIGH) ? "CB" : bandTags[g_bandIndex]);
}

void showModulation()
{
    const char* lab = g_bandModeDesc[g_currentMode];
    if (g_muteVolume && (g_currentMode == FM || g_currentMode == AM))
        lab = "MUTE";
    oledPrintKarat(lab, 0);
    uint8_t sx = (uint8_t)(oledKaratTextW(lab) + 2);
    if (isSSB() && g_Settings[SettingsIndex::Sync].param == 1)
        oledPrintKarat("S", sx, true);
    else
        oledPrintKarat("", sx, false, 1);
}

//Draw volume level
void showVolume()
{
    if (g_settingsActive)
        return;

    char buf[3];
    if (g_muteVolume == 0)
        convertToChar(buf, g_si4735.getCurrentVolume(), 2, 0, 0);
    else
    {
        buf[0] = ' ';
        buf[1] = 'M';
        buf[2] = 0;
    }

    if (g_currentCmd != CMD_VOLUME)
        return;
    paintTransient("VOL", buf);
}

void showBrightness()
{
    if (g_settingsActive || g_currentCmd != CMD_BRIGHT)
        return;
    char buf[4];
    convertToChar(buf, (uint16_t)g_Settings[SettingsIndex::Brightness].param, 3);
    paintTransient("BRT", buf);
}

#if USE_RDS
void showRDS()
{
    static uint16_t lastUpdatedFreq = 0;
    static uint32_t lastUpdatedTime = millis();
    static bool succeed = false;

    if (g_currentMode != FM || !g_displayRDS || g_settingsActive)
    { 
        lastUpdatedFreq = 0;
        g_rdsPrevLen = 0;
        succeed = false;
        g_rdsActiveInfo = 0;
        return;
    }

    if (millis() - lastUpdatedTime > 300)
        succeed = false;

    if (lastUpdatedFreq != g_currentFrequency || g_rdsSwitchPressed)
    {
        if (g_rdsSwitchPressed)
        {
            g_rdsActiveInfo++;
            if (g_rdsActiveInfo > RDSActiveInfo::ProgramInfo)
                g_rdsActiveInfo = RDSActiveInfo::StationName;
        }
        else
        {
            g_rdsActiveInfo = RDSActiveInfo::StationName;
            succeed = false;
        }
        g_rdsPrevLen = 0;
        oledClearLine(UI_PAGE_SECONDARY);
    }
    lastUpdatedFreq = g_currentFrequency;

    if (!succeed)
        g_si4735.getRdsStatus();

    if (!succeed && g_si4735.getRdsReceived() && g_si4735.getRdsSync() && g_si4735.getNumRdsFifoUsed() > 1)
    {
        g_RDSCells[RDSActiveInfo::StationName] = g_si4735.getRdsStationName();
        g_RDSCells[RDSActiveInfo::StationInfo] = g_si4735.getRdsStationInformation();
        g_RDSCells[RDSActiveInfo::ProgramInfo] = g_si4735.getRdsProgramInformation();
        g_RDSCells[RDSActiveInfo::StationInfo][17] = '\0';
        g_RDSCells[RDSActiveInfo::ProgramInfo][17] = '\0';
        succeed = true;
        lastUpdatedTime = millis();
    }
    else if (!g_rdsSwitchPressed && succeed)
        return;

    uint8_t len = strlen8(g_RDSCells[g_rdsActiveInfo]);

    if (len == 0 && !g_rdsSwitchPressed)
        return;

    oledPrint(g_RDSCells[g_rdsActiveInfo], 0, UI_PAGE_SECONDARY, DEFAULT_FONT);
    
    uint8_t toPrint = len == 0 ? 3 : (len < g_rdsPrevLen ? min(g_rdsPrevLen - len, 16 - len) : 0);
    char printChar = len == 0 ? '.' : ' ';
    for (uint8_t i = 0; i < toPrint; i++) 
        oled.write(pobIndex((uint8_t)printChar));

    g_rdsPrevLen = len;
    g_rdsSwitchPressed = false;
}
#endif

//Draw steps (with units)
void showStep()
{
    if (g_displayRDS && g_currentCmd != CMD_STEP)
        return;

    char buf[5];
    if (g_currentMode == FM)
    {
        if (g_tabStepFM[g_FMStepIndex] == 100)
        {
            buf[0] = ' ';
            buf[1] = ' ';
            buf[2] = '1';
            buf[3] = 'M';
            buf[4] = 0x0;
        }
        else
        {
            convertToChar(buf, g_tabStepFM[g_FMStepIndex] * 10, 3);
            buf[3] = 'K';
            buf[4] = '\0';
        }
    }
    else
    {
        if (g_tabStep[activeStepIndex()] == 1000)
        {
            buf[0] = ' ';
            buf[1] = ' ';
            buf[2] = '1';
            buf[3] = 'M';
            buf[4] = 0x0;
        }
        else if (isSSB() && g_stepIndexSSB >= g_amTotalSteps)
            convertToChar(buf, g_tabStep[g_stepIndexSSB], 4);
        else
        {
            convertToChar(buf, g_tabStep[activeStepIndex()], 3);
            buf[3] = 'K';
            buf[4] = '\0';
        }
    }

    if (g_currentCmd != CMD_STEP)
        return;
    paintTransient("STEP", buf);
}

// 1C: sandwich black-white-black. liveX = cube edges (fill map), not S1…+20.
static void smApplyNeedle(uint8_t col, uint8_t peakX, uint8_t* d0, uint8_t* d1)
{
    uint8_t i = (uint8_t)(col - peakX);
    if (i >= SMETER_NEEDLE_W)
        return;
    if (i == 1)
    {
        *d0 = 0xFF;
        *d1 = 0xFF;
    }
    else
    {
        *d0 = 0;
        *d1 = 0;
    }
}

void showSMeter()
{
    if (g_settingsActive)
        return;

    static uint32_t sMeterUpdated = 0;
    static uint8_t peakQ = 0;
    static uint8_t smDrawnPeakX = 255;
    uint32_t now = millis();
    if (g_sMeterDrawnVal != 255 && now - sMeterUpdated < 100)
        return;
    sMeterUpdated = now;

    g_si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = g_si4735.getCurrentRSSI();
    handleSquelch(rssi);

    static uint8_t smHystRssi = 0;
    if (g_sMeterDrawnVal == 255)
        smHystRssi = rssi;
    else if (rssi > smHystRssi)
    {
        if ((uint8_t)(rssi - smHystRssi) >= SMETER_HYST_DB)
            smHystRssi = rssi;
    }
    else if ((uint8_t)(smHystRssi - rssi) >= SMETER_HYST_DB)
        smHystRssi = rssi;

    uint8_t sUnit;
    uint8_t plusDb = 0;
    if (smHystRssi >= S9_DBUV)
    {
        sUnit = 9;
        plusDb = smHystRssi - S9_DBUV;
        if (plusDb > SMETER_MAX_OVER_S9)
            plusDb = SMETER_MAX_OVER_S9;
    }
    else
    {
        sUnit = (uint8_t)((smHystRssi + 20) / 6);
        if (sUnit > 9)
            sUnit = 9;
    }

    // 0..9 = S-units. Any S9+ is at least 10 (last cube); 11..15 = +20..+60 dB for peak hold.
    uint8_t val = sUnit;
    if (plusDb)
    {
        val = 9 + plusDb / 10;
        if (val < 10)
            val = 10;
    }
    if (val > SMETER_LEVELS - 1)
        val = SMETER_LEVELS - 1;

    static uint8_t drawnDot = 255;
    uint8_t dot = 0;
    if (g_currentMode == FM)
    {
        g_fmStereo = g_si4735.getCurrentPilot();
        if (g_fmStereo)
            dot = 1;
    }
    bool dotDirty = (dot != drawnDot);

    uint8_t cue = 0;
    uint8_t aoffFm = 0;
    static uint8_t farOn = 0;
    static uint8_t searchBestSnr = 0;
    static uint8_t searchPrevSnr = 0;
    static uint8_t searchWay = 0;
    static uint8_t farDir = 0;
    static uint8_t farUp = 0;
    static uint8_t farDn = 0;
    static uint16_t lastCueF = 0;
    uint8_t ssb = isSSB();
    if (!g_processFreqChange && !ssb
        && (now - g_lastFreqChange) >= FREQOFF_CUE_MS)
    {
        int8_t off = (int8_t)g_si4735.getCurrentSignedFrequencyOffset();
        if (g_currentMode == FM)
        {
            uint8_t snr = g_si4735.getCurrentSNR();
            aoffFm = (off < 0) ? (uint8_t)(-off) : (uint8_t)off;
            if (aoffFm >= FREQOFF_FAR_OFF)
            {
                if (snr < FREQOFF_SNR_FLOOR)
                    farOn = 0;
                else
                {
                    uint8_t way = g_seekDirection ? 1 : 0;
                    if (!farOn || searchWay != way)
                    {
                        farOn = 1;
                        searchWay = way;
                        searchBestSnr = searchPrevSnr = snr;
                        farUp = farDn = 0;
                        farDir = way ? 3 : 1;
                    }
                    if (lastCueF != g_currentFrequency)
                    {
                        if (snr > (uint8_t)(searchBestSnr + 1))
                            searchBestSnr = snr;
                        if (snr > (uint8_t)(searchPrevSnr + 1))
                        {
                            farUp++;
                            farDn = 0;
                        }
                        else if ((uint8_t)(snr + 1) < searchPrevSnr)
                        {
                            farDn++;
                            farUp = 0;
                        }
                        else
                            farUp = farDn = 0;
                        if (farUp >= 2)
                            farDir = way ? 3 : 1;
                        else if (farDn >= 2
                            && (uint8_t)(snr + 2) < searchBestSnr)
                            farDir = way ? 1 : 3;
                        searchPrevSnr = snr;
                    }
                    cue = farDir;
                }
            }
            else
            {
                farOn = 0;
                bool valid = g_si4735.getCurrentValidChannel();
                if (aoffFm > FREQOFF_CUE_FM_KHZ
                    || g_si4735.getCurrentAfcRailIndicator())
                    cue = (off < 0) ? 3 : 1;
                else if (valid)
                    cue = 2;
            }
        }
        else if (off < -FREQOFF_CUE_KHZ)
            cue = 1;
        else if (off > FREQOFF_CUE_KHZ)
            cue = 3;
        else
            cue = 2;
        lastCueF = g_currentFrequency;
    }
    if ((cue == 1 || cue == 3) && aoffFm < FREQOFF_FAR_OFF && !(now & 512u))
        cue = 0;

    static uint16_t tunedF = 0;
    uint8_t wantTuned = 0;
    if (g_currentMode == FM)
    {
        if (tunedF && g_currentFrequency != tunedF)
            tunedF = 0;
        if (cue == 2)
            tunedF = g_currentFrequency;
        if (tunedF)
        {
            wantTuned = (uint8_t)!g_uiLayer;
            cue = 0;
        }
    }
    else
        tunedF = 0;
    if (wantTuned != g_tunedDrawn)
    {
        g_tunedDrawn = wantTuned;
        if (!g_uiLayer)
            restoreIdleHeader();
    }

    uint8_t barW = smBarW();
    uint8_t barMax = (uint8_t)(barW - SMETER_NEEDLE_W);
    uint8_t cur = (val >= 10) ? SMETER_CUBES : (uint8_t)((val * (SMETER_CUBES - 1) + 4) / 9);
    uint8_t liveX = smBarLiveX(smHystRssi, sUnit, plusDb, cur, barMax);
    uint8_t liveQ = (uint8_t)((uint16_t)liveX * SMETER_PEAK_NUM / SMETER_PEAK_DEN);
    if (g_sMeterDrawnVal == 255)
        peakQ = liveQ;
    else if (liveQ > peakQ)
    {
        uint8_t d = (uint8_t)((liveQ - peakQ + 2) / 3);
        if (d == 0)
            d = 1;
        uint8_t n = (uint8_t)(peakQ + d);
        peakQ = (n > liveQ) ? liveQ : n;
    }
    else if (peakQ > liveQ)
    {
        uint8_t d = (uint8_t)((peakQ - liveQ + 6) / 7);
        if (d == 0)
            d = 1;
        peakQ = (uint8_t)(peakQ - d);
    }
    uint8_t peakX = (uint8_t)(((uint16_t)peakQ * SMETER_PEAK_DEN + (SMETER_PEAK_NUM / 2)) / SMETER_PEAK_NUM);
    if (peakX > barMax)
        peakX = barMax;
    uint8_t x0 = (uint8_t)(SMETER_LAB_X + SMETER_LAB_W + SMETER_LAB_GAP);
    static uint8_t prevBarX = 255;
    static uint8_t prevAux = 255;
    static uint8_t prevAuxSnr = 255;
    uint8_t mhzX = (uint8_t)(g_freqRightX + FREQ_MHZ_GAP);
    static uint8_t stereoX = 255;
    g_stereoVis = (g_bandIndex == FM_BAND_TYPE) ? dot : 0;
    if (g_bandIndex == FM_BAND_TYPE)
    {
        oledPrintStereoChip(mhzX, g_stereoVis);
        stereoX = mhzX;
    }
    else if (stereoX != 255)
    {
        oledPrintStereoChip(stereoX, false);
        stereoX = 255;
    }

    uint8_t auxKind = g_auxInd;
    uint8_t auxSnr = g_auxIndSnr;
    if (auxKind == AUX_IND_AUTO)
    {
        if (g_bandIndex == SW_BAND_TYPE)
        {
            auxKind = AUX_IND_SNR;
            auxSnr = g_si4735.getCurrentSNR();
        }
        else if (cue)
            auxKind = cue;
        else
            auxKind = AUX_IND_EMPTY;
    }
    if (auxSnr > 99)
        auxSnr = 99;

    if (val == g_sMeterDrawnVal && peakX == smDrawnPeakX && !dotDirty && x0 == prevBarX
        && auxKind == prevAux && (auxKind != AUX_IND_SNR || auxSnr == prevAuxSnr))
    {
        drawnDot = dot;
        return;
    }

    {
        oledPrintSMeterLab(sUnit, plusDb != 0, g_sMeterDrawnVal == 255);
        uint8_t barEnd = (uint8_t)(x0 + barW);
        uint8_t wx = auxWinX(barEnd);
        uint8_t ww = auxWinW(barEnd);
        for (uint8_t pg = 0; pg < 2; pg++)
        {
            oled.setCursor((uint8_t)(SMETER_LAB_X + SMETER_LAB_W), UI_PAGE_SECONDARY + pg);
            oled.startData();
            for (uint8_t z = 0; z < SMETER_LAB_GAP; z++)
                oled.sendData(0);
            uint8_t col = 0;
            for (uint8_t i = 0; i < SMETER_CUBES; i++)
            {
                bool fill = (i < cur);
                uint8_t sw = smSegW(i);
                for (uint8_t w = 0; w < sw; w++)
                {
                    bool side = (w == 0 || w == (uint8_t)(sw - 1));
                    uint8_t d0, d1;
                    smHornCol(i, fill, side, &d0, &d1);
                    smApplyNeedle(col, peakX, &d0, &d1);
                    oled.sendData(pg ? d1 : d0);
                    col++;
                }
                if (i != SMETER_CUBES - 1)
                {
                    uint8_t gap = smGapAfter(i);
                    for (uint8_t z = 0; z < gap; z++)
                    {
                        uint8_t d0 = 0, d1 = 0;
                        smApplyNeedle(col, peakX, &d0, &d1);
                        oled.sendData(pg ? d1 : d0);
                        col++;
                    }
                }
            }
            oled.repeatData(0, AUX_WIN_GAP);
            if (ww)
            {
                for (uint8_t lx = 0; lx < ww; lx++)
                    oled.sendData(auxIndCol(lx, ww, pg, auxKind, auxSnr));
            }
            uint8_t used = (uint8_t)(barEnd + AUX_WIN_GAP + ww);
            if (used < 128)
                oled.repeatData(0, (uint8_t)(128 - used));
            oled.endData();
        }
        g_sMeterDrawnVal = val;
        smDrawnPeakX = peakX;
        drawnDot = dot;
        prevBarX = x0;
        prevAux = auxKind;
        prevAuxSnr = auxSnr;
    }
}

//Draw bandwidth (Ignored for CW mode)
void showBandwidth()
{
    const char* bw;
    if (isSSB())
    {
        bw = g_bandwidthSSB[g_bwIndexSSB].desc;
        if (g_currentMode == CW)
            bw = "    ";
    }
    else if (g_currentMode == AM)
    {
        bw = g_bandwidthAM[g_bwIndexAM].desc;
    }
    else
    {
        bw = g_bandwidthFM[g_bwIndexFM];
    }

    if (g_currentCmd == CMD_BW)
    {
        paintTransient("BW", bw);
        return;
    }
    if (g_currentMode == CW)
        bw = g_bandwidthSSB[0].desc;
    uint8_t w = oledKaratTextW(bw);
    uint8_t x = (uint8_t)((128 - w) / 2);
    if (g_tunedDrawn)
        x -= 8;
    oledPrintKarat(bw, x);
}

void restoreIdleHeader()
{
    g_uiLayer = UI_LAYER_NORMAL;
    g_sMeterDrawnVal = 255;
    oled.setCursor(0, 0);
    oled.fillLength(0, 128);
    oled.setCursor(0, 1);
    oled.fillLength(0, 128);
    showModulation();
    showBandwidth();
    if (g_tunedDrawn)
        oledPrintKarat("TUNED", (uint8_t)(128 - oledKaratTextW("TUNED")));
    showVolume();
}

void formatBfo(char* buf)
{
    int16_t b = g_currentBFO;
    buf[0] = (b < 0) ? '-' : '+';
    if (b < 0)
        b = -b;
    convertToChar(&buf[1], (uint16_t)b, 5);
    buf[6] = 0;
}

void paintBfoTransient()
{
    char buf[7];
    formatBfo(buf);
    paintTransient("BFO", buf);
}

void cycleEncoderFocus()
{
    uint8_t next = g_uiFocus + 1;
    if (next == FOCUS_BFO && !isSSB())
        next++;
    if (next > FOCUS_BFO)
        next = FOCUS_FREQ;

    g_currentCmd = CMD_NONE;
    g_volFromButtons = false;
    restoreIdleHeader();
    g_uiFocus = next;
    showStep();
    showVolume();

    if (next == FOCUS_FREQ)
    {
        g_lastAdjustmentTime = 0;
        return;
    }

    g_lastAdjustmentTime = millis();
    if (next == FOCUS_STEP)
    {
        g_uiLayer = UI_LAYER_FOCUS;
        g_currentCmd = CMD_STEP;
        showStep();
    }
    else if (next == FOCUS_VOL)
    {
        g_volFromButtons = false;
        g_uiLayer = UI_LAYER_FOCUS;
        g_currentCmd = CMD_VOLUME;
        showVolume();
    }
    else
        paintBfoTransient();
}

void uiFlush()
{
    if (!g_uiDirty || g_settingsActive)
    {
        g_uiDirty = 0;
        g_uiFreqClean = false;
        return;
    }
    if (g_uiDirty & UI_FREQ)
        showFrequency(g_uiFreqClean);
    if (g_uiLayer != UI_LAYER_TRANSIENT)
    {
        if (g_uiDirty & UI_MOD)
            showModulation();
        if (g_uiDirty & UI_VOL)
            showVolume();
        if (g_uiDirty & UI_BW)
            showBandwidth();
    }
    if (g_uiDirty & UI_STEP)
        showStep();
    g_uiDirty = 0;
    g_uiFreqClean = false;
}

uint16_t getNextSWSuBband(bool up)
{
    uint16_t freq = g_currentFrequency;
    if (isSSB())
        freq += g_currentBFO / 1000;

    for (uint8_t i = 0; i < g_SWSubBandCount; i++)
    {
        uint8_t n = g_SWSubBandCount - 1 - i;
        if (!up && SWSubBands[n] < freq)
            return SWSubBands[n];
        else if (up && SWSubBands[i] > freq)
            return SWSubBands[i];
    }

    return 0;
}

void bandSwitch(bool up)
{
    uint16_t nextSW = getNextSWSuBband(up);

    if (g_bandIndex == SW_BAND_TYPE && nextSW != 0)
    {
        g_currentFrequency = nextSW;
        g_currentBFO = 0;
        if (isSSB())
            updateBFO();
        g_si4735.setFrequency(nextSW);
        agcSetFunc();
        uiMark(UI_FREQ, true);
    }
    else
    {
        if (g_currentMode == FM)
            g_bandList[g_bandIndex].currentStepIdx = g_FMStepIndex;
        else
            g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;

        if (up)
        {
            if (g_bandIndex < g_lastBand)
                g_bandIndex++;
            else
                g_bandIndex = 0;
        }
        else
        {
            if (g_bandIndex > 0)
                g_bandIndex--;
            else
                g_bandIndex = g_lastBand;
        }

#if USE_RDS
        if (g_displayRDS && g_currentMode != FM)
        {
            g_displayRDS = false;
            oledClearLine(UI_PAGE_SECONDARY);
        }
#endif

        g_currentBFO = 0;
        if (isSSB())
            updateBFO();
        applyBandConfiguration();
    }
}

#if USE_RDS
void setRDSConfig(uint8_t bias)
{
    g_si4735.setRdsConfig(1, bias, bias, bias, bias);
}
#endif

//Step value regulation
void doStep(int8_t v)
{
    if (g_currentMode == FM)
    {
        g_FMStepIndex = (v > 0) ? g_FMStepIndex + 1 : g_FMStepIndex - 1;
        if (g_FMStepIndex > g_lastStepFM)
            g_FMStepIndex = 0;
        else if (g_FMStepIndex < 0)
            g_FMStepIndex = g_lastStepFM;

        g_si4735.setFrequencyStep(g_tabStepFM[g_FMStepIndex]);
        g_bandList[g_bandIndex].currentStepIdx = g_FMStepIndex;
        g_si4735.setSeekFmSpacing(1);
        showStep();
    }
    else
    {
        int8_t& st = activeStepIndex();
        st = (v > 0) ? st + 1 : st - 1;
        if (st > getLastStep())
            st = 0;
        else if (st < 0)
            st = getLastStep();
        else if (isSSB() && st >= g_amTotalStepsSSB && st < g_amTotalSteps)
            st = v > 0 ? g_amTotalSteps : g_amTotalStepsSSB - 1;

        if (!isSSB())
        {
            g_si4735.setFrequencyStep(g_tabStep[st]);
            g_bandList[g_bandIndex].currentStepIdx = st;
            g_si4735.setSeekAmSpacing((st >= g_amTotalSteps) ? 1 : g_tabStep[st]);
        }
        else if (st < g_amTotalSteps)
            g_si4735.setFrequencyStep(g_tabStep[st]);
        showStep();
    }
}

void setMuted(bool on)
{
    if (on == (g_muteVolume != 0))
        return;
    if (on)
    {
        uint8_t vol = g_si4735.getCurrentVolume();
        if (!vol)
            return;
        g_muteVolume = vol;
        g_si4735.setVolume(0);
    }
    else
    {
        g_si4735.setVolume(g_muteVolume);
        g_muteVolume = 0;
        applySquelchNow();
    }
    if (!g_settingsActive)
        showStatus(true);
}

void doVolume(int8_t v)
{
    if (g_muteVolume)
        setMuted(false);
    else
    {
        uint8_t steps = (v > 0) ? v : -v;
        while (steps--)
        {
            if (v > 0)
                g_si4735.volumeUp();
            else
                g_si4735.volumeDown();
        }
    }
    if (g_currentCmd == CMD_VOLUME)
    {
        g_lastAdjustmentTime = millis();
        showVolume();
    }
}

//Helps to save more flash image size
void doSwitchLogic(int8_t& param, int8_t low, int8_t high, int8_t step)
{
    param += step;
    if (param < low)
        param = high;
    else if (param > high)
        param = low;
}

void agcSetFunc()
{
    uint8_t att = g_Settings[SettingsIndex::ATT].param;
    uint8_t disableAgc = att > 0;
    uint8_t agcNdx;
    if (att > 1) 
        agcNdx = att - 1;
    else
        agcNdx = 0;
    g_si4735.setAutomaticGainControl(disableAgc, agcNdx);
}

//Settings: Attenuation
void doAttenuation(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::ATT].param, 0, 37, v);
    agcSetFunc();
}

//Settings: Soft Mute
void doSoftMute(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::SoftMute].param, 0, 32, v);

    if (g_currentMode != FM)
        g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
}

//Settings: Brightness
void doBrightness(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::Brightness].param, 5, 125, v);
    oled.setContrast(uint8_t(g_Settings[SettingsIndex::Brightness].param) * 2);
}

//Settings: SSB AVC Switch
void doSSBAVC(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::SVC].param, 0, 1, v);

    if (isSSB())
    {
        g_si4735.setSSBAutomaticVolumeControl(g_Settings[SettingsIndex::SVC].param);
        applyBandConfiguration(true);
    }
}

//Settings: Automatic Volume Control
void doAvc(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::AutoVolControl].param, 12, 90, v);

    if (g_currentMode != FM)
        g_si4735.setAvcAmMaxGain(g_Settings[SettingsIndex::AutoVolControl].param);
}

//Settings: Sync switch
void doSync(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::Sync].param, 0, 1, v);

    if (isSSB())
    {
        g_si4735.setSSBDspAfc(g_Settings[SettingsIndex::Sync].param == 1 ? 0 : 1);
        g_si4735.setSSBAvcDivider(g_Settings[SettingsIndex::Sync].param == 0 ? 0 : 3); //Set Sync mode
        applyBandConfiguration(true);
    }
}

//Settings: FM DeEmp switch (50 or 75)
void doDeEmp(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::DeEmp].param, 0, 1, v);

    if (g_currentMode == FM)
        g_si4735.setFMDeEmphasis(g_Settings[SettingsIndex::DeEmp].param == 0 ? 1 : 2);
}

//Settings: SW Units
void doSWUnits(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::SWUnits].param, 0, 1, v);
}

//Settings: SW Units
void doSSBSoftMuteMode(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::SSM].param, 0, 1, v);

    if (isSSB())
        g_si4735.setSSBSoftMute(g_Settings[SettingsIndex::SSM].param);
}

//Settings: SSB Cutoff filter
void doCutoffFilter(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::CutoffFilter].param, 0, 2, v);

    if (isSSB())
        updateSSBCutoffFilter();
}

// EEPROM slot kept; runtime CLKPR removed (always 16 MHz).
void doCPUSpeed(int8_t)
{
    g_Settings[SettingsIndex::CPUSpeed].param = 0;
}

void doDisplayOff(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::DisplayOff].param, 0, DISPLAY_OFF_MAX, v);
    if (g_Settings[SettingsIndex::DisplayOff].param == 0 && !g_displayOn)
    {
        g_displayOn = true;
        displayPower(true);
    }
    g_lastInputMs = millis();
}

//Settings: BFO Offset calibration
void doBFOCalibration(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::BFO].param, -60, 60, v);

    if (isSSB())
    {
#if USE_RDS
        setRDSConfig(g_Settings[SettingsIndex::BFO].param);
#endif
        updateBFO();
    }
}

//Settings: Tune Frequency Antenna Capacitor
void doUnitsSwitch(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::UnitsSwitch].param, 0, 1, v);
}

//Settings: Scan button switch
void doScanSwitch(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::ScanSwitch].param, 0, 1, v);
}

//Settings: CW mode switch
void doCWSwitch(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::CWSwitch].param, 0, 1, v);

    if (g_currentMode == CW)
        applyBandConfiguration(true);
}

void doCWPitch(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::CWPitch].param, 5, 8, v);
    if (g_currentMode == CW)
        updateBFO();
}

void doANB(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::ANB].param, 0, 1, v);
    applyAMNoiseBlanker();
}

void doSQL(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::SQL].param, 0, 60, v);
    applySquelchNow();
}

#if USE_RDS
//Settings: RDS Error Level
void doRDSErrorLevel(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::RDSError].param, 0, 3, v);

    if (g_currentMode == FM)
        setRDSConfig(g_Settings[SettingsIndex::RDSError].param);
}


void doRDS()
{
    g_displayRDS = !g_displayRDS;

    if (g_displayRDS)
    {
        oledClearLine(UI_PAGE_SECONDARY);
        g_si4735.getRdsStatus();
        showRDS();
    }
    else
        updateLowerDisplayLine();
}
#endif

//Prevents repeatable code for flash image size saving
void doBandwidthLogic(int8_t& bwIndex, uint8_t upperLimit, uint8_t v)
{
    doSwitchLogic(bwIndex, 0, upperLimit, v);
    g_bandList[g_bandIndex].bandwidthIdx = bwIndex;
}

//Bandwidth regulation logic
void doBandwidth(uint8_t v)
{
    if (isSSB())
    {
        doSwitchLogic(g_bwIndexSSB, 0, g_bwSSBMaxIdx, v);
        g_si4735.setSSBAudioBandwidth(g_bandwidthSSB[g_bwIndexSSB].idx);
        updateSSBCutoffFilter();
    }
    else if (g_currentMode == AM)
    {
        doBandwidthLogic(g_bwIndexAM, g_maxFilterAM, v);
        g_bandList[g_bandIndex].bandwidthIdx = g_bwIndexAM;
        g_si4735.setBandwidth(g_bandwidthAM[g_bwIndexAM].idx, 1);
    }
    else
    {
        doBandwidthLogic(g_bwIndexFM, 4, v);
        g_bandList[g_bandIndex].bandwidthIdx = g_bwIndexFM;
        g_si4735.setFmBandwidth(g_bwIndexFM);
    }
    showBandwidth();
}

void switchCommand(uint8_t cmd, void (*showFunction)())
{
    uint8_t prev = g_currentCmd;

    if (cmd == CMD_NONE)
    {
        g_currentCmd = CMD_NONE;
        g_lastAdjustmentTime = 0;
        g_volFromButtons = false;
        g_uiFocus = FOCUS_FREQ;
        restoreIdleHeader();
        showStep();
        showVolume();
        if (prev == CMD_BRIGHT || prev == CMD_RADIO)
            saveAllReceiverInformation();
        return;
    }

    bool last = (g_currentCmd == cmd);

    if (!last)
    {
        g_currentCmd = CMD_NONE;
        g_lastAdjustmentTime = millis();
        showVolume();
        showStep();
        showBandwidth();
        showModulation();
    }
    else
        g_lastAdjustmentTime = 0;

    g_currentCmd = last ? CMD_NONE : cmd;

    if (g_currentCmd)
        g_uiLayer = UI_LAYER_FOCUS;

    if (showFunction)
        showFunction();
    if (g_currentCmd == CMD_BAND || g_currentCmd == CMD_MODE)
        paintModeOrBand();
    if ((prev == CMD_BRIGHT && g_currentCmd != CMD_BRIGHT)
        || (prev == CMD_RADIO && g_currentCmd != CMD_RADIO))
        saveAllReceiverInformation();
}

void resetLowerLine()
{
    if (g_displayRDS)
    {
        g_displayRDS = false;
        updateLowerDisplayLine();
    }
}

static const uint8_t kDispOffSec[] = { 0, 15, 30, 60, 120 };

void displayPower(bool on)
{
    if (on)
    {
        oled.enableChargePump();
        oled.on();
    }
    else
    {
        oled.disableChargePump();
        oled.off();
    }
}

void displayWake()
{
    if (!g_displayOn)
    {
        g_displayOn = true;
        displayPower(true);
        restoreIdleHeader();
    }
    g_lastInputMs = millis();
}

void displaySleepIfDue()
{
    uint8_t p = (uint8_t)g_Settings[SettingsIndex::DisplayOff].param;
    if (p == 0 || p > DISPLAY_OFF_MAX || !g_displayOn || g_settingsActive)
        return;
    if ((millis() - g_lastInputMs) < (uint32_t)kDispOffSec[p] * 1000UL)
        return;
    g_displayOn = false;
    displayPower(false);
}

void cycleModePick(int8_t dir)
{
    if (g_currentMode == FM || dir == 0)
        return;
    uint8_t m = g_modePick;
    if (dir > 0)
    {
        if (m == AM)
            m = LSB;
        else if (m == LSB)
            m = USB;
        else if (m == USB)
            m = CW;
        else
            m = AM;
    }
    else
    {
        if (m == AM)
            m = CW;
        else if (m == LSB)
            m = AM;
        else if (m == USB)
            m = LSB;
        else
            m = USB;
    }
    g_modePick = m;
}

void commitModePick()
{
    if (g_modePick == g_currentMode || g_currentMode == FM)
        return;

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    g_prevMode = g_currentMode;

    bool fromSsb = isSSB();
    bool toSsb = (g_modePick == LSB || g_modePick == USB || g_modePick == CW);

    if (fromSsb)
    {
        g_bandList[g_bandIndex].currentFreq += g_currentBFO / 1000;
        if (!toSsb)
            g_currentFrequency += (g_currentBFO / 1000);
    }

    if (toSsb && !g_ssbLoaded)
    {
        loadSSBPatch();
        g_processFreqChange = false;
    }
    if (!toSsb)
        g_ssbLoaded = false;

    g_currentMode = g_modePick;
    g_bandList[g_bandIndex].currentStepIdx = g_stepIndexAM;
    applyBandConfiguration();
}

void loop()
{
    uint8_t x;
    const bool screenWasOff = !g_displayOn;

    // Input first: encoder never waits on buttons, RDS, or S-meter.
    if (g_encoderCount != 0)
    {
        if (screenWasOff)
        {
            displayWake();
            g_encoderCount = 0;
            goto saveAttempt;
        }
        g_lastInputMs = millis();

        if (g_lastAdjustmentTime != 0
            && !(g_currentCmd == CMD_VOLUME && g_volFromButtons))
            g_lastAdjustmentTime = millis();

        if (g_radioError)
            ;
        else if (g_currentCmd == CMD_VOLUME && !g_volFromButtons)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doVolume(g_encoderCount);
        }
        else if (g_currentCmd == CMD_BRIGHT)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doBrightness(g_encoderCount);
            showBrightness();
        }
        else if (g_currentCmd == CMD_STEP)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doStep(g_encoderCount);
        }
        else if (g_currentCmd == CMD_BW)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doBandwidth(g_encoderCount);
        }
        else if (g_currentCmd == CMD_BAND)
        {
            if (g_encoderCount > 0)
                bandSwitch(true);
            else
                bandSwitch(false);
            paintModeOrBand();
        }
        else if (g_currentCmd == CMD_MODE)
        {
            cycleModePick(g_encoderCount);
            paintModeOrBand();
        }
        else if (g_currentCmd == CMD_RADIO)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            (*g_Settings[kRadioRing[g_radioSlot]].manipulateCallback)(g_encoderCount);
            showRadio();
        }
        else if (g_uiFocus == FOCUS_BFO && isSSB())
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doFrequencyTuneSSB();
            paintBfoTransient();
        }
        else if (isSSB())
            doFrequencyTuneSSB();
        else
            doFrequencyTune();
        g_encoderCount = 0;
        resetEepromDelay();
        uiFlush();
        goto saveAttempt;
    }

    if (screenWasOff)
    {
        uint8_t poke = 0;
        poke |= btn_Bandwidth.checkEvent(simpleEvent);
        poke |= btn_BandUp.checkEvent(simpleEvent);
        poke |= btn_BandDn.checkEvent(simpleEvent);
        poke |= btn_VolumeUp.checkEvent(simpleEvent);
        poke |= btn_VolumeDn.checkEvent(simpleEvent);
        poke |= btn_Encoder.checkEvent(simpleEvent);
        poke |= btn_Step.checkEvent(simpleEvent);
        poke |= btn_Mode.checkEvent(modeBtnEvent);
        uint8_t agcOff = btn_AGC.checkEvent(agcEvent);
        if (BUTTONEVENT_SHORTPRESS == agcOff)
            displayWake();
        else if (BUTTONEVENT_FIRSTLONGPRESS == agcOff)
        {
            displayWake();
            if (!g_settingsActive)
            {
                switchCommand(CMD_BRIGHT, showBrightness);
                g_uiLayer = UI_LAYER_TRANSIENT;
            }
        }
        else if (poke)
            displayWake();
        else if (millis() - g_lastFreqChange >= BACKGROUND_UI_MS && !g_radioError)
            applySquelchNow();
        goto saveAttempt;
    }

    servicePendingTune();
    uiFlush();

    if (!g_radioError && !g_settingsActive)
        showSMeter();
    if (!g_radioError && millis() - g_lastFreqChange >= BACKGROUND_UI_MS)
    {
#if USE_RDS
        showRDS();
#endif
    }

    if (g_uiLayer == UI_LAYER_TRANSIENT && g_lastAdjustmentTime != 0)
    {
        uint32_t idle = (g_currentCmd == CMD_RADIO)
            ? (uint32_t)CAVE_ACTIVE_TIMEOUT
            : (uint32_t)ADJUSTMENT_ACTIVE_TIMEOUT;
        uint32_t dt = millis() - g_lastAdjustmentTime;
        if (dt >= idle + OVERLAY_BLINK_MS)
        {
            if (g_currentCmd == CMD_MODE)
                commitModePick();
            switchCommand();
        }
        else if (dt > idle)
        {
            uint16_t ph = (uint16_t)(dt - idle);
            bool fill = (ph < OVERLAY_FLASH_MS)
                || (ph >= (uint16_t)(OVERLAY_FLASH_MS + OVERLAY_FLASH_GAP_MS));
            if ((uint8_t)(fill ? 1 : 0) != g_ovFill)
                overlayChip(fill);
        }
    }

    //Command-checkers
    if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(simpleEvent))
    {
        if (!g_settingsActive && g_currentMode != CW)
            switchCommand(CMD_BW, showBandwidth);
    }
    if (BUTTONEVENT_SHORTPRESS == btn_BandUp.checkEvent(bandEvent))
    {
        resetLowerLine();
        switchCommand(CMD_BAND, showModulation);
    }
    if (BUTTONEVENT_SHORTPRESS == btn_BandDn.checkEvent(bandEvent))
        switchCommand();
    if (BUTTONEVENT_SHORTPRESS == btn_VolumeUp.checkEvent(volumeEvent))
    {
        if (!g_settingsActive && g_muteVolume == 0)
        {
            g_volFromButtons = true;
            switchCommand(CMD_VOLUME, showVolume);
        }
    }
    if (BUTTONEVENT_SHORTPRESS == btn_VolumeDn.checkEvent(volumeEvent))
    {
        if (g_currentCmd != CMD_VOLUME)
            setMuted(g_muteVolume == 0);
    }
    uint8_t encEvent = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == encEvent)
    {
        if (g_radioError)
        {
            retryRadio();
            showStatus();
        }
        else if (g_uiLayer == UI_LAYER_TRANSIENT)
        {
            if (g_currentCmd == CMD_MODE)
                commitModePick();
            switchCommand();
        }
        else if (g_currentMode == FM || g_currentMode == AM)
            setMuted(g_muteVolume == 0);
        else
            cycleEncoderFocus();
    }
    else if (BUTTONEVENT_LONGPRESSDONE == encEvent)
    {
        if (g_currentMode == FM || g_currentMode == AM)
        {
            if (g_muteVolume)
                setMuted(false);
            else
                doSeek();
        }
        else
            switchCommand();
    }

    //This is a hack, it allows SHORTPRESS and LONGPRESS events
    //Be processed without complicated overhead
    //It requires to save checkEvent result into a variable
    //That has exact same name as event processing function for this button
    uint8_t agcEvent = btn_AGC.checkEvent(agcEvent);
    if (BUTTONEVENT_SHORTPRESS == agcEvent)
    {
        if (g_displayOn)
        {
            if (g_currentCmd != CMD_NONE)
                switchCommand();
            g_displayOn = false;
            displayPower(false);
        }
        else
        {
            g_displayOn = true;
            displayPower(true);
        }
        g_lastInputMs = millis();
    }
    if (BUTTONEVENT_FIRSTLONGPRESS == agcEvent)
    {
        if (g_currentCmd == CMD_BRIGHT)
            switchCommand();
        else
        {
            if (!g_displayOn)
            {
                g_displayOn = true;
                displayPower(true);
            }
            switchCommand(CMD_BRIGHT, showBrightness);
            g_uiLayer = UI_LAYER_TRANSIENT;
        }
    }
    uint8_t stepEvent = btn_Step.checkEvent(stepEvent);
    if (BUTTONEVENT_SHORTPRESS == stepEvent)
    {
        if (!g_settingsActive)
        {
            switchCommand(CMD_STEP, showStep);
            resetLowerLine();
        }
    }
    if (BUTTONEVENT_LONGPRESSDONE == stepEvent)
    {
        if (!g_settingsActive)
        {
            g_sMeterDrawnVal = 255;
            showSMeter();
        }
    }
    uint8_t modeEvent = btn_Mode.checkEvent(modeBtnEvent);
    if (BUTTONEVENT_SHORTPRESS == modeEvent)
    {
        if (g_currentCmd == CMD_RADIO)
        {
            uint8_t n = radioNextSlot(g_radioSlot);
            if (n != 255)
            {
                g_radioSlot = n;
                g_lastAdjustmentTime = millis();
                showRadio();
            }
        }
        else if (!g_settingsActive)
        {
            if (g_radioError)
            {
                retryRadio();
                showStatus();
            }
            else if (g_currentMode == FM)
            {
#if USE_RDS
                doRDS();
#else
                g_modePick = g_currentMode;
                switchCommand(CMD_MODE);
#endif
            }
            else if (g_currentCmd == CMD_MODE)
                switchCommand();
            else
            {
                g_modePick = g_currentMode;
                switchCommand(CMD_MODE);
            }
        }
    }
    else if (BUTTONEVENT_FIRSTLONGPRESS == modeEvent)
    {
        if (g_currentCmd == CMD_RADIO)
            switchCommand();
        else
        {
            switchCommand();
            radioEnter();
        }
    }

saveAttempt:
    servicePendingTune();
    displaySleepIfDue();
    //Save EEPROM if anough time passed and frequency changed
    if (g_currentFrequency != g_previousFrequency)
    {
        if ((millis() - g_storeTime) > STORE_TIME)
        {
            saveAllReceiverInformation();
            g_storeTime = millis();
            g_previousFrequency = g_currentFrequency;
        }
    }
}

//Overriding original main to save some space
int main(void)
{
    init();
    setup();
    while(1)
        loop();
    return 0;
}
