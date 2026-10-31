#pragma once

#include "ui_font.h"
#include "ui_painter.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

class IRenderDevice;
class IWindow;
class ITexture;
class SpriteBatch;

// The game's UI, in every config (ImGui is dev-only and compiled out of ship). Immediate mode:
// call the widgets from a scene's Update every frame, in back-to-front order. What they draw is
// recorded, and Game draws it over the scene.
//
//   ui.Panel({ 20, 80, 240, 200 });
//   ui.Label({ 32, 92 }, "Crew", { .size = 20, .bold = true });
//   if (ui.Button({ 32, 130, 120, 32 }, "Hire"))
//       ...
//   ui.ProgressBar({ 32, 170, 200, 18 }, 0.4f);
//
// Coordinates and sizes are logical pixels from the top-left of the window: 1 logical pixel is 1
// physical pixel at 96 DPI, and everything is scaled by the window's DPI. Scenes check WantsMouse
// after building their UI and before using the mouse on the world.
class Ui
{
public:
    enum class Align
    {
        Left,
        Center,
        Right,
    };

    struct TextStyle
    {
        float size   = 16.0f;
        Color color  = { 0.93f, 0.90f, 0.82f, 1.0f };
        bool  bold   = false;
        bool  shadow = true;
        Align align  = Align::Left; // relative to the given position
    };

    struct PanelStyle
    {
        Color fill  = { 0.10f, 0.09f, 0.08f, 0.92f };
        Color frame = { 0.55f, 0.45f, 0.25f, 1.0f };
    };

    struct ButtonStyle
    {
        bool  disabled = false;
        bool  selected = false; // shown as on, e.g. the current view or speed
        float textSize = 16.0f;
    };

    Ui(IRenderDevice& device, const IWindow& window);
    ~Ui();

    Ui(const Ui&)            = delete;
    Ui& operator=(const Ui&) = delete;

    // Game calls these two around the frame. `mouseAvailable`: the dev UI isn't using the mouse.
    void BeginFrame(bool mouseAvailable);
    void Render(SpriteBatch& batch);

    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    float GetScale() const { return m_scale; }
    Vec2  GetMouse() const { return m_mouse; }

    // Top of the area free for scene UI, below the dev menu bar and the HUD.
    float GetContentTop() const { return m_contentTop; }
    void  SetContentTop(float y) { m_contentTop = y; }

    // True when the mouse is on game UI: built so far this frame, or any panel last frame.
    bool WantsMouse() const { return m_mouseOverUi || m_topPanelLastFrame >= 0; }
    // The mouse is over this rectangle (and no later panel covers it).
    bool IsHovered(const UiRect& r);

    // Takes the mouse from anything drawn earlier; widgets on top of it stay usable.
    void Panel(const UiRect& r, const PanelStyle& style);
    void Panel(const UiRect& r) { Panel(r, PanelStyle{}); }
    void Fill(const UiRect& r, const Color& color);
    void Frame(const UiRect& r, float thickness, const Color& color);
    // `texture`: e.g. from Texture().
    void Image(const UiRect& r, const ITexture& texture, const Color& color = {}, UvRect uv = {});

    void  Label(Vec2 position, std::string_view text, const TextStyle& style);
    void  Label(Vec2 position, std::string_view text) { Label(position, text, TextStyle{}); }
    // Text centered in a rectangle, both ways.
    void  LabelIn(const UiRect& r, std::string_view text, TextStyle style);
    float MeasureText(std::string_view text, float size = 16.0f, bool bold = false) const;
    float GetLineHeight(float size = 16.0f) const;
    // Text with line breaks added to fit `maxWidth`.
    std::string WrapText(std::string_view text, float maxWidth, float size = 16.0f, bool bold = false) const;

    // True on the frame a click completes (pressed and released over it). Hover and press show.
    bool Button(const UiRect& r, std::string_view label, const ButtonStyle& style);
    bool Button(const UiRect& r, std::string_view label) { return Button(r, label, ButtonStyle{}); }
    // `tooltip` shows while hovered; may be empty.
    bool IconButton(const UiRect& r, const ITexture& icon, std::string_view tooltip, const ButtonStyle& style);
    bool IconButton(const UiRect& r, const ITexture& icon, std::string_view tooltip) { return IconButton(r, icon, tooltip, ButtonStyle{}); }

    // `fraction` 0..1; `label` is centered on it and may be empty.
    void ProgressBar(const UiRect& r, float fraction, const Color& color = { 0.78f, 0.62f, 0.25f, 1.0f }, std::string_view label = {});

    // Shows a box of text at the mouse, over everything, this frame.
    void Tooltip(std::string_view text);

    // A cooked texture, relative to the data folder ("ui/icons/lair.tex"): loaded once, kept for
    // the Ui's lifetime. Fatal if it's missing.
    const ITexture& Texture(const char* path);

    // Direct access, in physical pixels, for anything the widgets don't cover.
    UiPainter& GetPainter() { return *m_painter; }

private:
    // State of a clickable widget this frame; `r` is logical. Its id is its label and position.
    struct Clickable
    {
        bool hovered = false;
        bool pressed = false;
        bool clicked = false;
    };
    Clickable UpdateClickable(const UiRect& r, std::string_view label, bool disabled);
    UiRect    ToPhysical(const UiRect& r) const;
    const UiFont& GetFont(bool bold) const { return bold ? *m_bold : *m_regular; }
    void      DrawButtonBack(const UiRect& r, const Clickable& state, const ButtonStyle& style);

    IRenderDevice& m_device;
    const IWindow& m_window;

    std::unique_ptr<ITexture>   m_white;
    std::unique_ptr<UiFont>     m_regular;
    std::unique_ptr<UiFont>     m_bold;
    std::unique_ptr<UiPainter>  m_painter;
    std::unordered_map<std::string, std::unique_ptr<ITexture>> m_textures;

    float m_scale  = 1.0f;
    float m_width  = 0.0f;
    float m_height = 0.0f;
    Vec2  m_mouse;
    float m_contentTop = 0.0f;
    bool  m_mouseAvailable = true;
    bool  m_mouseOverUi    = false;
    bool  m_pressedThisFrame  = false;
    bool  m_releasedThisFrame = false;
    bool  m_mouseDown = false;

    size_t m_activeId = 0; // the widget the left button went down on; 0 = none

    // Panels are numbered in the order they're drawn; widgets belong to the latest. A widget is
    // covered if a higher numbered panel was under the mouse last frame.
    int m_panelCount        = 0;
    int m_topPanelThisFrame = -1;
    int m_topPanelLastFrame = -1;

    std::string m_tooltip;
};
