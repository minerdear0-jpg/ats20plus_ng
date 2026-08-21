#pragma once

#include <stdint.h>

bool isSSB();
int getSteps();
void loadSSBPatch();
void applyBandConfiguration(bool extraSSBReset = false);
void updateBFO();
bool clampSSBBand();
void doFrequencyTune();
void doFrequencyTuneSSB();
void commitRadioFrequency();
