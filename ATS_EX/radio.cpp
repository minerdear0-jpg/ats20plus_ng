#include "globals.h"
#include "patch_ssb_compressed.h"
#include "radio.h"

void showStatus(bool cleanFreq);
void showFrequency(bool cleanDisplay);
void showModulation();
void resetEepromDelay();
void updateSSBCutoffFilter();
void agcSetFunc();
void oledClearLine(uint8_t y);
#if USE_RDS
void setRDSConfig(uint8_t bias);
#endif

int getSteps()
{
    if (isSSB())
    {
        if (g_stepIndexSSB >= g_amTotalSteps)
            return g_tabStep[g_stepIndexSSB];

        return g_tabStep[g_stepIndexSSB] * 1000;
    }

    if (g_stepIndexAM >= g_amTotalSteps)
        g_stepIndexAM = 0;

    return g_tabStep[g_stepIndexAM];
}

void loadSSBPatch()
{
    // This works, but i am not sure it's safe
    //g_si4735.setI2CFastModeCustom(700000);
    g_si4735.setI2CFastModeCustom(500000);
    g_si4735.queryLibraryId(); //Do we really need this? Research it.
    g_si4735.patchPowerUp();
    delay(50);
    g_si4735.downloadCompressedPatch(ssb_patch_content, sizeof(ssb_patch_content), cmd_0x15, sizeof(cmd_0x15));
    g_si4735.setSSBConfig(g_bandwidthSSB[g_bwIndexSSB].idx, 1, 0, 1, 0, 1);
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);
    g_ssbLoaded = true;
}

void applyAMNoiseBlanker()
{
    if (g_currentMode == FM)
        return;

    if (g_Settings[SettingsIndex::ANB].param)
    {
        g_si4735.setProperty(AM_NB_DETECT_THRESHOLD, 12);
        g_si4735.setProperty(AM_NB_INTERVAL, 55);
        g_si4735.setProperty(AM_NB_RATE, 64);
        g_si4735.setProperty(AM_NB_IIR_FILTER, 300);
        g_si4735.setProperty(AM_NB_DELAY, 172);
    }
    else
        g_si4735.setProperty(AM_NB_DETECT_THRESHOLD, 0);
}

void handleSquelch(uint8_t rssi)
{
    if (g_muteVolume)
        return;

    bool cut = g_Settings[SettingsIndex::SQL].param
        && g_currentMode == AM
        && rssi < (uint8_t)g_Settings[SettingsIndex::SQL].param;

    if (cut != g_squelchCutoff)
    {
        g_si4735.setAudioMute(cut);
        g_squelchCutoff = cut;
    }
}

void applySquelchNow()
{
    if (g_muteVolume)
        return;

    if (g_currentMode != AM || g_Settings[SettingsIndex::SQL].param == 0)
    {
        if (g_squelchCutoff)
        {
            g_si4735.setAudioMute(false);
            g_squelchCutoff = false;
        }
        return;
    }

    g_si4735.getCurrentReceivedSignalQuality();
    handleSquelch(g_si4735.getCurrentRSSI());
}

