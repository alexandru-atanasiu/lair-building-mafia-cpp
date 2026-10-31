#include "ui.h"

#include "core/fatal.h"
#include "platform/window.h"
#include "render/render_device.h"
#include "render/sprite_batch.h"
#include "render/texture_file.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{
    constexpr const char* kWhiteTexture = "ui/white.tex";
    constexpr const char* kRegularFont  = "fonts/pt_sans_regular";
    constexpr const char* kBoldFont     = "fonts/pt_sans_bold";

    constexpr float kTooltipSize    = 14.0f;
    constexpr float kTooltipPadding = 6.0f;
    constexpr float kTooltipOffset  = 16.0f; // from the cursor

    const Color kButtonFill     = { 0.20f, 0.17f, 0.13f, 1.0f };
    const Color kButtonHover    = { 0.31f, 0.26f, 0.18f, 1.0f };
    const Color kButtonPress    = { 0.14f, 0.12f, 0.09f, 1.0f };
    const Color kButtonSelected = { 0.50f, 0.39f, 0.17f, 1.0f };
    const Color kButtonDisabled = { 0.14f, 0.13f, 0.12f, 1.0f };
    const Color kButtonFrame    = { 0.55f, 0.45f, 0.25f, 1.0f };
    const Color kTextDisabled   = { 0.50f, 0.48f, 0.44f, 1.0f };
    const Color kShadow         = { 0.0f, 0.0f, 0.0f, 0.75f };
    const Color kBarBack        = { 0.05f, 0.05f, 0.05f, 0.9f };
}

Ui::Ui(IRenderDevice& device, const IWindow& window)
    : m_device(device)
    , m_window(window)
    , m_white(LoadOrFatal(LoadTexture(device, kWhiteTexture), kWhiteTexture))
    , m_regular(LoadOrFatal(LoadUiFont(device, kRegularFont), kRegularFont))
    , m_bold(LoadOrFatal(LoadUiFont(device, kBoldFont), kBoldFont))
    , m_painter(std::make_unique<UiPainter>(*m_white))
{
}

Ui::~Ui() = default;

void Ui::BeginFrame(bool mouseAvailable)
{
    m_scale  = std::max(m_window.GetDpiScale(), 0.25f);
    m_width  = (float)m_window.GetWidth() / m_scale;
    m_height = (float)m_window.GetHeight() / m_scale;
    m_mouse  = { (float)m_window.GetMouseX() / m_scale, (float)m_window.GetMouseY() / m_scale };

    m_mouseAvailable    = mouseAvailable;
    m_mouseOverUi       = false;
    m_pressedThisFrame  = m_window.WasMouseButtonPressed(MouseButton::Left);
    m_releasedThisFrame = m_window.WasMouseButtonReleased(MouseButton::Left);
    m_mouseDown         = m_window.IsMouseButtonDown(MouseButton::Left);

    m_topPanelLastFrame = m_mouseAvailable ? m_topPanelThisFrame : -1;
    m_topPanelThisFrame = -1;
    m_panelCount        = 0;
    m_contentTop        = 0.0f;
    m_tooltip.clear();
}

void Ui::Render(SpriteBatch& batch)
{
    if (!m_tooltip.empty())
    {
        const float     lineHeight = GetLineHeight(kTooltipSize);
        const float     width      = MeasureText(m_tooltip, kTooltipSize) + 2.0f * kTooltipPadding;
        const long long lines      = 1 + std::count(m_tooltip.begin(), m_tooltip.end(), '\n');
        const float     height     = lineHeight * (float)lines + 2.0f * kTooltipPadding;
        // Below-right of the cursor, kept on screen.
        const float x = std::min(m_mouse.x + kTooltipOffset, m_width - width);
        const float y = m_mouse.y + kTooltipOffset + height <= m_height ? m_mouse.y + kTooltipOffset : m_mouse.y - height - 4.0f;
        Fill({ x, y, width, height }, { 0.04f, 0.04f, 0.04f, 0.95f });
        Frame({ x, y, width, height }, 1.0f, kButtonFrame);
        TextStyle style;
        style.size   = kTooltipSize;
        style.shadow = false;
        Label({ x + kTooltipPadding, y + kTooltipPadding }, m_tooltip, style);
    }

    m_painter->Flush(batch, { (float)m_window.GetWidth(), (float)m_window.GetHeight() });
}

UiRect Ui::ToPhysical(const UiRect& r) const
{
    // Whole pixels, so edges stay crisp at any scale and neighbors don't get gaps.
    const float x0 = std::round(r.x * m_scale);
    const float y0 = std::round(r.y * m_scale);
    const float x1 = std::round((r.x + r.w) * m_scale);
    const float y1 = std::round((r.y + r.h) * m_scale);
    return { x0, y0, x1 - x0, y1 - y0 };
}

bool Ui::IsHovered(const UiRect& r)
{
    const bool over = m_mouseAvailable && r.Contains(m_mouse);
    if (over)
        m_mouseOverUi = true;
    return over && m_panelCount - 1 >= m_topPanelLastFrame;
}

void Ui::Panel(const UiRect& r, const PanelStyle& style)
{
    const int panel = m_panelCount++;
    if (m_mouseAvailable && r.Contains(m_mouse))
    {
        m_mouseOverUi       = true;
        m_topPanelThisFrame = panel;
    }
    const UiRect p = ToPhysical(r);
    m_painter->Fill(p, style.fill);
    m_painter->Frame(p, std::max(1.0f, std::round(m_scale)), style.frame);
}

