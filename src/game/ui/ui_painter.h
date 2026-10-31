#pragma once

#include "core/color.h"
#include "core/math.h"
#include "render/sprite_batch.h"

#include <string_view>
#include <vector>

class ITexture;
class UiFont;

// Screen rectangle from the top-left of the window.
struct UiRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
};

// A list of textured / flat rectangles and text in screen pixels, in draw order, flushed to a
// SpriteBatch. Recorded while the game updates and drawn afterwards, so game UI can be written in
// one place (Ui, ui.h, builds on it, and is what game code normally uses). Textures and fonts must
// stay alive until Flush. `white` is a plain white texture for flat fills.
class UiPainter
{
public:
    explicit UiPainter(const ITexture& white);

    void Image(const ITexture& texture, const UiRect& r, const Color& color = {}, UvRect uv = {});
    void Fill(const UiRect& r, const Color& color) { Image(m_white, r, color); }
    // Outline of `thickness` pixels inside the rectangle.
    void Frame(const UiRect& r, float thickness, const Color& color);

    // `topLeft` is the top of the first line; '\n' starts a new line. At a font's exact strike size
    // glyphs land on whole pixels.
    void Text(const UiFont& font, Vec2 topLeft, float pixelSize, const Color& color, std::string_view text);

    // Draws everything recorded (between IRenderDevice::BeginFrame and EndFrame) and clears the list.
    void Flush(SpriteBatch& batch, Vec2 viewport);

private:
    struct Item
    {
        const ITexture* texture;
        UiRect          rect;
        Color           color;
        UvRect          uv;
    };

    const ITexture&   m_white;
    std::vector<Item> m_items;
};
