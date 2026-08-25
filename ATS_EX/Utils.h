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

void oledPrintModeBadge(const char* text, uint8_t x0 = 0, uint8_t sidePad = 0, uint8_t visForce = 0, bool filled = true, uint8_t page0 = 0)
{
    const DCfont* f = DEFAULT_FONT;
    uint8_t skip = 0;
    uint8_t vis;
    if (visForce)
        vis = visForce;
    else
    {
        while (text[skip] == ' ')
            skip++;
        vis = oledBadgeVis(text);
    }
    if (vis > 6)
        vis = 6;
    uint8_t boxW = (uint8_t)(vis * 8 + 2 * sidePad);
    uint8_t pad = (vis * 8 < boxW) ? (uint8_t)((boxW - vis * 8) / 2) : 0;

    for (uint8_t page = 0; page < 2; page++)
    {
        oled.setCursor(x0, page0 + page);
        oled.startData();
        for (uint8_t x = 0; x < boxW; x++)
        {
            uint8_t d = x;
            if ((uint8_t)(boxW - 1 - x) < d)
                d = (uint8_t)(boxW - 1 - x);
            uint8_t mask = (page == 0) ? 0xFE : 0x7F;
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

            uint8_t b = filled ? 0xFF : 0;
            if (x >= pad && x < pad + vis * 8)
            {
                uint8_t ci = (uint8_t)((x - pad) / 8);
                uint8_t col = (uint8_t)((x - pad) % 8);
                uint8_t c = (uint8_t)text[skip + ci];
                uint16_t off = (uint16_t)pobIndex(c) * 16 + (uint16_t)page * 8;
                uint8_t g = pgm_read_byte(&f->bitmap[off + col]);
                b = filled ? (uint8_t)~g : g;
            }
            oled.sendData(b & mask);
        }
        oled.endData();
    }
}

// Karat-3.5 5×7: MHz (and leftover glyphs).
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

void oledClear7segBand()
{
    for (uint8_t pg = 0; pg < 4; pg++)
    {
        oled.setCursor(0, (uint8_t)(UI_PAGE_FREQ - 1 + pg));
        oled.fillLength(0, 128);
    }
}

uint8_t oledFreqCharW(char c)
{
    uint8_t u = (uint8_t)c;
    if (u == '.')
        return FREQ_DOT_W;
    if (u >= '0' && u <= '9')
        return FREQ_CELL_W;
    return 0;
}

static void oledSendKenwoodCol(uint8_t pg, uint32_t g)
{
    g <<= FREQ_SHIFT;
    twiWrite((uint8_t)(g >> (8 * pg)));
}

void oledBlit7segChar(uint8_t x, char ch)
{
    uint8_t c = (uint8_t)ch;
    uint8_t slot = oledFreqCharW(ch);
    if (!slot)
        return;
    for (uint8_t pg = 0; pg < 4; pg++)
    {
        oledCmdDataStart(x, (uint8_t)(UI_PAGE_FREQ - 1 + pg));
        if (c == '.')
        {
            for (uint8_t col = 0; col < FREQ_DOT_W; col++)
            {
                uint32_t g = pgm_read_byte(&kFreqDot[col]);
                g |= (uint32_t)pgm_read_byte(&kFreqDot[FREQ_DOT_W + col]) << 8;
                g |= (uint32_t)pgm_read_byte(&kFreqDot[FREQ_DOT_W * 2 + col]) << 16;
                oledSendKenwoodCol(pg, g);
            }
        }
        else
        {
            uint8_t d = (uint8_t)(c - '0');
            uint8_t w = pgm_read_byte(&kFreqWidthKW[d]);
            uint8_t pad = pgm_read_byte(&kFreqColOffKW[d]);
            uint16_t off = pgm_read_word(&kKenwoodOff[d]);
            const uint8_t* base = ssd1306xled_font16x24vfoIcom + off;
            for (uint8_t z = 0; z < pad; z++)
                twiWrite(0);
            for (uint8_t col = 0; col < w; col++)
            {
                uint32_t g = pgm_read_byte(base + col);
                g |= (uint32_t)pgm_read_byte(base + w + col) << 8;
                g |= (uint32_t)pgm_read_byte(base + (uint16_t)w * 2 + col) << 16;
                oledSendKenwoodCol(pg, g);
            }
            uint8_t rest = (uint8_t)(FREQ_CELL_W - pad - w);
            for (uint8_t z = 0; z < rest; z++)
                twiWrite(0);
        }
        twiStop();
    }
}

