#pragma once

#include "../scene.h"
#include "level_view.h"

#include <memory>
#include <vector>

class Mesh;

// The ground level: the warehouse, the front, with its yard and the road. Everything fixed here is
// dressing built once in the constructor; the truck and the crates are drawn from WarehouseSim.
class WarehouseScene final : public IScene
{
public:
    explicit WarehouseScene(const SceneServices& services);
    ~WarehouseScene() override;

    const char* GetName() const override { return "Warehouse"; }
    Color       GetClearColor() const override { return { 0.30f, 0.36f, 0.42f, 1.0f }; }
    void        Update(const SceneFrame& frame) override;
    void        Draw(const SceneRenderers& renderers) const override;
    OrbitCamera* GetOrbitCamera() override { return &m_view.GetCamera(); }

#if defined(GAME_IMGUI_ENABLED)
    void RegisterCheats(DebugPad& pad) override;
#endif

private:
    using MeshPtr = std::shared_ptr<const Mesh>;

    struct Prop
    {
        MeshPtr mesh;
        Mat4    world;
        Color   color;
        float   emissive = 0.0f;
    };

    MeshPtr Load(const SceneServices& services, const char* path) const;
    void    BuildProps(const SceneServices& services);
    void    DrawTallWalls(MeshRenderer::Scope& meshes) const;
    WorldView::WallLook BackRoomWallLook(const TileCoord& tile, EdgeSide side) const;
    void    DrawDockDoor(MeshRenderer::Scope& meshes) const;
    void    DrawTruck(MeshRenderer::Scope& meshes) const;
    void    DrawCrates(MeshRenderer::Scope& meshes) const;

    GameState&        m_state;
    LevelView         m_view;
    std::vector<Prop> m_props;

    MeshPtr m_crate;
    MeshPtr m_wallUpper, m_wallTrim;
    MeshPtr m_shutter;
    MeshPtr m_truckChassis, m_truckCab, m_truckGlass, m_truckBed, m_truckTarp, m_truckGate, m_truckWheels, m_truckLights;

#if defined(GAME_IMGUI_ENABLED)
    DebugPad* m_debugPad = nullptr;
#endif
};
