#include "game/game.h"

#include "character/character_view.h"
#include "core/log.h"
#include "game/launch_options.h"
#include "hud/hud.h"
#include "hud/law_screen.h"
#include "platform/window.h"
#include "scene.h"
#include "scenes/city_scene.h"
#include "scenes/lair_scene.h"
#include "scenes/warehouse_scene.h"
#include "sim/simulation.h"
#include "ui/ui.h"
#include "world/world_view.h"

#if defined(GAME_IMGUI_ENABLED)
#include "debug/debug_pad.h"
#include "dev/sim_cheats.h"

#include <imgui.h>
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
#include "dev/dev_driver.h"
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <thread>

namespace
{
    // Longest step the simulation takes (e.g. after a breakpoint).
    constexpr float kMaxDeltaSeconds = 0.1f;

    // Without focus, frames are throttled to about this rate (with vsync off the game would
    // otherwise keep a CPU core and the GPU busy in the background).
    constexpr std::chrono::milliseconds kBackgroundFrameTime(33);
}

Game::Game(IWindow& window, IRenderDevice& device, const LaunchOptions& options)
    : m_window(window)
    , m_device(device)
#if defined(GAME_IMGUI_ENABLED)
    , m_imgui(window, device)
    , m_devUI(window)
#endif
    , m_spriteBatch(device)
    , m_meshRenderer(device)
    , m_skinnedRenderer(device)
    , m_meshCache(device)
    , m_simulation(std::make_unique<Simulation>())
    , m_ui(std::make_unique<Ui>(device, window))
    , m_hud(std::make_unique<Hud>())
    , m_worldView(std::make_unique<WorldView>(m_meshCache))
    , m_characters(std::make_unique<CharacterView>(device))
{
    BuildScenes();

#if defined(GAME_IMGUI_ENABLED)
    DebugPad& pad = m_devUI.GetDebugPad();
    pad.RegisterCategory("View", [this]() {
        for (size_t i = 0; i < m_scenes.size(); ++i)
        {
            if (i > 0)
                ImGui::SameLine();
            if (ImGui::RadioButton(m_scenes[i]->GetName(), m_activeScene == i))
                SetActiveView(i);
        }
    });
    RegisterSimCheats(pad, *m_simulation);

    for (const std::unique_ptr<IScene>& scene : m_scenes)
        scene->RegisterCheats(pad);
#endif

#if defined(GAME_DEV_TOOLS_ENABLED)
    DevDriverHooks hooks;
    hooks.getView = [this]() { return m_scenes[m_activeScene]->GetName(); };
    hooks.setView = [this](const std::string& name) {
        for (size_t i = 0; i < m_scenes.size(); ++i)
        {
            if (_stricmp(m_scenes[i]->GetName(), name.c_str()) == 0)
            {
                SetActiveView(i);
                return true;
            }
        }
        return false;
    };
    hooks.getOrbitCamera = [this]() { return m_scenes[m_activeScene]->GetOrbitCamera(); };
    hooks.getTileSurfaceHeight = [this](const TileCoord& tile) { return m_scenes[m_activeScene]->GetTileSurfaceHeight(tile); };
#if defined(GAME_IMGUI_ENABLED)
    hooks.setDevUiVisible = [this](bool visible) { m_devUI.SetVisible(visible); };
    hooks.setDebugPadOpen = [this](bool open) { m_devUI.GetDebugPad().SetOpen(open); };
#endif
    m_devDriver = std::make_unique<DevDriver>(options.dev, window, device, *m_simulation, std::move(hooks));
#else
    (void)options;
#endif

    // Keep drawing while the window is dragged or resized (PumpEvents is stuck in the OS loop).
    m_window.SetModalLoopCallback([this]() { Frame(); });

    LOG_INFO("Game initialized");
}

Game::~Game()
{
    m_window.SetModalLoopCallback(nullptr);
#if defined(GAME_IMGUI_ENABLED)
    UnregisterSimCheats(m_devUI.GetDebugPad());
    m_devUI.GetDebugPad().UnregisterCategory("View");
#endif
}

void Game::BuildScenes()
{
    // In ViewId order. The lair starts active.
    const SceneServices services = { m_window, m_device, m_meshCache, m_simulation->GetState(), *m_worldView, *m_characters };
    m_scenes.clear();
    m_scenes.push_back(std::make_unique<WarehouseScene>(services));
    m_scenes.push_back(std::make_unique<LairScene>(services));
    m_scenes.push_back(std::make_unique<CityScene>(services));
    m_activeScene = (size_t)ViewId::Lair;
}

