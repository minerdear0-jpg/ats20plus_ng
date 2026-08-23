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
#include <Tiny4kOLED.h>
#include "font8x16pob_ui.h"
#include "font14x24sevenSeg.h"
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

    oled.begin(128, 64, sizeof(tiny4koled_init_128x64br), tiny4koled_init_128x64br);
    oled.clear();
    oled.on();
    oled.setFont(DEFAULT_FONT);

    //Don't use digitalRead()
    //Registers save us more space
    if (!(PINC & (1 << (ENCODER_BUTTON - 14))))
    {
        saveAllReceiverInformation();
        oled.print("  EEPROM RESET");
        oled.setCursor(0, 2);
        oled.print("----------------");
        delay(960);
    }
    else
    {
        oledPrint(" ATS-20 RECEIVER", 0, 0, DEFAULT_FONT, true);
        oledPrint("ATS_EX V" FW_VERSION_STR, 16, 2);
        oledPrint("(C) OOMAN   2026", 0, 4);
        oledPrint("(C) CURSOR  2026", 0, 6);
        delay(2000);
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

    //Clock speed configuration
    noInterrupts(); //cli()
    CLKPR = 0x80;   //Allow edit CLKPR register
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();   //sei()

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
    g_currentBFO = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
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
        g_bandList[i].currentFreq = (EEPROM.read(addr++) << 8) | EEPROM.read(addr++);
        g_bandList[i].currentStepIdx = EEPROM.read(addr++);
        g_bandList[i].bandwidthIdx = EEPROM.read(addr++);
    }

    for (uint8_t i = 0; i < SettingsIndex::SETTINGS_MAX; i++)
        g_Settings[i].param = EEPROM.read(addr++);

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
//I2C: reprint 7-seg from the first glyph that changed ('/' blanks a slot).
static uint8_t g_mhzX;
static uint8_t g_freqRightX = 128;