void oledPrintMhz(uint8_t x0)
{
    static const uint8_t kSeq[3] PROGMEM = { 15, 16, 17 };
    if ((uint16_t)x0 + MHZ_LABEL_W > 128)
        x0 = (uint8_t)(128 - MHZ_LABEL_W);
    for (uint8_t pg = 0; pg < 2; pg++)
    {
        oled.setCursor(x0, UI_PAGE_FREQ + 1 + pg);
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
            uint16_t col = (uint16_t)b << FREQ_SHIFT;
            oled.sendData(pg ? (uint8_t)(col >> 8) : (uint8_t)col);
        }
        oled.endData();
    }
}

// Pictogram above MHz (pages 2–3), no chip fill. Outer parens 9 px, inner 5 px, 3×3 dot.
void oledPrintStereoChip(uint8_t x0, bool on)
{
    static const uint16_t kStereo[] PROGMEM = {
        0x0000, 0x0FF8, 0x0808, 0x0000,
        0x03E0, 0x0220, 0x0000,
        0x01C0, 0x01C0, 0x01C0,
        0x0000, 0x0220, 0x03E0, 0x0000,
        0x0808, 0x0FF8, 0x0000
    };
    if ((uint16_t)x0 + MHZ_LABEL_W > 128)
        x0 = (uint8_t)(128 - MHZ_LABEL_W);
    uint8_t boxW = MHZ_LABEL_W;
    for (uint8_t page = 0; page < 2; page++)
    {
        oled.setCursor(x0, (uint8_t)(UI_PAGE_FREQ - 1 + page));
        oled.startData();
        for (uint8_t x = 0; x < boxW; x++)
        {
            if (!on)
            {
                oled.sendData(0);
                continue;
            }
            uint16_t g = pgm_read_word(&kStereo[x]);
            uint8_t ink = page ? (uint8_t)(g >> 8) : (uint8_t)g;
            oled.sendData(ink);
        }
        oled.endData();
    }
}

void oledPrintSMeterLab(uint8_t sUnit, bool plus, bool forceS)
{
    static uint8_t prevU = 255;
    static uint8_t prevP = 255;
    uint8_t p = plus ? 1 : 0;
    if (!forceS && sUnit == prevU && p == prevP)
        return;
    uint8_t gw = SMETER_7X14_W;
    uint8_t gap = SMETER_S_GAP;
    uint8_t inner = (uint8_t)(gw + gap + gw + gw);
    uint8_t boxW = (uint8_t)(inner + 2 * BADGE_PAD);
    uint8_t pad = BADGE_PAD;
    uint8_t x0 = SMETER_LAB_X;
    uint8_t dGi = sUnit;
    uint8_t pGi = plus ? SMETER_7X14_PLUS : SMETER_7X14_SPC;
    for (uint8_t page = 0; page < 2; page++)
    {
        oled.setCursor(x0, UI_PAGE_SECONDARY + page);
        oled.startData();
        for (uint8_t x = 0; x < boxW; x++)
        {
            uint8_t d = x;
            if ((uint8_t)(boxW - 1 - x) < d)
                d = (uint8_t)(boxW - 1 - x);
            uint8_t mask = (page == 0) ? 0xFE : 0x7F;
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
            if (x >= pad && x < pad + inner)
            {
                uint8_t lx = (uint8_t)(x - pad);
                uint8_t gi = 255;
                uint8_t col = 0;
                if (lx < gw)
                {
                    gi = SMETER_7X14_S;
                    col = lx;
                }
                else if (lx >= (uint8_t)(gw + gap) && lx < (uint8_t)(gw + gap + gw))
                {
                    gi = dGi;
                    col = (uint8_t)(lx - gw - gap);
                }
                else if (lx >= (uint8_t)(gw + gap + gw))
                {
                    gi = pGi;
                    col = (uint8_t)(lx - gw - gap - gw);
                }
                if (gi != 255)
                {
                    uint16_t off = (uint16_t)gi * SMETER_7X14_BYTES
                        + (uint16_t)page * gw + col;
                    uint8_t g = pgm_read_byte(&ssd1306xled_font7x14smeter[off]);
                    b = (uint8_t)~g;
                }
            }
            oled.sendData(b & mask);
        }
        oled.endData();
    }
    prevU = sUnit;
    prevP = p;
}

