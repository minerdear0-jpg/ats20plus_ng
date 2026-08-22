#include "globals.h"

long g_storeTime = millis();

bool g_voltagePinConnnected = false;
bool g_ssbLoaded = false;
bool g_fmStereo = false;

bool g_cmdVolume = false;
bool g_cmdStep = false;
bool g_cmdBw = false;
bool g_cmdBand = false;
bool g_settingsActive = false;

uint8_t g_uiDirty = 0;
bool g_uiFreqClean = false;
uint8_t g_uiLayer = 0;
uint8_t g_uiFocus = 0;

bool g_sMeterOn = false;
uint8_t g_sMeterDrawnVal = 255;
bool g_displayOn = true;
bool g_displayRDS = false;
bool g_rdsSwitchPressed = false;
bool g_seekStop = false;
uint32_t g_lastAdjustmentTime = 0;

uint8_t g_muteVolume = 0;
int g_currentBFO = 0;

SimpleButton  btn_Bandwidth(BANDWIDTH_BUTTON);
SimpleButton  btn_BandUp(BAND_BUTTON);
SimpleButton  btn_BandDn(SOFTMUTE_BUTTON);
SimpleButton  btn_VolumeUp(VOLUME_BUTTON);
SimpleButton  btn_VolumeDn(AVC_BUTTON);
SimpleButton  btn_Encoder(ENCODER_BUTTON);
SimpleButton  btn_AGC(AGC_BUTTON);
SimpleButton  btn_Step(STEP_BUTTON);
SimpleButton  btn_Mode(MODE_SWITCH);

volatile int8_t g_encoderCount = 0;

uint16_t g_currentFrequency;
uint16_t g_previousFrequency;

SettingsItem g_Settings[] =
{
    //Page 1
    { "ATT", 0,  SettingType::ZeroAuto,     doAttenuation     },
    { "SM ", 0,  SettingType::Num,          doSoftMute        },
    { "SVC", 1,  SettingType::Switch,       doSSBAVC          },
    { "Syn", 0,  SettingType::Switch,       doSync            },
    { "DeE", 1,  SettingType::Switch,       doDeEmp           },
    { "AVC", 46, SettingType::Num,          doAvc             },
    //Page 2
    { "Scr", 80, SettingType::Num,          doBrightness      },
    { "SW ", 0,  SettingType::Switch,       doSWUnits         },
    { "SSM", 1,  SettingType::Switch,       doSSBSoftMuteMode },
    { "COF", 0,  SettingType::SwitchAuto,   doCutoffFilter    },
    { "CPU", 0,  SettingType::Switch,       doCPUSpeed        },
#if USE_RDS
    { "RDS", 1,  SettingType::Num,          doRDSErrorLevel   },
#endif
    //Page 3
    { "BFO", 0,  SettingType::Num,          doBFOCalibration  },
    { "Uni", 1,  SettingType::Switch,       doUnitsSwitch     },
    { "Sca", 1,  SettingType::Switch,       doScanSwitch      },
    { "CW ", 0,  SettingType::Switch,       doCWSwitch        },
    { "ANB", 0,  SettingType::Switch,       doANB             },
};

int8_t g_SettingSelected = 0;
int8_t g_SettingsPage = 1;
bool g_SettingEditing = false;

int8_t g_bwIndexSSB = 4;
Bandwidth g_bandwidthSSB[] =
{
    { 4, "0.5k" },
    { 5, "1.0k" },
    { 0, "1.2k" },
    { 1, "2.2k" },
    { 2, "3.0k" },
    { 3, "4.0k" }
};

int8_t g_bwIndexAM = 4;
Bandwidth g_bandwidthAM[] =
{
    { 4, "1.0k" },
    { 5, "1.8k" },
    { 3, "2.0k" },
    { 6, "2.5k" },
    { 2, "3.0k" },
    { 1, "4.0k" },
    { 0, "6.0k" }
};

int8_t g_bwIndexFM = 0;
char* g_bandwidthFM[] =
{
    "AUTO",
    "110k",
    " 84k",
    " 60k",
    " 40k"
};

int g_tabStep[] =
{
    1,
    5,
    9,
    10,
    50,
    100,
    1000,
    10,
    25,
    50,
    100,
    500
};
uint8_t g_amTotalSteps = 7;
uint8_t g_amTotalStepsSSB = 4;
uint8_t g_ssbTotalSteps = 5;
volatile int8_t g_stepIndexAM = 3;
volatile int8_t g_stepIndexSSB = 7;

int8_t g_tabStepFM[] =
{
    5,
    10,
    100
};
int8_t g_FMStepIndex = 1;
const int8_t g_lastStepFM = (sizeof(g_tabStepFM) / sizeof(int8_t)) - 1;

#if USE_RDS
uint8_t g_rdsActiveInfo = RDSActiveInfo::StationName;
char g_rdsPrevLen = 0;
char* g_RDSCells[3];
#endif

char* bandTags[] =
{
    "LW",
    "MW",
    "SW",
    "  ",
};

Band g_bandList[] =
{
    /* LW */ { LW_LIMIT_LOW, 520, 300, 0, 4 },
    /* MW */ { 520, 1710, 1476, 3, 4 },
    /* SW */ { SW_LIMIT_LOW, SW_LIMIT_HIGH, SW_LIMIT_LOW, 0, 4 },
    /* FM */ { 6400, 10800, 8400, 1, 0 },
};

uint16_t SWSubBands[] =
{
    SW_LIMIT_LOW,
    3500,
    4500,
    5600,
    6800,
    7200,
    8500,
    10000,
    11200,
    13400,
    14000,
    15000,
    17200,
    18000,
    21000,
    21400,
    24890,
    CB_LIMIT_LOW,
    CB_LIMIT_HIGH
};
const uint8_t g_SWSubBandCount = sizeof(SWSubBands) / sizeof(uint16_t);
const uint8_t g_lastBand = (sizeof(g_bandList) / sizeof(Band)) - 1;
int8_t g_bandIndex = 1;

volatile uint8_t g_currentMode = FM;
const char* g_bandModeDesc[] =
{
    "AM ",
    "LSB",
    "USB",
    "CW ",
    "FM "
};
volatile uint8_t g_prevMode = FM;
uint8_t g_seekDirection = 1;

uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = 0;
bool g_ssbNeedHwFreq = false;
uint8_t g_volume = DEFAULT_VOLUME;

Rotary g_encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735 g_si4735;
