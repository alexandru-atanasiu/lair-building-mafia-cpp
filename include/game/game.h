#pragma once

#include "debug/dev_ui.h"
#include "debug/imgui_layer.h"
#include "render/mesh_cache.h"
#include "render/mesh_renderer.h"
#include "render/render_device.h"
#include "render/skinned_mesh_renderer.h"
#include "render/sprite_batch.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

class CharacterView;
class DevDriver;
class Hud;
class IScene;
class IWindow;
class Simulation;
class Ui;
class WorldView;
struct LaunchOptions;

// Top-level game context: owns the simulation and the views on it. Only depends on the
// platform/render interfaces.
class Game
{
public:
    Game(IWindow& window, IRenderDevice& device, const LaunchOptions& options);
    ~Game();

    // Runs the main loop until the window closes. Returns the process exit code.
    int Run();

private:
    void Frame();
    void SetActiveView(size_t index);

    IWindow&       m_window;
    IRenderDevice& m_device;

#if defined(GAME_IMGUI_ENABLED)
    ImGuiLayer m_imgui;
    DevUI      m_devUI;
#endif

    SpriteBatch         m_spriteBatch;
    MeshRenderer        m_meshRenderer;
    SkinnedMeshRenderer m_skinnedRenderer;
    MeshCache           m_meshCache;

    std::unique_ptr<Simulation>    m_simulation;
    std::unique_ptr<Ui>            m_ui;
    std::unique_ptr<Hud>           m_hud;
    std::unique_ptr<WorldView>     m_worldView;
    std::unique_ptr<CharacterView> m_characters;

    // One per ViewId, in its order; only the active one is updated and drawn. The simulation
    // keeps running under all of them.
    std::vector<std::unique_ptr<IScene>>  m_scenes;
    size_t                                m_activeScene = 0;
    std::chrono::steady_clock::time_point m_lastFrameTime;

#if defined(GAME_DEV_TOOLS_ENABLED)
    std::unique_ptr<DevDriver> m_devDriver;
#endif
};