// Fleeing-wall: rectangles sized to read as perspective (concept trapezoids stay in fonts/).
// SMETER_WALL_WIDE_GAP 1 restores the wide-break car-radio backup.
#if SMETER_WALL_WIDE_GAP
static const uint8_t kSmW[] PROGMEM = { 4, 5, 5, 6, 7, 8, 10, 12 };
static const uint8_t kSmGap[] PROGMEM = { 2, 2, 2, 2, 6, 3, 3 };
static const uint8_t kF0[] PROGMEM = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFE, 0xFE };
static const uint8_t kF1[] PROGMEM = { 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x3F, 0x7F, 0x7F };
static const uint8_t kO0[] PROGMEM = { 0x40, 0x20, 0x10, 0x08, 0x04, 0x04, 0x02, 0x02 };
static const uint8_t kO1[] PROGMEM = { 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x40, 0x40 };
#else
static const uint8_t kSmW[] PROGMEM = { 3, 3, 4, 5, 6, 8, 10, 13 };
static const uint8_t kSmGap[] PROGMEM = { 2, 2, 2, 2, 2, 2, 2 };
static const uint8_t kF0[] PROGMEM = { 0xC0, 0xE0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE };
static const uint8_t kF1[] PROGMEM = { 0x03, 0x07, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F };
static const uint8_t kO0[] PROGMEM = { 0x40, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02 };
static const uint8_t kO1[] PROGMEM = { 0x02, 0x04, 0x04, 0x08, 0x10, 0x20, 0x40, 0x40 };
#endif

static uint8_t smSegW(uint8_t i)
{
    return pgm_read_byte(&kSmW[i]);
}

static uint8_t smGapAfter(uint8_t i)
{
    return pgm_read_byte(&kSmGap[i]);
}

static uint8_t smBarW(void)
{
    uint8_t w = 0;
    for (uint8_t i = 0; i < SMETER_CUBES; i++)
    {
        w = (uint8_t)(w + smSegW(i));
        if (i != SMETER_CUBES - 1)
            w = (uint8_t)(w + smGapAfter(i));
    }
    return w;
}

static uint8_t smCubeEdge(uint8_t cur)
{
    if (!cur)
        return 0;
    uint8_t x = 0;
    for (uint8_t i = 0; i < cur; i++)
    {
        x = (uint8_t)(x + smSegW(i));
        if (i != (uint8_t)(cur - 1))
            x = (uint8_t)(x + smGapAfter(i));
    }
    return x;
}

static void smHornCol(uint8_t i, bool fill, bool side, uint8_t* d0, uint8_t* d1)
{
    if (fill || side)
    {
        *d0 = pgm_read_byte(&kF0[i]);
        *d1 = pgm_read_byte(&kF1[i]);
    }
    else
    {
        *d0 = pgm_read_byte(&kO0[i]);
        *d1 = pgm_read_byte(&kO1[i]);
    }
}

// Needle on the cube scale: at the right edge of `cur`, crawling toward the next cube inside the 6 dB S-step.
static uint8_t smBarLiveX(uint8_t rssi, uint8_t sUnit, uint8_t plusDb, uint8_t cur, uint8_t barMax)
{
    uint8_t e0 = smCubeEdge(cur);
    if (e0 > barMax)
        e0 = barMax;
    if (plusDb || cur >= SMETER_CUBES)
        return barMax;
    uint8_t e1 = smCubeEdge((uint8_t)(cur + 1));
    if (e1 > barMax)
        e1 = barMax;
    int16_t lo = (int16_t)sUnit * 6 - 20;
    if (lo < 0)
        lo = 0;
    int16_t t = (int16_t)rssi - lo;
    if (t < 0)
        t = 0;
    if (t > 5)
        t = 5;
    return (uint8_t)(e0 + (uint16_t)(e1 - e0) * (uint8_t)t / 6);
}

// FREQOFF cue: ui freqoff_cue_{left,stop,right}.png, 11×16, page-major.
static const uint8_t kFreqOffCue[3][22] PROGMEM = {
    { 0x80, 0xC0, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFE, 0xFF, 0xFF, 0x01, 0x03, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x3F, 0x7F, 0xFF, 0xFF },
    { 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x00 },
    { 0xFF, 0xFF, 0xFE, 0xFC, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0xC0, 0x80, 0xFF, 0xFF, 0x7F, 0x3F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x03, 0x01 }
};

void oledPrint(const char* text, int offX = -1, int offY = -1, const DCfont* font = LastFont, bool invert = false)
{
    oledSetFont(font);
    if (invert)
        oled.invertOutput(invert);
    if (offX >= 0 && offY >= 0)
        oled.setCursor(offX, offY);
    if (font == FONT8X16POB_UI)
    {
        while (*text)
            oled.write(pobIndex((uint8_t)*text++));
    }
    else
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