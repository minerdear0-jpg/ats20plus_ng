#pragma once

const DCfont* LastFont = DEFAULT_FONT;

void oledSetFont(const DCfont* font)
{
    if (font && LastFont != font)
    {
        LastFont = font;
        oled.setFont(font);
    }
}

// Inverted rounded chip, 16 px tall. vis≤3 → 24 px (2-letter labels centered).
uint8_t oledBadgeVis(const char* text)
{
    uint8_t a = 0;
    while (text[a] == ' ')
        a++;
    uint8_t b = a;
    while (text[b])
        b++;
    while (b > a && text[b - 1] == ' ')
        b--;
    return (uint8_t)(b - a);
}

uint8_t oledBadgeWidth(const char* text, uint8_t sidePad = 0)
{
    uint8_t vis = oledBadgeVis(text);
    if (vis > 6)
        vis = 6;
    return (uint8_t)(vis * 8 + 2 * sidePad);
}

void oledPrintModeBadge(const char* text, uint8_t x0 = 0, uint8_t sidePad = 0)
{
    const DCfont* f = DEFAULT_FONT;
    uint8_t skip = 0;
    while (text[skip] == ' ')
        skip++;
    uint8_t vis = oledBadgeVis(text);
    if (vis > 6)
        vis = 6;
    uint8_t boxW = oledBadgeWidth(text, sidePad);
    uint8_t pad = (vis * 8 < boxW) ? (uint8_t)((boxW - vis * 8) / 2) : 0;

    for (uint8_t page = 0; page < 2; page++)
    {
        oled.setCursor(x0, page);
        oled.startData();
        for (uint8_t x = 0; x < boxW; x++)
        {
            uint8_t d = x;
            if ((uint8_t)(boxW - 1 - x) < d)
                d = (uint8_t)(boxW - 1 - x);
            uint8_t mask = 0xFF;
            if (page == 0)
            {
                if (d == 0)
                    mask = 0xF8;
                else if (d == 1)
                    mask = 0xFC;
                else if (d == 2)
                    mask = 0xFE;
            }
            else
            {
                if (d == 0)
                    mask = 0x1F;
                else if (d == 1)
                    mask = 0x3F;
                else if (d == 2)
                    mask = 0x7F;
            }

            uint8_t b = 0xFF;
            if (x >= pad && x < pad + vis * 8)
            {
                uint8_t ci = (uint8_t)((x - pad) / 8);
                uint8_t col = (uint8_t)((x - pad) % 8);
                uint8_t c = (uint8_t)text[skip + ci];
                if (c < f->first || c > f->last)
                    c = ' ';
                uint16_t off = (uint16_t)(c - f->first) * 16 + (uint16_t)page * 8;
                b = (uint8_t)~pgm_read_byte(&f->bitmap[off + col]);
            }
            oled.sendData(b & mask);
        }
        oled.endData();
    }
}

// Karat-3.5 5×7 (charMap.h): 0-9, S T E R O M H z +. Secondary / S-meter / STEREO / MHz.
#define STEREO_PAD 2
#define STEREO_CHIP_W 39
#define MHZ_LABEL_W 17

static const uint8_t kKarat5x7[][5] PROGMEM = {
    { 0x3E, 0x51, 0x49, 0x45, 0x3E },
    { 0x00, 0x42, 0x7F, 0x40, 0x00 },
    { 0x42, 0x61, 0x51, 0x49, 0x46 },
    { 0x21, 0x41, 0x45, 0x4B, 0x31 },
    { 0x18, 0x14, 0x12, 0x7F, 0x10 },
    { 0x27, 0x45, 0x45, 0x45, 0x39 },
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 },
    { 0x01, 0x71, 0x09, 0x05, 0x03 },
    { 0x36, 0x49, 0x49, 0x49, 0x36 },
    { 0x06, 0x49, 0x49, 0x29, 0x1E },
    { 0x46, 0x49, 0x49, 0x49, 0x31 },
    { 0x01, 0x01, 0x7F, 0x01, 0x01 },
    { 0x7F, 0x49, 0x49, 0x49, 0x41 },
    { 0x7F, 0x09, 0x19, 0x29, 0x46 },
    { 0x3E, 0x41, 0x41, 0x41, 0x3E },
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F },
    { 0x7F, 0x08, 0x08, 0x08, 0x7F },
    { 0x44, 0x64, 0x54, 0x4C, 0x44 },
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }
};

