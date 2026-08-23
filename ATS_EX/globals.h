#pragma once

#include <Arduino.h>
#include <SI4735.h>
#include "SimpleButton.h"
#include "Rotary.h"
#include "defs.h"

extern long g_storeTime;

extern bool g_voltagePinConnnected;
extern bool g_ssbLoaded;
extern bool g_fmStereo;

enum UiCmd : uint8_t
{
    CMD_NONE = 0,
    CMD_VOLUME,
    CMD_STEP,
    CMD_BW,
    CMD_BAND,
    CMD_MODE
};
extern uint8_t g_currentCmd;
extern bool g_settingsActive;
extern uint8_t g_radioError;

enum UiLayer : uint8_t
{
    UI_LAYER_NORMAL = 0,
    UI_LAYER_FOCUS,
    UI_LAYER_TRANSIENT
};

enum UiDirty : uint8_t
{
    UI_FREQ = 1 << 0,
    UI_MOD  = 1 << 1,
    UI_STEP = 1 << 2,
    UI_BW   = 1 << 3,
    UI_VOL  = 1 << 4
};
extern uint8_t g_uiDirty;
extern bool g_uiFreqClean;
extern uint8_t g_uiLayer;
extern uint8_t g_uiFocus;

enum UiFocus : uint8_t
{
    FOCUS_FREQ = 0, // MAIN: rotate = frequency, not a press-cycle item
    FOCUS_STEP,
    FOCUS_VOL,
    FOCUS_BFO
};

inline void uiMark(uint8_t bits, bool freqClean = false)
{
    g_uiDirty |= bits;
    if (freqClean)
        g_uiFreqClean = true;
}
extern uint8_t g_sMeterDrawnVal;
extern bool g_displayOn;
extern bool g_displayRDS;
extern bool g_rdsSwitchPressed;
extern volatile bool g_seekStop;
extern uint32_t g_lastAdjustmentTime;
extern uint32_t g_lastInputMs;

extern uint8_t g_muteVolume;
extern bool g_squelchCutoff;
extern int g_currentBFO;

// Encoder buttons
extern SimpleButton  btn_Bandwidth;
extern SimpleButton  btn_BandUp;
extern SimpleButton  btn_BandDn;
extern SimpleButton  btn_VolumeUp;
extern SimpleButton  btn_VolumeDn;
extern SimpleButton  btn_Encoder;
extern SimpleButton  btn_AGC;
extern SimpleButton  btn_Step;
extern SimpleButton  btn_Mode;

extern volatile int8_t g_encoderCount;

//Frequency tracking
extern uint16_t g_currentFrequency;
extern uint16_t g_previousFrequency;

enum SettingType
{
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

struct SettingsItem
{
    char name[5];
    int8_t param;
    uint8_t type;
    void (*manipulateCallback)(int8_t);
};

void doAttenuation(int8_t v);
void doSoftMute(int8_t v);
void doBrightness(int8_t v);
void doSSBAVC(int8_t v = 0);
void doAvc(int8_t v);
void doSync(int8_t v = 0);
void doDeEmp(int8_t v = 0);
void doSWUnits(int8_t v = 0);
void doSSBSoftMuteMode(int8_t v = 0);
void doCutoffFilter(int8_t v);
void doCPUSpeed(int8_t v = 0);
#if USE_RDS
void doRDSErrorLevel(int8_t v);
#endif
void doDisplayOff(int8_t v);
void doBFOCalibration(int8_t v);
void doUnitsSwitch(int8_t v = 0);
void doScanSwitch(int8_t v = 0);
void doCWSwitch(int8_t v = 0);
void doCWPitch(int8_t v);
void doANB(int8_t v = 0);
void doSQL(int8_t v);

extern SettingsItem g_Settings[];

enum SettingsIndex
{
    ATT,
    SoftMute,
    SVC,
    Sync,
    DeEmp,
    AutoVolControl,
    Brightness,
    SWUnits,
    SSM,
    CutoffFilter,
    CPUSpeed,
#if USE_RDS
    RDSError,
#endif
    DisplayOff,
    BFO,
    UnitsSwitch,
    ScanSwitch,
    CWSwitch,
    CWPitch,
    ANB,
    SQL,
    SETTINGS_MAX
};

extern int8_t g_SettingSelected;
extern uint8_t g_menuLevel;
extern uint8_t g_menuCat;
extern bool g_SettingEditing;

//For managing BW
struct Bandwidth
{
    uint8_t idx;      //Internal SI473X index
    const char* desc;
};

extern int8_t g_bwIndexSSB;
extern Bandwidth g_bandwidthSSB[];
const uint8_t g_bwSSBMaxIdx = 5;

extern int8_t g_bwIndexAM;
const uint8_t g_maxFilterAM = 6;
extern Bandwidth g_bandwidthAM[];

extern int8_t g_bwIndexFM;
extern char* g_bandwidthFM[];

extern int g_tabStep[];
extern uint8_t g_amTotalSteps;
extern uint8_t g_amTotalStepsSSB;
extern uint8_t g_ssbTotalSteps;
extern int8_t g_stepIndexAM;
extern int8_t g_stepIndexSSB;

extern int8_t g_tabStepFM[];
extern int8_t g_FMStepIndex;
extern const int8_t g_lastStepFM;

//Band table structures
enum BandType : uint8_t
{
    LW_BAND_TYPE,
    MW_BAND_TYPE,
    SW_BAND_TYPE,
    FM_BAND_TYPE
};

struct Band
{
    uint16_t minimumFreq;
    uint16_t maximumFreq;
    uint16_t currentFreq;
    int8_t currentStepIdx;
    int8_t bandwidthIdx;     // Bandwidth table index (internal table in Si473x controller)
};

#if USE_RDS
enum RDSActiveInfo : uint8_t
{
    StationName,
    StationInfo,
    ProgramInfo
};
extern uint8_t g_rdsActiveInfo;
extern char g_rdsPrevLen;
extern char* g_RDSCells[3];
#endif

extern char* bandTags[];

extern Band g_bandList[];

extern uint16_t SWSubBands[];
extern const uint8_t g_SWSubBandCount;
extern const uint8_t g_lastBand;
extern int8_t g_bandIndex;

// Modulation
enum Modulations : uint8_t
{
    AM,
    LSB,
    USB,
    CW,
    FM
};
extern uint8_t g_currentMode;
extern const char* g_bandModeDesc[];
extern uint8_t g_prevMode;
extern uint8_t g_seekDirection;

//Special logic for fast and responsive frequency surfing
extern uint32_t g_lastFreqChange;
extern uint32_t g_rfPendingSince;
extern bool g_processFreqChange;
extern bool g_ssbNeedHwFreq;
extern uint8_t g_volume;

extern Rotary g_encoder;
extern SI4735 g_si4735;