void Game::StartNewGame()
{
    m_scenes[m_activeScene]->OnDeactivate();
    m_simulation->NewGame();
    BuildScenes();
    m_hud = std::make_unique<Hud>();
#if defined(GAME_IMGUI_ENABLED)
    for (const std::unique_ptr<IScene>& scene : m_scenes)
        scene->RegisterCheats(m_devUI.GetDebugPad());
#endif
    LOG_INFO("New game");
}

void Game::SetActiveView(size_t index)
{
    if (index == m_activeScene || index >= m_scenes.size())
        return;
    m_scenes[m_activeScene]->OnDeactivate();
    m_activeScene = index;
    LOG_INFO("View: %s", m_scenes[index]->GetName());
}

int Game::Run()
{
    m_lastFrameTime = std::chrono::steady_clock::now();

    uint64_t frameCount = 0;
    while (m_window.PumpEvents())
    {
        const auto frameStart = std::chrono::steady_clock::now();
        Frame();
        ++frameCount;

        if (!m_window.HasFocus())
            std::this_thread::sleep_until(frameStart + kBackgroundFrameTime);
    }

    LOG_INFO("Game exiting after %llu frames", (unsigned long long)frameCount);
    return 0;
}

void Game::Frame()
{
    if (m_simulation->GetState().law.newGameWanted)
        StartNewGame();

    const auto  now          = std::chrono::steady_clock::now();
    const float deltaSeconds = std::min(std::chrono::duration<float>(now - m_lastFrameTime).count(), kMaxDeltaSeconds);
    m_lastFrameTime = now;

    m_device.Resize(m_window.GetWidth(), m_window.GetHeight());

    bool keyboardAvailable = true;
    bool mouseAvailable    = true;
    float devMenuHeight    = 0.0f;
#if defined(GAME_DEV_TOOLS_ENABLED)
    m_devDriver->BeginFrame(); // injected input and commands count for this frame
#endif
#if defined(GAME_IMGUI_ENABLED)
    m_imgui.BeginFrame();
    m_devUI.Update();
    keyboardAvailable = !m_imgui.WantsKeyboard();
    mouseAvailable    = !m_imgui.WantsMouse();
    if (m_devUI.IsVisible())
        devMenuHeight = ImGui::GetFrameHeight();
#endif
#if defined(GAME_DEV_TOOLS_ENABLED)
    if (m_devDriver->IsInjecting())
    {
        // The real cursor may be anywhere: ImGui must not take the simulated input.
        keyboardAvailable = true;
        mouseAvailable    = true;
    }
#endif

    const SimStep step = m_simulation->Advance(deltaSeconds);
#if defined(GAME_DEV_TOOLS_ENABLED)
    for (int i = 1; i < m_simulation->GetState().law.devTurbo; ++i)
        m_simulation->Advance(deltaSeconds);
#endif

    // Whoever the law is finished with.
    {
        std::vector<uint32_t>& departed = m_simulation->GetState().law.departedIds;
        for (uint32_t id : departed)
            m_characters->Forget(id);
        departed.clear();
    }

    // Behind one of the law's screens, the keys do nothing and the mouse is the screen's.
    if (m_simulation->GetState().law.screen != LawScreen::None)
        keyboardAvailable = false;

    m_ui->BeginFrame(mouseAvailable);
    const ViewId wanted = m_hud->Update(*m_ui, m_simulation->GetState(), m_window, keyboardAvailable, (ViewId)m_activeScene,
                                        devMenuHeight / m_ui->GetScale());
    SetActiveView((size_t)wanted);

    IScene& scene = *m_scenes[m_activeScene];
    scene.Update({ deltaSeconds, step, keyboardAvailable, mouseAvailable, *m_ui });
    m_hud->DrawNews(*m_ui, m_simulation->GetState(), deltaSeconds);
    DrawLawScreen(*m_ui, m_simulation->GetState());

    const Color clear         = scene.GetClearColor();
    const float clearColor[4] = { clear.r, clear.g, clear.b, clear.a };
    m_device.BeginFrame(clearColor);
    m_meshRenderer.ResetStats();
    m_skinnedRenderer.ResetStats();
    m_spriteBatch.ResetStats();
    scene.Draw({ m_spriteBatch, m_meshRenderer, m_skinnedRenderer });
    m_ui->Render(m_spriteBatch);
#if defined(GAME_IMGUI_ENABLED)
    m_imgui.Render();
#endif
#if defined(GAME_DEV_TOOLS_ENABLED)
    m_devDriver->EndFrame(); // screenshots see the finished frame
#endif
    m_device.EndFrame();
}