void oledPrintStereoChip(uint8_t x0, bool on)
{
    static const uint8_t kSeq[6] PROGMEM = { 10, 11, 12, 13, 12, 14 };

    if ((uint16_t)x0 + STEREO_CHIP_W > 128)
        x0 = (uint8_t)(128 - STEREO_CHIP_W);
    for (uint8_t page = 0; page < 2; page++)
    {
        oled.setCursor(x0, UI_PAGE_FREQ + page);
        oled.startData();
        for (uint8_t x = 0; x < STEREO_CHIP_W; x++)
        {
            if (!on)
            {
                oled.sendData(0);
                continue;
            }
            uint8_t d = x;
            if ((uint8_t)(STEREO_CHIP_W - 1 - x) < d)
                d = (uint8_t)(STEREO_CHIP_W - 1 - x);
            uint8_t mask = 0xFF;
            if (page == 0)
            {
                if (d == 0)
                    mask = 0xFC;
                else if (d == 1)
                    mask = 0xFE;
            }
            else
            {
                mask = 0x07;
                if (d == 0)
                    mask = 0x01;
                else if (d == 1)
                    mask = 0x03;
            }
            uint16_t col = 0x07FF;
            if (x >= STEREO_PAD && x < STEREO_PAD + 35)
            {
                uint8_t lx = (uint8_t)(x - STEREO_PAD);
                uint8_t c = (uint8_t)(lx % 6);
                if (c < 5)
                {
                    uint8_t letter = pgm_read_byte(&kSeq[lx / 6]);
                    uint8_t g = pgm_read_byte(&kKarat5x7[letter][c]);
                    col = (uint16_t)(0x07FF & ~((uint16_t)g << 2));
                }
            }
            oled.sendData((page ? (uint8_t)(col >> 8) : (uint8_t)col) & mask);
        }
        oled.endData();
    }
}

void oledPrintMhz(uint8_t x0)
{
    static const uint8_t kSeq[3] PROGMEM = { 15, 16, 17 };
    if ((uint16_t)x0 + MHZ_LABEL_W > 128)
        x0 = (uint8_t)(128 - MHZ_LABEL_W);
    oled.setCursor(x0, UI_PAGE_FREQ + 2);
    oled.startData();
    for (uint8_t x = 0; x < MHZ_LABEL_W; x++)
    {
        uint8_t b = 0;
        uint8_t c = (uint8_t)(x % 6);
        if (c < 5)
        {
            uint8_t letter = pgm_read_byte(&kSeq[x / 6]);
            b = pgm_read_byte(&kKarat5x7[letter][c]);
        }
        oled.sendData(b);
    }
    oled.endData();
}

void oledPrintSMeterLab(const char* text)
{
    oled.setCursor(0, UI_PAGE_SECONDARY);
    oled.fillLength(0, SMETER_BAR_X);
    oled.setCursor(0, UI_PAGE_SECONDARY + 1);
    oled.fillLength(0, SMETER_BAR_X);
    oled.setCursor(2, UI_PAGE_SECONDARY + 1);
    oled.startData();
    for (uint8_t i = 0; text[i]; i++)
    {
        uint8_t c = (uint8_t)text[i];
        uint8_t idx = 255;
        if (c >= '0' && c <= '9')
            idx = (uint8_t)(c - '0');
        else if (c == 'S')
            idx = 10;
        else if (c == '+')
            idx = 18;
        for (uint8_t col = 0; col < 5; col++)
        {
            uint8_t g = (idx == 255) ? 0 : pgm_read_byte(&kKarat5x7[idx][col]);
            oled.sendData((uint8_t)(g << 1));
        }
        oled.sendData(0);
    }
    oled.endData();
}

void oledPrint(const char* text, int offX = -1, int offY = -1, const DCfont* font = LastFont, bool invert = false)
{
    oledSetFont(font);
    if (invert)
        oled.invertOutput(invert);
    if (offX >= 0 && offY >= 0)
        oled.setCursor(offX, offY);
    oled.print(text);
    if (invert)
        oled.invertOutput(false);
}

void oledClearLine(uint8_t y)
{
    oled.setCursor(0, y);
    oled.fillLength(0, 128);
    oled.setCursor(0, y + 1);
    oled.fillLength(0, 128);
}

//Better than sprintf which has overwhelmingly large overhead, it helps to reduce binary size
void convertToChar(char* strValue, uint16_t value, uint8_t len, uint8_t dot = 0, uint8_t separator = 0, uint8_t space = ' ')
{
    char d;
    int8_t i;
    for (i = (len - 1); i >= 0; i--)
    {
        d = value % 10;
        value = value / 10;
        strValue[i] = d + 48;
    }
    strValue[len] = '\0';

    if (dot > 0)
    {
        for (int i = len; i >= dot; i--)
        {
            strValue[i + 1] = strValue[i];
        }
        strValue[dot] = separator;
        len = dot;
    }
    i = 0;
    len--;

    while ((i < len) && ('0' == strValue[i]))
    {
        strValue[i++] = space;
    }
}

//Measure integer digit length
int ilen(uint16_t n)
{
    if (n < 10)
        return 1;
    else if (n < 100)
        return 2;
    else if (n < 1000)
        return 3;
    else if (n < 10000)
        return 4;
    else
        return 5;
}

//Split KHz frequency + BFO to KHz and .00 tail
void splitFreq(uint16_t& khz, uint16_t& tail)
{
    int32_t freq = (uint32_t(g_currentFrequency) * 1000) + g_currentBFO;
    khz = freq / 1000;
    tail = abs(freq % 1000) / 10;
}

uint8_t strlen8(const char* str)
{
    uint8_t n = 0;
    while (str[n] != '\0')
        n++;
    return n;
}