void Ui::Fill(const UiRect& r, const Color& color)
{
    m_painter->Fill(ToPhysical(r), color);
}

void Ui::Frame(const UiRect& r, float thickness, const Color& color)
{
    m_painter->Frame(ToPhysical(r), std::max(1.0f, std::round(thickness * m_scale)), color);
}

void Ui::Image(const UiRect& r, const ITexture& texture, const Color& color, UvRect uv)
{
    m_painter->Image(texture, ToPhysical(r), color, uv);
}

void Ui::Label(Vec2 position, std::string_view text, const TextStyle& style)
{
    const UiFont& font = GetFont(style.bold);
    const float   size = style.size * m_scale;

    Vec2 p = { position.x * m_scale, position.y * m_scale };
    if (style.align != Align::Left)
    {
        const float width = font.MeasureWidth(text, size);
        p.x -= style.align == Align::Center ? 0.5f * width : width;
    }
    if (style.shadow)
    {
        const float offset = std::max(1.0f, std::round(m_scale));
        m_painter->Text(font, { p.x + offset, p.y + offset }, size, kShadow, text);
    }
    m_painter->Text(font, p, size, style.color, text);
}

void Ui::LabelIn(const UiRect& r, std::string_view text, TextStyle style)
{
    style.align = Align::Center;
    const long long lines = 1 + std::count(text.begin(), text.end(), '\n');
    const float height = GetFont(style.bold).GetLineHeight(style.size * m_scale) / m_scale * (float)lines;
    Label({ r.x + 0.5f * r.w, r.y + 0.5f * (r.h - height) }, text, style);
}

float Ui::MeasureText(std::string_view text, float size, bool bold) const
{
    return GetFont(bold).MeasureWidth(text, size * m_scale) / m_scale;
}

float Ui::GetLineHeight(float size) const
{
    return m_regular->GetLineHeight(size * m_scale) / m_scale;
}

std::string Ui::WrapText(std::string_view text, float maxWidth, float size, bool bold) const
{
    return GetFont(bold).WrapText(text, size * m_scale, maxWidth * m_scale);
}

Ui::Clickable Ui::UpdateClickable(const UiRect& r, std::string_view label, bool disabled)
{
    Clickable state;
    state.hovered = IsHovered(r) && !disabled;

    const size_t id = std::hash<std::string_view>{}(label) ^ ((size_t)(r.x * 8.0f) << 1) ^ ((size_t)(r.y * 8.0f) << 17) | 1u;
    if (state.hovered && m_pressedThisFrame)
        m_activeId = id;

    if (m_activeId == id)
    {
        state.pressed = state.hovered && (m_mouseDown || m_pressedThisFrame);
        if (m_releasedThisFrame)
        {
            state.clicked = state.hovered;
            m_activeId    = 0;
        }
        else if (!m_mouseDown && !m_pressedThisFrame)
        {
            m_activeId = 0; // released while the UI wasn't looking, e.g. on losing focus
        }
    }
    return state;
}

void Ui::DrawButtonBack(const UiRect& r, const Clickable& state, const ButtonStyle& style)
{
    Color fill = kButtonFill;
    if (style.disabled)
        fill = kButtonDisabled;
    else if (state.pressed)
        fill = kButtonPress;
    else if (style.selected)
        fill = kButtonSelected;
    else if (state.hovered)
        fill = kButtonHover;
    const UiRect p = ToPhysical(r);
    m_painter->Fill(p, fill);
    m_painter->Frame(p, std::max(1.0f, std::round(m_scale)), kButtonFrame);
}

bool Ui::Button(const UiRect& r, std::string_view label, const ButtonStyle& style)
{
    const Clickable state = UpdateClickable(r, label, style.disabled);
    DrawButtonBack(r, state, style);

    TextStyle text;
    text.size = style.textSize;
    text.bold = style.selected;
    if (style.disabled)
        text.color = kTextDisabled;
    LabelIn(r, label, text);
    return state.clicked;
}

bool Ui::IconButton(const UiRect& r, const ITexture& icon, std::string_view tooltip, const ButtonStyle& style)
{
    const Clickable state = UpdateClickable(r, tooltip, style.disabled);
    DrawButtonBack(r, state, style);

    const float  size  = std::min(r.w, r.h) * 0.72f;
    const UiRect inner = { r.x + 0.5f * (r.w - size), r.y + 0.5f * (r.h - size), size, size };
    Image(inner, icon, style.disabled ? kTextDisabled : Color{ 0.93f, 0.90f, 0.82f, 1.0f });

    if (state.hovered && !tooltip.empty())
        Tooltip(tooltip);
    return state.clicked;
}

void Ui::ProgressBar(const UiRect& r, float fraction, const Color& color, std::string_view label)
{
    const UiRect p = ToPhysical(r);
    m_painter->Fill(p, kBarBack);
    const float t = std::max(1.0f, std::round(m_scale));
    m_painter->Fill({ p.x + t, p.y + t, std::round((p.w - 2.0f * t) * std::clamp(fraction, 0.0f, 1.0f)), p.h - 2.0f * t }, color);
    m_painter->Frame(p, t, kButtonFrame);
    if (!label.empty())
    {
        TextStyle text;
        text.size = std::min(14.0f, r.h - 2.0f);
        LabelIn(r, label, text);
    }
}

void Ui::Tooltip(std::string_view text)
{
    m_tooltip = text;
}

const ITexture& Ui::Texture(const char* path)
{
    std::unique_ptr<ITexture>& slot = m_textures[path];
    if (!slot)
        slot = LoadOrFatal(LoadTexture(m_device, path), path);
    return *slot;
}