void applyBandConfiguration(bool extraSSBReset)
{
    if (g_squelchCutoff)
    {
        g_si4735.setAudioMute(false);
        g_squelchCutoff = false;
    }

    g_si4735.setTuneFrequencyAntennaCapacitor(uint16_t(g_bandIndex == SW_BAND_TYPE));
    if (g_bandIndex == FM_BAND_TYPE)
    {
        g_currentMode = FM;
        g_si4735.setFM(g_bandList[g_bandIndex].minimumFreq,
            g_bandList[g_bandIndex].maximumFreq,
            g_bandList[g_bandIndex].currentFreq,
            g_tabStepFM[g_bandList[g_bandIndex].currentStepIdx]);
        g_si4735.setSeekFmLimits(g_bandList[g_bandIndex].minimumFreq, g_bandList[g_bandIndex].maximumFreq);
        g_si4735.setSeekFmSpacing(1);
        g_ssbLoaded = false;
#if USE_RDS
        setRDSConfig(g_Settings[SettingsIndex::RDSError].param);
#endif
        g_si4735.setFifoCount(1);
        g_bwIndexFM = g_bandList[g_bandIndex].bandwidthIdx;
        g_si4735.setFmBandwidth(g_bwIndexFM);
        g_si4735.setFMDeEmphasis(g_Settings[SettingsIndex::DeEmp].param == 0 ? 1 : 2);
#if USE_RDS
        g_displayRDS = true;
#endif
    }
    else
    {
        uint16_t minFreq = g_bandList[g_bandIndex].minimumFreq;
        uint16_t maxFreq = g_bandList[g_bandIndex].maximumFreq;
        if (g_bandIndex == SW_BAND_TYPE)
        {
            minFreq = SW_LIMIT_LOW;
            maxFreq = SW_LIMIT_HIGH;
        }

        if (g_ssbLoaded)
        {
            g_currentBFO = 0;
            if (extraSSBReset)
                loadSSBPatch();

            //Call this before to call crazy volume after AM when SVC is off
            g_si4735.setSSBAutomaticVolumeControl(g_Settings[SettingsIndex::SVC].param);
            g_si4735.setSSB(minFreq,
                maxFreq,
                g_bandList[g_bandIndex].currentFreq,
                g_bandList[g_bandIndex].currentStepIdx >= g_amTotalSteps ? 0 : g_tabStep[g_bandList[g_bandIndex].currentStepIdx],
                g_currentMode == CW ? g_Settings[SettingsIndex::CWSwitch].param + 1 : g_currentMode);
            updateSSBCutoffFilter();
            g_si4735.setSSBAutomaticVolumeControl(g_Settings[SettingsIndex::SVC].param);
            g_si4735.setSSBDspAfc(g_Settings[SettingsIndex::Sync].param == 1 ? 0 : 1);
            g_si4735.setSSBAvcDivider(g_Settings[SettingsIndex::Sync].param == 0 ? 0 : 3); //Set Sync mode
            g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
            g_si4735.setSSBAudioBandwidth(g_currentMode == CW ? g_bandwidthSSB[0].idx : g_bandwidthSSB[g_bwIndexSSB].idx);
            updateBFO();
            g_si4735.setSSBSoftMute(g_Settings[SettingsIndex::SSM].param);
        }
        else
        {
            g_currentMode = AM;
            g_si4735.setAM(minFreq,
                maxFreq,
                g_bandList[g_bandIndex].currentFreq,
                g_bandList[g_bandIndex].currentStepIdx >= g_amTotalSteps ? 0 : g_tabStep[g_bandList[g_bandIndex].currentStepIdx]);
            g_si4735.setAmSoftMuteMaxAttenuation(g_Settings[SettingsIndex::SoftMute].param);
            g_bwIndexAM = g_bandList[g_bandIndex].bandwidthIdx;
            g_si4735.setBandwidth(g_bandwidthAM[g_bwIndexAM].idx, 1);
        }

        agcSetFunc();
        g_si4735.setAvcAmMaxGain(g_Settings[SettingsIndex::AutoVolControl].param);
        g_si4735.setSeekAmLimits(minFreq, maxFreq);
        g_si4735.setSeekAmSpacing((g_bandList[g_bandIndex].currentStepIdx >= g_amTotalSteps) ? 1 : g_tabStep[g_bandList[g_bandIndex].currentStepIdx]);
        applyAMNoiseBlanker();
#if USE_RDS
        g_displayRDS = false;
#endif
    }

    g_currentFrequency = g_bandList[g_bandIndex].currentFreq;
    if (g_currentMode == FM)
        g_FMStepIndex = g_bandList[g_bandIndex].currentStepIdx;
    else
    {
        g_stepIndexAM = g_bandList[g_bandIndex].currentStepIdx;
        if (g_stepIndexAM >= g_amTotalSteps)
            g_stepIndexAM = 0;
        if ((g_bandIndex == LW_BAND_TYPE || g_bandIndex == MW_BAND_TYPE)
            && g_stepIndexAM > g_amTotalStepsSSB)
            g_stepIndexAM = g_amTotalStepsSSB;
    }

    if (!g_settingsActive)
        showStatus(true);
    resetEepromDelay();
}

void updateBFO()
{
    int16_t bfo = g_currentBFO + (g_Settings[SettingsIndex::BFO].param * 10);
    if (g_currentMode == CW)
    {
        int16_t pitch = (int16_t)g_Settings[SettingsIndex::CWPitch].param * 100;
        // CWSwitch 0 = LSB, 1 = USB. Pitch is audio offset, not the dial.
        if (g_Settings[SettingsIndex::CWSwitch].param)
            bfo -= pitch;
        else
            bfo += pitch;
    }
    //Actually to move frequency forward you need to move BFO backwards, so just * -1
    g_si4735.setSSBBfo(bfo * -1);
}

