#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class IRenderDevice;
class ITexture;

// A bitmap font baked at cook time from assets/fonts/<name>.ttf by py/pack_font.py, at several
// pixel sizes ("strikes") into one atlas. Text at a size asks for the closest strike at least that
// big, so it's sharp at every DPI scale. Text is UTF-8; missing characters show as '?'.
class UiFont
{
public:
    struct Glyph
    {
        uint16_t x = 0, y = 0, w = 0, h = 0; // atlas rectangle, pixels
        int16_t  xOffset = 0;                // of the rectangle from the pen, strike pixels
        int16_t  yOffset = 0;                // of the rectangle's top from the baseline
        float    advance = 0.0f;
    };

    struct Strike
    {
        uint32_t pixelSize  = 0;
        float    ascent     = 0.0f;
        float    lineHeight = 0.0f;
        std::unordered_map<uint32_t, Glyph> glyphs;
    };

    UiFont(std::unique_ptr<ITexture> atlas, std::vector<Strike> strikes);
    ~UiFont();

    UiFont(const UiFont&)            = delete;
    UiFont& operator=(const UiFont&) = delete;

    const ITexture& GetAtlas() const { return *m_atlas; }

    // The strike for text of `pixelSize`; `scale` is what its metrics get multiplied by.
    const Strike& SelectStrike(float pixelSize, float& scale) const;
    // The character's glyph, or '?' for a character the font doesn't have.
    static const Glyph& FindGlyph(const Strike& strike, uint32_t codepoint);

    float GetLineHeight(float pixelSize) const;
    // Width of the widest line ('\n' starts a new one).
    float MeasureWidth(std::string_view text, float pixelSize) const;
    // Replaces spaces with line breaks so no line is wider than `maxWidth` (when it can).
    std::string WrapText(std::string_view text, float pixelSize, float maxWidth) const;

private:
    std::unique_ptr<ITexture> m_atlas;
    std::vector<Strike>       m_strikes; // smallest first
};

// Reads the next UTF-8 character of `text` from `pos`, advancing it. Malformed bytes read as '?'.
uint32_t NextUtf8(std::string_view text, size_t& pos);

// Loads <stem>.font and <stem>.tex (relative to the data folder). Logs and returns null on failure.
std::unique_ptr<UiFont> LoadUiFont(IRenderDevice& device, const char* stem);
