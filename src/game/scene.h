#pragma once

#include "core/color.h"
#include "sim/game_state.h"

class CharacterView;
class DebugPad;
class IRenderDevice;
class IWindow;
class MeshCache;
class MeshRenderer;
class OrbitCamera;
class SkinnedMeshRenderer;
class SpriteBatch;
class Ui;
class WorldView;

// The views of the one simulation.
enum class ViewId
{
    Warehouse,
    Lair,
    City,
    Count,
};

// What every scene is built with (owned by Game, outliving the scenes).
struct SceneServices
{
    IWindow&       window;
    IRenderDevice& device;
    MeshCache&     meshes;
    GameState&     state;
    WorldView&     worldView;
    CharacterView& characters;
};

// Renderers shared by all scenes (owned by Game).
struct SceneRenderers
{
    SpriteBatch&         sprites;
    MeshRenderer&        meshes;
    SkinnedMeshRenderer& skinned;
};

// One frame's input to the active scene.
struct SceneFrame
{
    float   realSeconds;       // wall clock: for the camera and UI, which work while paused
    SimStep sim;               // the simulation's step: for anything in the game world
    bool    keyboardAvailable; // the dev UI isn't using it
    bool    mouseAvailable;    // the dev UI isn't using it; also check Ui::WantsMouse
    Ui&     ui;                // the scene's UI goes here, see ui.h
};

// A view Game can switch to. Only the active scene is updated and drawn. Scenes hold no game
// state of their own: that lives in GameState, so it carries on while another view is shown.
class IScene
{
public:
    virtual ~IScene() = default;

    // For logs and dev tools.
    virtual const char* GetName() const = 0;

    // Back buffer clear color while the scene is drawn.
    virtual Color GetClearColor() const = 0;

    // Advances the view: input, camera, animation, UI. The HUD has already had its turn.
    virtual void Update(const SceneFrame& frame) = 0;

    // Draws the 3D world or 2D map (between IRenderDevice::BeginFrame and EndFrame). UI recorded
    // by Update is drawn over it afterwards.
    virtual void Draw(const SceneRenderers& renderers) const = 0;

    // Another scene became active: release whatever needs input to end (e.g. mouse-look).
    virtual void OnDeactivate() {}

    // The scene's 3D camera, if it has one (dev driver: tile positions on screen).
    virtual OrbitCamera* GetOrbitCamera() { return nullptr; }

    // Height above its floor of what the mouse hits on this tile (dev driver: clicking on earth).
    virtual float GetTileSurfaceHeight(const TileCoord&) const { return 0.0f; }

#if defined(GAME_IMGUI_ENABLED)
    // Registers the scene's DebugPad categories (unregistered when the scene is destroyed).
    // `pad` must outlive the scene.
    virtual void RegisterCheats(DebugPad& pad) { (void)pad; }
#endif
};
