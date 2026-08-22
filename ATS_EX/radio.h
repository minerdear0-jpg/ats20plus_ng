#pragma once

#include <stdint.h>
#include "globals.h"

inline bool isSSB()
{
    return g_currentMode > AM && g_currentMode < FM;
}

inline volatile int8_t& activeStepIndex()
{
    return isSSB() ? g_stepIndexSSB : g_stepIndexAM;
}

int getSteps();
void applyAMNoiseBlanker();
void handleSquelch(uint8_t rssi);
void applySquelchNow();
void loadSSBPatch();
void applyBandConfiguration(bool extraSSBReset = false);
void updateBFO();
bool clampSSBBand();
void doFrequencyTune();
void doFrequencyTuneSSB();
void commitRadioFrequency();