bool clampSSBBand()
{
    uint16_t freq = g_currentFrequency + (g_currentBFO / 1000);
    auto bfoReset = [&]()
    {
        g_currentBFO = 0;
        updateBFO();
        showFrequency(true);
        showModulation();
    };

    bool upd = false;
    if (freq > g_bandList[g_bandIndex].maximumFreq)
    {
        g_currentFrequency = g_bandList[g_bandIndex].minimumFreq;
        upd = true;
    }
    else if (freq < g_bandList[g_bandIndex].minimumFreq)
    {
        g_currentFrequency = g_bandList[g_bandIndex].maximumFreq;
        upd = true;
    }

    if (upd)
    {
        g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
        g_si4735.setFrequency(g_currentFrequency);
        g_ssbNeedHwFreq = false;
        g_processFreqChange = false;
        bfoReset();
        return true;
    }

    return false;
}

void doFrequencyTune()
{
    g_seekDirection = g_encoderCount > 0 ? 1 : 0;

    //Update frequency
    g_previousFrequency = g_currentFrequency; //Force EEPROM update
    if (g_currentMode == FM)
    {
        g_currentFrequency += g_tabStepFM[g_FMStepIndex] * g_encoderCount; //g_si4735.getFrequency() is too slow
#if USE_RDS
        if (g_displayRDS)
            oledClearLine(UI_PAGE_SECONDARY);
#endif
    }
    else
        g_currentFrequency += g_tabStep[g_stepIndexAM] * g_encoderCount;
    uint16_t bMin = g_bandList[g_bandIndex].minimumFreq, bMax = g_bandList[g_bandIndex].maximumFreq;

    //Special logic for fast and responsive frequency surfing
    if (g_currentFrequency > bMax)
        g_currentFrequency = bMin;
    else if (g_currentFrequency < bMin)
        g_currentFrequency = bMax;

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency;
    markFreqPending();

    uiMark(UI_FREQ);
}

void doFrequencyTuneSSB()
{
    const int BFOMax = 16000;
    int step = getSteps() * g_encoderCount;
    int newBFO = g_currentBFO + step;
    int redundant = 0;

    if (newBFO > BFOMax)
    {
        redundant = (newBFO / BFOMax) * BFOMax;
        g_currentFrequency += redundant / 1000;
        newBFO -= redundant;
    }
    else if (newBFO < -BFOMax)
    {
        redundant = ((abs(newBFO) / BFOMax) * BFOMax);
        g_currentFrequency -= redundant / 1000;
        newBFO += redundant;
    }

    g_currentBFO = newBFO;
    if (redundant != 0)
        g_ssbNeedHwFreq = true;

    g_bandList[g_bandIndex].currentFreq = g_currentFrequency + (g_currentBFO / 1000);
    markFreqPending();
    g_previousFrequency = 0; //Force EEPROM update
    if (!clampSSBBand())
        uiMark(UI_FREQ);
}

void markFreqPending()
{
    if (!g_processFreqChange)
        g_rfPendingSince = millis();
    g_processFreqChange = true;
    g_lastFreqChange = millis();
}

void servicePendingTune()
{
    if (g_radioError || !g_processFreqChange)
        return;
    uint32_t t0 = isSSB() ? g_rfPendingSince : g_lastFreqChange;
    uint16_t wait = isSSB() ? (uint16_t)RF_COMMIT_MS : (uint16_t)FREQ_COMMIT_MS;
    if ((millis() - t0) >= wait)
        commitRadioFrequency();
}

void commitRadioFrequency()
{
    if (g_radioError)
    {
        g_processFreqChange = false;
        return;
    }
    g_si4735.setI2CFastModeCustom(I2C_FAST_HZ);
    if (isSSB())
    {
        if (g_ssbNeedHwFreq)
        {
            g_si4735.setFrequency(g_currentFrequency);
            agcSetFunc();
            g_currentFrequency = g_si4735.getFrequency();
            g_bandList[g_bandIndex].currentFreq = g_currentFrequency + (g_currentBFO / 1000);
            g_ssbNeedHwFreq = false;
        }
        updateBFO();
    }
    else
        g_si4735.setFrequency(g_currentFrequency);
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);
    g_processFreqChange = false;
}

extern "C" void si4735OnBusFail(void)
{
    g_radioError = 1;
}

void retryRadio()
{
    g_radioError = 0;
    g_si4735.reset();
    delay(50);
    applyBandConfiguration();
    g_si4735.setVolume(g_volume);
    g_si4735.setI2CFastModeCustom(I2C_RUN_HZ);
}
