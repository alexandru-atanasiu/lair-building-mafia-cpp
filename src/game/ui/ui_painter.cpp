#include "ui_painter.h"

#include "render/camera_2d.h"
#include "render/render_device.h"
#include "ui_font.h"

#include <cmath>

UiPainter::UiPainter(const ITexture& white)
    : m_white(white)
{
}

void UiPainter::Image(const ITexture& texture, const UiRect& r, const Color& color, UvRect uv)
{
    m_items.push_back({ &texture, r, color, uv });
}

void UiPainter::Frame(const UiRect& r, float t, const Color& color)
{
    Fill({ r.x, r.y, r.w, t }, color);
    Fill({ r.x, r.y + r.h - t, r.w, t }, color);
    Fill({ r.x, r.y + t, t, r.h - 2.0f * t }, color);
    Fill({ r.x + r.w - t, r.y + t, t, r.h - 2.0f * t }, color);
}

void UiPainter::Text(const UiFont& font, Vec2 topLeft, float pixelSize, const Color& color, std::string_view text)
{
    float                 scale  = 1.0f;
    const UiFont::Strike& strike = font.SelectStrike(pixelSize, scale);
    const ITexture&       atlas  = font.GetAtlas();
    const float           atlasW = (float)atlas.GetWidth();
    const float           atlasH = (float)atlas.GetHeight();
    const bool            exact  = std::abs(scale - 1.0f) < 0.001f;

    const float left = std::round(topLeft.x);
    float       penX = left;
    float       top  = std::round(topLeft.y);
    for (size_t pos = 0; pos < text.size();)
    {
        const uint32_t cp = NextUtf8(text, pos);
        if (cp == '\n')
        {
            penX = left;
            top += std::round(strike.lineHeight * scale);
            continue;
        }
        const UiFont::Glyph& glyph = UiFont::FindGlyph(strike, cp);
        if (glyph.w > 0 && glyph.h > 0)
        {
            UiRect r = { penX + (float)glyph.xOffset * scale, top + (strike.ascent + (float)glyph.yOffset) * scale,
                         (float)glyph.w * scale, (float)glyph.h * scale };
            if (exact)
            {
                r.x = std::round(r.x);
                r.y = std::round(r.y);
            }
            const UvRect uv = { (float)glyph.x / atlasW, (float)glyph.y / atlasH,
                                (float)(glyph.x + glyph.w) / atlasW, (float)(glyph.y + glyph.h) / atlasH };
            Image(atlas, r, color, uv);
        }
        penX += glyph.advance * scale;
    }
}

void UiPainter::Flush(SpriteBatch& batch, Vec2 viewport)
{
    if (m_items.empty())
        return;

    // Screen pixels: world = (x, height - y) at zoom 1 with the camera on the viewport center.
    Camera2D camera;
    camera.SetViewportSize((uint32_t)viewport.x, (uint32_t)viewport.y);
    camera.SetPosition(viewport * 0.5f);

    SpriteBatch::Scope sprites = batch.Begin(camera);
    for (const Item& item : m_items)
    {
        Sprite sprite;
        sprite.position = { item.rect.x + 0.5f * item.rect.w, viewport.y - (item.rect.y + 0.5f * item.rect.h) };
        sprite.size     = { item.rect.w, item.rect.h };
        sprite.uv       = item.uv;
        sprite.color    = item.color;
        sprites.Draw(*item.texture, sprite);
    }
    m_items.clear();
}
