#pragma once

#include <avr/pgmspace.h>

long g_storeTime = millis();

bool g_voltagePinConnnected = false;
bool g_ssbLoaded = false;
bool g_fmStereo = true;

bool g_cmdVolume = false;
bool g_cmdStep = false;
bool g_cmdBw = false;
bool g_cmdBand = false;
bool g_settingsActive = false;
bool g_sMeterOn = false;
bool g_displayOn = true;
bool g_displayRDS = false;
bool g_rdsSwitchPressed = false;
bool g_seekStop = false;
uint32_t g_lastAdjustmentTime = 0;

uint8_t g_muteVolume = 0;
int g_currentBFO = 0;

// Encoder buttons
SimpleButton  btn_Bandwidth(BANDWIDTH_BUTTON);
SimpleButton  btn_BandUp(BAND_BUTTON);
SimpleButton  btn_BandDn(SOFTMUTE_BUTTON);
SimpleButton  btn_VolumeUp(VOLUME_BUTTON);
SimpleButton  btn_VolumeDn(AVC_BUTTON);
SimpleButton  btn_Encoder(ENCODER_BUTTON);
SimpleButton  btn_AGC(AGC_BUTTON);
SimpleButton  btn_Step(STEP_BUTTON);
SimpleButton  btn_Mode(MODE_SWITCH);

volatile int g_encoderCount = 0;

//Frequency tracking
uint16_t g_currentFrequency;
uint16_t g_previousFrequency;

enum SettingType
{
    ZeroAuto,
    Num,
    Switch,
    SwitchAuto
};

struct SettingsItem
{
    const char* name; // PROGMEM
    int8_t param;
    uint8_t type;
    void (*manipulateCallback)(int8_t);
};

const char SET_ATT[] PROGMEM = "ATT";
const char SET_SM[]  PROGMEM = "SM ";
const char SET_SVC[] PROGMEM = "SVC";
const char SET_SYN[] PROGMEM = "Syn";
const char SET_DEE[] PROGMEM = "DeE";
const char SET_AVC[] PROGMEM = "AVC";
const char SET_SCR[] PROGMEM = "Scr";
const char SET_SW[]  PROGMEM = "SW ";
const char SET_SSM[] PROGMEM = "SSM";
const char SET_COF[] PROGMEM = "COF";
const char SET_CPU[] PROGMEM = "CPU";
#if USE_RDS
const char SET_RDS[] PROGMEM = "RDS";
#endif
const char SET_BFO[] PROGMEM = "BFO";
const char SET_UNI[] PROGMEM = "Uni";
const char SET_SCA[] PROGMEM = "Sca";
const char SET_CW[]  PROGMEM = "CW ";

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
void doBFOCalibration(int8_t v);
void doUnitsSwitch(int8_t v = 0);
void doScanSwitch(int8_t v = 0);
void doCWSwitch(int8_t v = 0);

SettingsItem g_Settings[] =
{
    //Page 1
    { SET_ATT, 0,  SettingType::ZeroAuto,     doAttenuation     },  //Attenuation
    { SET_SM,  0,  SettingType::Num,          doSoftMute        },  //Soft Mute
    { SET_SVC, 1,  SettingType::Switch,       doSSBAVC          },  //SSB AVC Switch
    { SET_SYN, 0,  SettingType::Switch,       doSync            },  //SSB Sync
    { SET_DEE, 1,  SettingType::Switch,       doDeEmp           },  //FM DeEmphasis (0 - 50, 1 - 75)
    { SET_AVC, 46, SettingType::Num,          doAvc             },  //Automatic Volume Control
    //Page 2
    { SET_SCR, 80, SettingType::Num,          doBrightness      },  //Screen Brightness
    { SET_SW,  0,  SettingType::Switch,       doSWUnits         },  //SW Units
    { SET_SSM, 1,  SettingType::Switch,       doSSBSoftMuteMode },  //SSB Soft Mute Mode
    { SET_COF, 0,  SettingType::SwitchAuto,   doCutoffFilter    },  //SSB Cutoff Filter
    { SET_CPU, 0,  SettingType::Switch,       doCPUSpeed        },  //CPU Frequency
#if USE_RDS
    { SET_RDS, 1,  SettingType::Num,          doRDSErrorLevel   },  //RDS ErrorLevel
#endif
    //Page 3
    { SET_BFO, 0,  SettingType::Num,          doBFOCalibration  },  //BFO Offset calibration
    { SET_UNI, 1,  SettingType::Switch,       doUnitsSwitch     },  //Show/Hide frequency units
    { SET_SCA, 1,  SettingType::Switch,       doScanSwitch      },  //AM Encoder scan switch
    { SET_CW,  0,  SettingType::Switch,       doCWSwitch        },  //CW is LSB or USB
};

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
    BFO,
    UnitsSwitch,
    ScanSwitch,
    CWSwitch,
    SETTINGS_MAX
};

const uint8_t g_SettingsMaxPages = 3;
int8_t g_SettingSelected = 0;
int8_t g_SettingsPage = 1;
bool g_SettingEditing = false;

//For managing BW
struct Bandwidth
{
    uint8_t idx;      //Internal SI473X index
    const char* desc; // PROGMEM
};