void showFrequency(bool cleanDisplay = false)
{
    if (g_settingsActive)
        return;

    g_si4735.setI2CFastModeCustom(I2C_FAST_HZ);

    char unit[4];
    char line[10];
    static char prevLine[10];
    static uint8_t prevOff = 255;
    static uint8_t prevAuxX = 255;
    static uint8_t prevAuxMode = 255;
    static bool prevShowMhz = false;
    static bool prevStereo = false;
    uint16_t khzBFO = 0, tailBFO = 0;
    uint8_t i = 0;
    uint8_t len;

    unit[0] = 'k';
    unit[1] = 'H';
    unit[2] = 'z';
    unit[3] = 0x0;
    line[0] = 0;

    if (g_bandIndex == FM_BAND_TYPE)
    {
        convertToChar(line, g_currentFrequency, 5, 3, '.', '/');
        unit[0] = 'M';
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
                if (g_bandIndex == SW_BAND_TYPE && swMhz)
                    unit[0] = 'M';
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

    uint8_t nGlyphs = strlen8(line);
    bool showMhz = g_Settings[SettingsIndex::UnitsSwitch].param == 1 && unit[0] == 'M'
        && (!isSSB() || isSSB() && len < 5);
    uint8_t unitW = 0;
    if (g_currentMode == FM)
        unitW = (uint8_t)(2 + STEREO_CHIP_W);
    else if (showMhz)
        unitW = (uint8_t)(2 + MHZ_LABEL_W);
    uint8_t off = (uint8_t)((128 - (uint16_t)nGlyphs * 14 - unitW) / 2);

    if (cleanDisplay || prevOff != off)
    {
        oledPrint("/////////", 0, UI_PAGE_FREQ, FONT14X24SEVENSEG);
        prevLine[0] = 0;
        prevOff = off;
    }

    while (line[i] && line[i] == prevLine[i])
        i++;
    if (line[i] || prevLine[i])
    {
        uint8_t oldLen = strlen8(prevLine);
        oledSetFont(FONT14X24SEVENSEG);
        oled.setCursor(off + i * 14, UI_PAGE_FREQ);
        oled.print(&line[i]);
        i = strlen8(line);
        while (i < oldLen)
        {
            oled.write((uint8_t)'/');
            i++;
        }
        i = 0;
        do
        {
            prevLine[i] = line[i];
        } while (line[i++]);
    }

    g_freqRightX = (uint8_t)(off + nGlyphs * 14);
    g_mhzX = (uint8_t)(g_freqRightX + 2);
    bool layoutDirty = cleanDisplay || prevAuxX != g_mhzX || prevAuxMode != g_currentMode || prevShowMhz != showMhz;
    if (showMhz && layoutDirty)
        oledPrintMhz(g_mhzX);
    if (g_currentMode == FM && (layoutDirty || prevStereo != g_fmStereo))
        oledPrintStereoChip(g_mhzX, g_fmStereo);
    prevAuxX = g_mhzX;
    prevAuxMode = g_currentMode;
    prevShowMhz = showMhz;
    prevStereo = g_fmStereo;
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);
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
    oled.setCursor(0, UI_PAGE_SIGNAL);
    oled.fillLength(0, 128);
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

//Converts settings value to UI value
void SettingParamToUI(char* buf, uint8_t idx)
{
    int8_t param = g_Settings[idx].param;
    switch (g_Settings[idx].type)
    {
    case SettingType::ZeroAuto:
        if (param == 0)
        {
            buf[0] = 'A';
            buf[1] = 'U';
            buf[2] = 'T';
            buf[3] = 0x0;
        }
        else
            convertToChar(buf, param, 3);

        break;

    case SettingType::Num:
        if (idx == SettingsIndex::DisplayOff)
        {
            if (param <= 0)
            {
                buf[0] = 'O';
                buf[1] = 'F';
                buf[2] = 'F';
            }
            else if (param == 1)
            {
                buf[0] = '1';
                buf[1] = '5';
                buf[2] = 'S';
            }
            else if (param == 2)
            {
                buf[0] = '3';
                buf[1] = '0';
                buf[2] = 'S';
            }
            else if (param == 3)
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
            break;
        }
        if (idx == SettingsIndex::CWPitch)
        {
            buf[0] = '0' + param;
            buf[1] = '0';
            buf[2] = '0';
            buf[3] = 0;
            break;
        }
        convertToChar(buf, abs(param), 3);
        if (param < 0)
            buf[0] = '-';
        break;

    case SettingType::SwitchAuto:
        if (param == 0)
        {
            buf[0] = 'A';
            buf[1] = 'U';
            buf[2] = 'T';       }
        else if (param == 1)
        {
            buf[0] = 'O';
            buf[1] = 'N';
            buf[2] = ' ';
        }
        else
        {
            buf[0] = 'O';
            buf[1] = 'F';
            buf[2] = 'F';
        }
        buf[3] = 0x0;
        break;

    case SettingType::Switch:
        if (idx == SettingsIndex::DeEmp)
        {
            if (param == 0)
            {
                buf[0] = '5';
                buf[1] = '0';
                buf[2] = 'U';
            }
            else
            {
                buf[0] = '7';
                buf[1] = '5';
                buf[2] = 'U';
            }
        }
        else if (idx == SettingsIndex::SWUnits)
        {
            if (param == 0)
                buf[0] = 'K';
            else
                buf[0] = 'M';
            buf[1] = 'H';
            buf[2] = 'Z';
        }
        else if (idx == SettingsIndex::SSM)
        {
            if (param == 0)
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
        }
        else if (idx == SettingsIndex::CWSwitch)
        {
            if (param == 0)
                buf[0] = 'L';
            else
                buf[0] = 'U';

            buf[1] = 'S';
            buf[2] = 'B';
        }
        else if (idx == SettingsIndex::CPUSpeed)
        {
            if (param == 0)
            {
                buf[0] = '1';
                buf[1] = '0';
                buf[2] = '0';
            }
            else
            {
                buf[0] = '5';
                buf[1] = '0';
                buf[2] = '%';
            }
        }
        else
        {
            if (param == 0)
            {
                buf[0] = 'O';
                buf[1] = 'F';
                buf[2] = 'F';
            }
            else
            {
                buf[0] = 'O';
                buf[1] = 'N';
                buf[2] = ' ';
            }
        }
        buf[3] = 0x0;
        break;
    }
}

enum { MENU_CATS = 4, MENU_ROWS = 3 };

static const uint8_t kMenuRadio[] = {
    SettingsIndex::ATT, SettingsIndex::Sync, SettingsIndex::AutoVolControl,
    SettingsIndex::CutoffFilter, SettingsIndex::ANB, SettingsIndex::SQL, SettingsIndex::BFO
};
static const uint8_t kMenuDisp[] = {
    SettingsIndex::Brightness, SettingsIndex::DisplayOff, SettingsIndex::SWUnits, SettingsIndex::UnitsSwitch
};
static const uint8_t kMenuAudio[] = {
    SettingsIndex::SoftMute, SettingsIndex::DeEmp, SettingsIndex::SVC, SettingsIndex::SSM
};
static const uint8_t kMenuSys[] = {
    SettingsIndex::CPUSpeed, SettingsIndex::ScanSwitch, SettingsIndex::CWSwitch, SettingsIndex::CWPitch
};
static const uint8_t* const kMenuItems[] = { kMenuRadio, kMenuDisp, kMenuAudio, kMenuSys };
static const uint8_t kMenuCount[] = { 7, 4, 4, 4 };
static const char* const kMenuNames[] = { "RADIO", "DISP", "AUDIO", "SYS" };

static uint8_t menuItemAt(uint8_t sel)
{
    return kMenuItems[g_menuCat][sel];
}

static uint8_t menuScrollTop()
{
    uint8_t n = kMenuCount[g_menuCat];
    uint8_t sel = (uint8_t)g_SettingSelected;
    if (n <= MENU_ROWS || sel < MENU_ROWS)
        return 0;
    uint8_t s = sel - (MENU_ROWS - 1);
    if (s + MENU_ROWS > n)
        s = n - MENU_ROWS;
    return s;
}

void DrawSetting(uint8_t idx, uint8_t row, bool full)
{
    if (!g_settingsActive)
        return;

    char buf[5];
    uint8_t y = 2 + row * 2;
    if (full)
        oledPrint(g_Settings[idx].name, 0, y, DEFAULT_FONT, idx == menuItemAt((uint8_t)g_SettingSelected) && !g_SettingEditing);
    SettingParamToUI(buf, idx);
    oledPrint(buf, 72, y, DEFAULT_FONT, idx == menuItemAt((uint8_t)g_SettingSelected) && g_SettingEditing);
}

void showMenu()
{
    oled.clear();
    if (g_menuLevel == 1)
    {
        for (uint8_t i = 0; i < MENU_CATS; i++)
            oledPrint(kMenuNames[i], 0, i * 2, DEFAULT_FONT, i == (uint8_t)g_SettingSelected);
        return;
    }

    oledPrint(kMenuNames[g_menuCat], 0, 0, DEFAULT_FONT, true);
    uint8_t n = kMenuCount[g_menuCat];
    uint8_t top = menuScrollTop();
    for (uint8_t r = 0; r < MENU_ROWS && (top + r) < n; r++)
        DrawSetting(menuItemAt(top + r), r, true);
}

void menuBack()
{
    if (g_SettingEditing)
    {
        g_SettingEditing = false;
        showMenu();
        return;
    }
    if (g_menuLevel == 2)
    {
        g_menuLevel = 1;
        g_SettingSelected = g_menuCat;
        showMenu();
        return;
    }
    g_settingsActive = false;
    saveAllReceiverInformation();
    oled.clear();
    showStatus(true);
}

void switchSettings()
{
    oled.clear();
    if (g_settingsActive)
    {
        g_menuLevel = 1;
        g_menuCat = 0;
        g_SettingSelected = 0;
        g_SettingEditing = false;
        showMenu();
    }
    else
    {
        saveAllReceiverInformation();
        showStatus(true);
    }
}

//Draw curremt modulation
void paintTransient(const char* title, const char* value)
{
    oled.setCursor(0, 0);
    oled.fillLength(0, 128);
    oled.setCursor(0, 1);
    oled.fillLength(0, 128);

    oledPrint(title, 0, 0, DEFAULT_FONT);
    uint8_t tw = 0;
    while (title[tw])
        tw++;
    uint8_t badgeW = oledBadgeWidth(value, BADGE_PAD);
    uint8_t badgeX = (uint8_t)(128 - badgeW);
    oledSetFont(DEFAULT_FONT);
    oled.setCursor(tw * 8, 0);
    uint8_t x = tw * 8;
    while (x + 8 <= badgeX)
    {
        oled.print(".");
        x += 8;
    }
    oledPrintModeBadge(value, badgeX, BADGE_PAD);
    g_uiLayer = UI_LAYER_TRANSIENT;
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
    oledPrintModeBadge(g_bandModeDesc[g_currentMode], 0, BADGE_PAD);
    uint8_t sx = (uint8_t)(oledBadgeWidth(g_bandModeDesc[g_currentMode], BADGE_PAD) + 2);
    if (isSSB() && g_Settings[SettingsIndex::Sync].param == 1)
        oledPrint("S", sx, 0, DEFAULT_FONT, true);
    else
        oledPrint(" ", sx, 0, DEFAULT_FONT);
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
        oled.print(printChar);

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

void showSMeter()
{
    if (g_settingsActive)
        return;

    static uint32_t sMeterUpdated = 0;
    static uint8_t smPeak = 0;
    static uint8_t smDrawnPeak = 255;
    static uint32_t smPeakMs = 0;
    uint32_t now = millis();
    if (g_sMeterDrawnVal != 255 && now - sMeterUpdated < 100)
        return;
    sMeterUpdated = now;

    g_si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = g_si4735.getCurrentRSSI();
    handleSquelch(rssi);
    uint8_t sUnit;
    uint8_t plusDb = 0;
    if (rssi >= S9_DBUV)
    {
        sUnit = 9;
        plusDb = rssi - S9_DBUV;
        if (plusDb > SMETER_MAX_OVER_S9)
            plusDb = SMETER_MAX_OVER_S9;
    }
    else
    {
        sUnit = (uint8_t)((rssi + 20) / 6);
        if (sUnit > 9)
            sUnit = 9;
    }

    // 0..9 = S-units, 10..15 = +10..+60 dB over S9. Label follows current, not peak.
    uint8_t val = sUnit;
    if (plusDb)
        val = 9 + plusDb / 10;
    if (val > SMETER_SEGMENTS - 1)
        val = SMETER_SEGMENTS - 1;

    if (g_sMeterDrawnVal == 255)
    {
        smPeak = val;
        smDrawnPeak = 255;
        smPeakMs = now;
    }
    else if (val > smPeak)
    {
        smPeak = val;
        smPeakMs = now;
    }
    else if (val < smPeak && now - smPeakMs >= SMETER_RELEASE_MS)
    {
        smPeak--;
        smPeakMs = now;
    }

    bool stereoDirty = false;
    if (g_currentMode == FM)
    {
        bool st = g_si4735.getCurrentPilot();
        if (st != g_fmStereo)
        {
            g_fmStereo = st;
            stereoDirty = true;
        }
    }
    if (val == g_sMeterDrawnVal && smPeak == smDrawnPeak && !stereoDirty)
        return;

    g_si4735.setI2CFastModeCustom(I2C_FAST_HZ);
    if (val != g_sMeterDrawnVal)
    {
        char lab[4];
        lab[0] = 'S';
        lab[1] = (char)('0' + sUnit);
        lab[2] = plusDb ? '+' : ' ';
        lab[3] = 0;
        oledPrintSMeterLab(lab);
    }
    if (val != g_sMeterDrawnVal || smPeak != smDrawnPeak)
    {
        oled.setCursor(SMETER_BAR_X, UI_PAGE_SECONDARY);
        oled.fillLength(0, (uint8_t)(128 - SMETER_BAR_X));
        oled.setCursor(SMETER_BAR_X, UI_PAGE_SECONDARY + 1);
        oled.startData();
        for (uint8_t i = 0; i < SMETER_SEGMENTS; i++)
        {
            uint8_t pat = 0;
            if (i < val)
                pat = 0xFE;
            else if (i < smPeak)
                pat = 0x54;
            for (uint8_t w = 0; w < SMETER_SEG_W; w++)
                oled.sendData(pat);
            if (i != SMETER_SEGMENTS - 1)
                oled.sendData(0);
        }
        oled.endData();
        oled.setCursor((uint8_t)(SMETER_BAR_X + SMETER_SEGMENTS * SMETER_SEG_W + (SMETER_SEGMENTS - 1)), UI_PAGE_SECONDARY + 1);
        oled.fillLength(0, (uint8_t)(128 - SMETER_BAR_X - (SMETER_SEGMENTS * SMETER_SEG_W + (SMETER_SEGMENTS - 1))));
        g_sMeterDrawnVal = val;
        smDrawnPeak = smPeak;
    }
    if (stereoDirty)
        oledPrintStereoChip(g_mhzX, g_fmStereo);
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);
}

//Draw bandwidth (Ignored for CW mode)
void showBandwidth()
{
    char* bw;
    if (isSSB())
    {
        bw = (char*)g_bandwidthSSB[g_bwIndexSSB].desc;
        if (g_currentMode == CW)
            bw = "    ";
    }
    else if (g_currentMode == AM)
    {
        bw = (char*)g_bandwidthAM[g_bwIndexAM].desc;
    }
    else
    {
        bw = (char*)g_bandwidthFM[g_bwIndexFM];
    }

    if (g_currentCmd == CMD_BW)
    {
        paintTransient("BW", bw);
        return;
    }
    if (g_currentMode == CW)
        bw = (char*)g_bandwidthSSB[0].desc;
    uint8_t w = oledBadgeWidth(bw, BADGE_PAD);
    oledPrintModeBadge(bw, (uint8_t)((128 - w) / 2), BADGE_PAD);
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
        agcSetFunc(); //Re-apply to remove noize
        showFrequency();
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
        volatile int8_t& st = activeStepIndex();
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

//Volume control
void doVolume(int8_t v)
{
    if (g_muteVolume)
    {
        g_si4735.setVolume(g_muteVolume);
        g_muteVolume = 0;
        applySquelchNow();
    }
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
    showVolume();
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

//Settings: CPU Frequency divider
void doCPUSpeed(int8_t v = 0)
{
    doSwitchLogic(g_Settings[SettingsIndex::CPUSpeed].param, 0, 1, v);

    noInterrupts();
    CLKPR = 0x80;
    CLKPR = g_Settings[SettingsIndex::CPUSpeed].param;
    interrupts();
    // CPU 50% keeps F_CPU=16e6 at compile; millis/TWI/deadlines scale. Needs hardware check (P0.5).
}

void doDisplayOff(int8_t v)
{
    doSwitchLogic(g_Settings[SettingsIndex::DisplayOff].param, 0, DISPLAY_OFF_MAX, v);
    if (g_Settings[SettingsIndex::DisplayOff].param == 0 && !g_displayOn)
    {
        g_displayOn = true;
        oled.on();
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
    if (cmd == CMD_NONE)
    {
        g_currentCmd = CMD_NONE;
        g_lastAdjustmentTime = 0;
        g_uiFocus = FOCUS_FREQ;
        restoreIdleHeader();
        showStep();
        showVolume();
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

void displayWake()
{
    if (!g_displayOn)
    {
        g_displayOn = true;
        oled.on();
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
    oled.off();
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

        if (g_lastAdjustmentTime != 0)
            g_lastAdjustmentTime = millis();

        if (g_settingsActive)
        {
            if (g_menuLevel == 1)
            {
                int8_t next = g_SettingSelected + g_encoderCount;
                while (next < 0)
                    next += MENU_CATS;
                while (next >= MENU_CATS)
                    next -= MENU_CATS;
                g_SettingSelected = next;
                showMenu();
            }
            else if (!g_SettingEditing)
            {
                int8_t n = (int8_t)kMenuCount[g_menuCat];
                int8_t next = g_SettingSelected + g_encoderCount;
                while (next < 0)
                    next += n;
                while (next >= n)
                    next -= n;
                g_SettingSelected = next;
                showMenu();
            }
            else
            {
                uint8_t idx = menuItemAt((uint8_t)g_SettingSelected);
                (*g_Settings[idx].manipulateCallback)(g_encoderCount);
                DrawSetting(idx, (uint8_t)g_SettingSelected - menuScrollTop(), false);
            }
        }
        else if (g_radioError)
            ;
        else if (g_currentCmd == CMD_VOLUME)
        {
            g_uiLayer = UI_LAYER_TRANSIENT;
            doVolume(g_encoderCount);
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
        poke |= btn_AGC.checkEvent(simpleEvent);
        poke |= btn_Step.checkEvent(simpleEvent);
        poke |= btn_Mode.checkEvent(simpleEvent);
        if (poke)
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

    if (g_lastAdjustmentTime != 0 && millis() - g_lastAdjustmentTime > ADJUSTMENT_ACTIVE_TIMEOUT
        && g_currentCmd != CMD_STEP && g_currentCmd != CMD_MODE)
        switchCommand();

    //Command-checkers
    if (BUTTONEVENT_SHORTPRESS == btn_Bandwidth.checkEvent(simpleEvent))
    {
        if (!g_settingsActive && g_currentMode != CW)
            switchCommand(CMD_BW, showBandwidth);
    }
    if (BUTTONEVENT_SHORTPRESS == btn_BandUp.checkEvent(bandEvent))
    {
        if (!g_settingsActive)
        {
            resetLowerLine();
            switchCommand(CMD_BAND, showModulation);
        }
        else
        {
            menuBack();
        }
    }
    if (BUTTONEVENT_SHORTPRESS == btn_BandDn.checkEvent(bandEvent))
    {
        if (!g_settingsActive)
            switchCommand();
        g_settingsActive = !g_settingsActive;
        switchSettings();
    }
    if (BUTTONEVENT_SHORTPRESS == btn_VolumeUp.checkEvent(volumeEvent))
    {
        if (!g_settingsActive && g_muteVolume == 0)
            switchCommand(CMD_VOLUME, showVolume);
    }
    if (BUTTONEVENT_SHORTPRESS == btn_VolumeDn.checkEvent(volumeEvent))
    {
        if (g_currentCmd != CMD_VOLUME)
        {
            uint8_t vol = g_si4735.getCurrentVolume();
            if (vol > 0 && g_muteVolume == 0)
            {
                g_muteVolume = vol;
                g_si4735.setVolume(0);
            }
            else if (g_muteVolume > 0)
            {
                g_si4735.setVolume(g_muteVolume);
                g_muteVolume = 0;
                applySquelchNow();
            }
            showVolume();
        }
    }
    uint8_t encEvent = btn_Encoder.checkEvent(simpleEvent);
    if (BUTTONEVENT_SHORTPRESS == encEvent)
    {
        if (g_settingsActive)
        {
            if (g_menuLevel == 1)
            {
                g_menuCat = (uint8_t)g_SettingSelected;
                g_menuLevel = 2;
                g_SettingSelected = 0;
                g_SettingEditing = false;
                showMenu();
            }
            else
            {
                g_SettingEditing = !g_SettingEditing;
                showMenu();
            }
        }
        else if (g_radioError)
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
        else
            cycleEncoderFocus();
    }
    else if (BUTTONEVENT_LONGPRESSDONE == encEvent)
    {
        if (g_settingsActive)
            menuBack();
        else if (g_currentMode == FM || g_currentMode == AM)
            doSeek();
        else
        {
            switchCommand();
            g_settingsActive = true;
            switchSettings();
        }
    }

    //This is a hack, it allows SHORTPRESS and LONGPRESS events
    //Be processed without complicated overhead
    //It requires to save checkEvent result into a variable
    //That has exact same name as event processing function for this button
    uint8_t agcEvent = btn_AGC.checkEvent(agcEvent);
    if (BUTTONEVENT_SHORTPRESS == agcEvent)
    {
        if (!g_settingsActive || (g_settingsActive && !g_displayOn))
        {
            g_displayOn = !g_displayOn;
            if (g_displayOn)
                oled.on();
            else
                oled.off();
            g_lastInputMs = millis();
        }
    }
    if (BUTTONEVENT_LONGPRESS == agcEvent)
    {
        if (!g_settingsActive)
        {
            if (isSSB())
                doSync(1);
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
    if (BUTTONEVENT_SHORTPRESS == btn_Mode.checkEvent(simpleEvent))
    {
        if (!g_settingsActive)
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
#endif
            }
            else if (g_currentCmd == CMD_MODE)
                switchCommand();
            else
            {
                g_modePick = g_currentMode;
                g_currentCmd = CMD_MODE;
                paintModeOrBand();
            }
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
