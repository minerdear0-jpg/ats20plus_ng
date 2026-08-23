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