#include "ui_font.h"

#include "core/file.h"
#include "core/log.h"
#include "render/render_device.h"
#include "render/texture_file.h"

#include <algorithm>
#include <cstring>

namespace
{
    constexpr uint32_t kFontVersion = 1;

    // Reads little endian values from the .font file (layout: py/pack_font.py).
    class Reader
    {
    public:
        explicit Reader(const std::vector<uint8_t>& bytes) : m_bytes(bytes) {}

        template <typename T>
        bool Read(T& value)
        {
            if (m_pos + sizeof(T) > m_bytes.size())
                return false;
            memcpy(&value, m_bytes.data() + m_pos, sizeof(T));
            m_pos += sizeof(T);
            return true;
        }

    private:
        const std::vector<uint8_t>& m_bytes;
        size_t                      m_pos = 0;
    };

    bool ParseFont(const std::vector<uint8_t>& bytes, std::vector<UiFont::Strike>& strikes)
    {
        Reader   reader(bytes);
        char     magic[4]    = {};
        uint32_t version     = 0;
        uint32_t strikeCount = 0;
        if (!reader.Read(magic) || memcmp(magic, "LFNT", 4) != 0 || !reader.Read(version) ||
            version != kFontVersion || !reader.Read(strikeCount) || strikeCount == 0)
            return false;

        strikes.resize(strikeCount);
        for (UiFont::Strike& strike : strikes)
        {
            uint32_t glyphCount = 0;
            if (!reader.Read(strike.pixelSize) || !reader.Read(strike.ascent) || !reader.Read(strike.lineHeight) ||
                !reader.Read(glyphCount))
                return false;
            strike.glyphs.reserve(glyphCount);
            for (uint32_t i = 0; i < glyphCount; ++i)
            {
                uint32_t      codepoint = 0;
                UiFont::Glyph glyph;
                if (!reader.Read(codepoint) || !reader.Read(glyph.x) || !reader.Read(glyph.y) || !reader.Read(glyph.w) ||
                    !reader.Read(glyph.h) || !reader.Read(glyph.xOffset) || !reader.Read(glyph.yOffset) ||
                    !reader.Read(glyph.advance))
                    return false;
                strike.glyphs[codepoint] = glyph;
            }
        }
        std::sort(strikes.begin(), strikes.end(),
                  [](const UiFont::Strike& a, const UiFont::Strike& b) { return a.pixelSize < b.pixelSize; });
        return true;
    }
}

uint32_t NextUtf8(std::string_view text, size_t& pos)
{
    const uint8_t first = (uint8_t)text[pos++];
    int           extra = 0;
    uint32_t      cp    = first;
    if (first >= 0xF0)      { extra = 3; cp = first & 0x07; }
    else if (first >= 0xE0) { extra = 2; cp = first & 0x0F; }
    else if (first >= 0xC0) { extra = 1; cp = first & 0x1F; }
    else if (first >= 0x80) { return '?'; }

    for (int i = 0; i < extra; ++i)
    {
        if (pos >= text.size() || ((uint8_t)text[pos] & 0xC0) != 0x80)
            return '?';
        cp = (cp << 6) | ((uint8_t)text[pos++] & 0x3F);
    }
    return cp;
}

UiFont::UiFont(std::unique_ptr<ITexture> atlas, std::vector<Strike> strikes)
    : m_atlas(std::move(atlas))
    , m_strikes(std::move(strikes))
{
}

UiFont::~UiFont() = default;

const UiFont::Strike& UiFont::SelectStrike(float pixelSize, float& scale) const
{
    const Strike* chosen = &m_strikes.back();
    for (const Strike& strike : m_strikes)
    {
        if ((float)strike.pixelSize >= pixelSize - 0.01f)
        {
            chosen = &strike;
            break;
        }
    }
    scale = pixelSize / (float)chosen->pixelSize;
    return *chosen;
}

const UiFont::Glyph& UiFont::FindGlyph(const Strike& strike, uint32_t codepoint)
{
    static const Glyph kNoGlyph;
    auto found = strike.glyphs.find(codepoint);
    if (found == strike.glyphs.end())
        found = strike.glyphs.find('?');
    return found != strike.glyphs.end() ? found->second : kNoGlyph;
}

float UiFont::GetLineHeight(float pixelSize) const
{
    float scale = 1.0f;
    const Strike& strike = SelectStrike(pixelSize, scale);
    return strike.lineHeight * scale;
}

float UiFont::MeasureWidth(std::string_view text, float pixelSize) const
{
    float scale = 1.0f;
    const Strike& strike = SelectStrike(pixelSize, scale);
    float widest = 0.0f;
    float width  = 0.0f;
    for (size_t pos = 0; pos < text.size();)
    {
        const uint32_t cp = NextUtf8(text, pos);
        if (cp == '\n')
        {
            widest = std::max(widest, width);
            width  = 0.0f;
            continue;
        }
        width += FindGlyph(strike, cp).advance * scale;
    }
    return std::max(widest, width);
}

std::string UiFont::WrapText(std::string_view text, float pixelSize, float maxWidth) const
{
    std::string result(text);
    size_t      lineStart = 0;
    size_t      lastSpace = std::string::npos;
    for (size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] == '\n')
        {
            lineStart = i + 1;
            lastSpace = std::string::npos;
        }
        else if (result[i] == ' ')
        {
            if (MeasureWidth(std::string_view(result).substr(lineStart, i - lineStart), pixelSize) > maxWidth &&
                lastSpace != std::string::npos)
            {
                result[lastSpace] = '\n';
                lineStart = lastSpace + 1;
            }
            lastSpace = i;
        }
    }
    if (lastSpace != std::string::npos &&
        MeasureWidth(std::string_view(result).substr(lineStart), pixelSize) > maxWidth)
        result[lastSpace] = '\n';
    return result;
}

std::unique_ptr<UiFont> LoadUiFont(IRenderDevice& device, const char* stem)
{
    const std::string fontPath = std::string(stem) + ".font";
    const std::string texPath  = std::string(stem) + ".tex";

    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(fontPath.c_str(), bytes))
    {
        LOG_ERR("Can't read font '%s'", fontPath.c_str());
        return nullptr;
    }
    std::vector<UiFont::Strike> strikes;
    if (!ParseFont(bytes, strikes))
    {
        LOG_ERR("Invalid font file '%s'", fontPath.c_str());
        return nullptr;
    }
    std::unique_ptr<ITexture> atlas = LoadTexture(device, texPath.c_str());
    if (!atlas)
        return nullptr;
    return std::make_unique<UiFont>(std::move(atlas), std::move(strikes));
}
