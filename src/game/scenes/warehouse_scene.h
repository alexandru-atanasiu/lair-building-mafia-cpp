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
    bool    IsAnyoneInBackRoom() const;
    void    UpdateBackRoom(float realSeconds);
    void    DrawBackRoomRoof(MeshRenderer::Scope& translucent) const;
    void    DrawDockDoor(MeshRenderer::Scope& meshes) const;
    void    DrawTruck(MeshRenderer::Scope& meshes) const;
    void    DrawCrates(MeshRenderer::Scope& meshes) const;

    GameState&        m_state;
    LevelView         m_view;
    std::vector<Prop> m_props;
    std::unique_ptr<MeshRenderer> m_translucent; // the back room's roof

    MeshPtr m_crate;
    MeshPtr m_roof;
    MeshPtr m_wallUpper, m_wallTrim;
    MeshPtr m_shutter;
    MeshPtr m_truckChassis, m_truckCab, m_truckGlass, m_truckBed, m_truckTarp, m_truckGate, m_truckWheels, m_truckLights;

    float m_backRoomOpen = 0.0f; // 0: the walls stand whole; 1: cut down to see whoever's inside
    float m_backRoomHold = 0.0f; // real seconds left before they start to rise

#if defined(GAME_IMGUI_ENABLED)
    DebugPad* m_debugPad = nullptr;
#endif
};
