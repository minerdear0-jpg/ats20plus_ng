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

// 6x8 column-major (LSB = top). S, +, then digits 0-9. Page-tall so it stays off the 7-seg frequency.
const uint8_t kSmeterGlyphs[] PROGMEM = {
    0x26, 0x49, 0x49, 0x49, 0x32, 0x00,
    0x08, 0x08, 0x3E, 0x08, 0x08, 0x00,
    0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00,
    0x00, 0x42, 0x7F, 0x40, 0x00, 0x00,
    0x42, 0x61, 0x51, 0x49, 0x46, 0x00,
    0x21, 0x41, 0x45, 0x4B, 0x31, 0x00,
    0x18, 0x14, 0x12, 0x7F, 0x10, 0x00,
    0x27, 0x45, 0x45, 0x45, 0x39, 0x00,
    0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00,
    0x01, 0x71, 0x09, 0x05, 0x03, 0x00,
    0x36, 0x49, 0x49, 0x49, 0x36, 0x00,
    0x06, 0x49, 0x49, 0x29, 0x1E, 0x00
};

void oledDraw6x8(uint8_t x, uint8_t page, uint8_t glyph)
{
    oled.setCursor(x, page);
    oled.startData();
    uint16_t off = (uint16_t)glyph * 6;
    for (uint8_t i = 0; i < 6; i++)
        oled.sendData(pgm_read_byte(&kSmeterGlyphs[off + i]));
    oled.endData();
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