const char BW_SSB_05[] PROGMEM = "0.5k";
const char BW_SSB_10[] PROGMEM = "1.0k";
const char BW_SSB_12[] PROGMEM = "1.2k";
const char BW_SSB_22[] PROGMEM = "2.2k";
const char BW_SSB_30[] PROGMEM = "3.0k";
const char BW_SSB_40[] PROGMEM = "4.0k";

int8_t g_bwIndexSSB = 4;
Bandwidth g_bandwidthSSB[] =
{
    { 4, BW_SSB_05 },
    { 5, BW_SSB_10 },
    { 0, BW_SSB_12 },
    { 1, BW_SSB_22 },
    { 2, BW_SSB_30 },
    { 3, BW_SSB_40 }
};
const uint8_t g_bwSSBMaxIdx = 5;

int8_t g_bwIndexAM = 4;
const uint8_t g_maxFilterAM = 6;

const char BW_AM_10[] PROGMEM = "1.0k";
const char BW_AM_18[] PROGMEM = "1.8k";
const char BW_AM_20[] PROGMEM = "2.0k";
const char BW_AM_25[] PROGMEM = "2.5k";
const char BW_AM_30[] PROGMEM = "3.0k";
const char BW_AM_40[] PROGMEM = "4.0k";
const char BW_AM_60[] PROGMEM = "6.0k";

Bandwidth g_bandwidthAM[] =
{
    { 4, BW_AM_10 }, // 0
    { 5, BW_AM_18 }, // 1
    { 3, BW_AM_20 }, // 2
    { 6, BW_AM_25 }, // 3
    { 2, BW_AM_30 }, // 4 - Default
    { 1, BW_AM_40 }, // 5
    { 0, BW_AM_60 }  // 6
};

int8_t g_bwIndexFM = 0;
const char BW_FM_AUTO[] PROGMEM = "AUTO";
const char BW_FM_110[]  PROGMEM = "110k";
const char BW_FM_84[]   PROGMEM = " 84k";
const char BW_FM_60[]   PROGMEM = " 60k";
const char BW_FM_40[]   PROGMEM = " 40k";
const char* const g_bandwidthFM[] =
{
    BW_FM_AUTO,
    BW_FM_110,
    BW_FM_84,
    BW_FM_60,
    BW_FM_40
};

int g_tabStep[] =
{
    // AM steps in KHz
    1,
    5,
    9,
    10,
    // Large AM steps in KHz
    50,
    100,
    1000,
    // SSB steps in Hz
    10,
    25,
    50,
    100,
    500
};
uint8_t g_amTotalSteps = 7;
uint8_t g_amTotalStepsSSB = 4; //Prevent large AM steps appear in SSB mode
uint8_t g_ssbTotalSteps = 5;
volatile int8_t g_stepIndex = 3;

int8_t g_tabStepFM[] =
{
    5,  // 50 KHz
    10, // 100 KHz
    100 // 1 MHz
};
int8_t g_FMStepIndex = 1;
const int8_t g_lastStepFM = (sizeof(g_tabStepFM) / sizeof(int8_t)) - 1;

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
uint8_t g_rdsActiveInfo = RDSActiveInfo::StationName;
char g_rdsPrevLen = 0;
char* g_RDSCells[3];
#endif

const char TAG_LW[] PROGMEM = "LW";
const char TAG_MW[] PROGMEM = "MW";
const char TAG_SW[] PROGMEM = "SW";
const char TAG_FM[] PROGMEM = "  ";
const char* const bandTags[] =
{
    TAG_LW,
    TAG_MW,
    TAG_SW,
    TAG_FM,
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
    SW_LIMIT_LOW,  // 160 Meter
    3500, // 80 Meter
    4500, 
    5600,
    6800, // 40 Meter
    7200, // 41 Meter
    8500, 
    10000, // 30 Meter
    11200,
    13400, 
    14000, // 20 Meter
    15000,
    17200, 
    18000, // 17 Meter
    21000, // 15 Meter
    21400, // 13 Meter
    24890, // 12 Meter
    CB_LIMIT_LOW, // CB Band (11 Meter)
    CB_LIMIT_HIGH  // 10 Meter
};
const uint8_t g_SWSubBandCount = sizeof(SWSubBands) / sizeof(uint16_t);
const uint8_t g_lastBand = (sizeof(g_bandList) / sizeof(Band)) - 1;
int8_t g_bandIndex = 1;

// Modulation
enum Modulations : uint8_t
{
    AM,
    LSB,
    USB,
    CW,
    FM
};
volatile uint8_t g_currentMode = FM;
const char MODE_AM[]  PROGMEM = "AM ";
const char MODE_LSB[] PROGMEM = "LSB";
const char MODE_USB[] PROGMEM = "USB";
const char MODE_CW[]  PROGMEM = "CW ";
const char MODE_FM[]  PROGMEM = "FM ";
const char* const g_bandModeDesc[] = 
{ 
    MODE_AM,
    MODE_LSB,
    MODE_USB,
    MODE_CW,
    MODE_FM
};
volatile uint8_t g_prevMode = FM;
uint8_t g_seekDirection = 1;

//Special logic for fast and responsive frequency surfing
uint32_t g_lastFreqChange = 0;
bool g_processFreqChange = 0;
uint8_t g_volume = DEFAULT_VOLUME;

Rotary g_encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
SI4735 g_si4735;