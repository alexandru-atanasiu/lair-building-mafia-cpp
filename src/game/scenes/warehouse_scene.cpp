#include "warehouse_scene.h"

#include "../ui/ui.h"
#include "../world/warehouse_layout.h"
#include "../world/world_layout.h"
#include "../world/world_view.h"
#include "warehouse_panel.h"
#include "core/fatal.h"
#include "render/mesh.h"
#include "render/mesh_cache.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#if defined(GAME_IMGUI_ENABLED)
#include "debug/debug_pad.h"
#endif

namespace
{
    // From the road side, looking in over the docked truck at the racks.
    constexpr float     kStartDistance = 28.0f;
    constexpr float     kStartYaw      = DegToRad(-28.0f);
    constexpr TileCoord kStartFocus    = { WorldLevel::Ground, 24, 17 };

    constexpr float kHalfPi = 0.5f * kPi;

    // The truck's meshes; keep in sync with py/create_warehouse_meshes.py.
    constexpr float kBedFrontZ   = 1.45f;
    constexpr float kBedRearZ    = -4.25f;
    constexpr float kBedFloorY   = 1.05f;
    constexpr float kGateHingeX  = 1.2f;
    constexpr float kTarpRolled  = 0.09f; // squeezed to this much of its length when open
    constexpr float kGateOpenAngle = DegToRad(105.0f);
    constexpr float kCargoRowSpacing = 0.95f;
    constexpr float kCargoColumnX    = 0.58f;
    constexpr float kCargoScale      = 0.9f; // leaves gaps, so a load reads as separate crates

    // The big door, in the middle of the two door tiles' outer edge.
    constexpr float kDockFrameYaw = -kHalfPi; // the meshes' -Z (outward) points along +X
    constexpr float kShutterTop   = 3.2f;
    constexpr float kShutterRolled = 0.05f;

    // A crate in someone's arms.
    constexpr float kCarryForward = 0.42f;
    constexpr float kCarryHeight  = 0.62f;
    constexpr float kCarryScale   = 0.72f;

    // A wall stays low when its outside faces the camera at more than this (cos of the angle).
    constexpr float kCutawayFacing = -0.65f;

    const Color kCrateColors[] = {
        { 0.62f, 0.47f, 0.27f, 1.0f }, // cigarettes: pale pine
        { 0.40f, 0.26f, 0.15f, 1.0f }, // booze: dark oak
        { 0.27f, 0.33f, 0.24f, 1.0f }, // contraband: army green
    };

    const Color kTruckChassis = { 0.11f, 0.11f, 0.12f, 1.0f };
    const Color kTruckCab     = { 0.20f, 0.36f, 0.42f, 1.0f };
    const Color kTruckGlass   = { 0.10f, 0.14f, 0.18f, 1.0f };
    const Color kTruckBed     = { 0.27f, 0.29f, 0.31f, 1.0f };
    const Color kTruckTarp    = { 0.36f, 0.38f, 0.26f, 1.0f };
    const Color kTruckWheels  = { 0.07f, 0.07f, 0.07f, 1.0f };
    const Color kTruckLights  = { 0.95f, 0.82f, 0.45f, 1.0f };

    const Color kWallUpper    = { 0.55f, 0.50f, 0.43f, 1.0f };
    const Color kWallTrim     = { 0.24f, 0.26f, 0.28f, 1.0f };
    const Color kDockFrame    = { 0.26f, 0.28f, 0.30f, 1.0f };
    const Color kShutter      = { 0.46f, 0.50f, 0.52f, 1.0f };
    const Color kHazard       = { 0.80f, 0.62f, 0.12f, 1.0f };
    const Color kBumper       = { 0.70f, 0.55f, 0.12f, 1.0f };
    const Color kRackFrame    = { 0.20f, 0.27f, 0.38f, 1.0f };
    const Color kRackBeams    = { 0.72f, 0.42f, 0.14f, 1.0f };
    const Color kPallet       = { 0.56f, 0.44f, 0.28f, 1.0f };
    const Color kCarton       = { 0.66f, 0.56f, 0.40f, 1.0f };
    const Color kCartonDark   = { 0.57f, 0.49f, 0.36f, 1.0f };
    const Color kForklift     = { 0.72f, 0.56f, 0.14f, 1.0f };
    const Color kDark         = { 0.10f, 0.10f, 0.11f, 1.0f };
    const Color kDesk         = { 0.36f, 0.25f, 0.16f, 1.0f };
    const Color kCabinet      = { 0.34f, 0.37f, 0.36f, 1.0f };
    const Color kRail         = { 0.72f, 0.56f, 0.14f, 1.0f };
    const Color kGround       = { 0.20f, 0.23f, 0.16f, 1.0f };
    const Color kWeeds        = { 0.24f, 0.27f, 0.17f, 1.0f };
    const Color kAsphalt      = { 0.13f, 0.13f, 0.14f, 1.0f };
    const Color kRoadMarks    = { 0.70f, 0.68f, 0.60f, 1.0f };
    const Color kFence        = { 0.32f, 0.34f, 0.36f, 1.0f };
    const Color kLamp         = { 0.22f, 0.23f, 0.25f, 1.0f };
    const Color kContainerA   = { 0.25f, 0.36f, 0.42f, 1.0f };
    const Color kContainerB   = { 0.46f, 0.34f, 0.20f, 1.0f };
    const Color kBlocks[]     = { { 0.36f, 0.31f, 0.28f, 1.0f }, { 0.31f, 0.33f, 0.35f, 1.0f }, { 0.38f, 0.35f, 0.29f, 1.0f } };
    const Color kWindows      = { 0.12f, 0.15f, 0.18f, 1.0f };
    const Color kDrums        = { 0.24f, 0.32f, 0.26f, 1.0f };

    constexpr const char* kMeshDir = "meshes/warehouse/";

    // Daylight.
    MeshLighting MakeLighting(const WorldGrid& world)
    {
        MeshLighting lighting;
        lighting.directional.direction = Normalize({ -0.45f, -0.9f, 0.5f }); // from the road side, so the dock wall is lit
        lighting.directional.color     = { 1.0f, 0.97f, 0.9f, 1.0f };
        lighting.directional.intensity = 0.8f;
        lighting.light.position        = world.TileCenter(GetWarehouseCenter()) + Vec3{ 0.0f, 6.0f, 0.0f };
        lighting.light.intensity       = 0.3f;
        lighting.light.range           = 30.0f;
        lighting.ambient               = { 0.30f, 0.30f, 0.33f, 1.0f };
        lighting.specular              = 0.1f;
        return lighting;
    }

    Vec3 DockPosition(const WorldGrid& world)
    {
        // The shared corner of the two door tiles' outer edges.
        return world.TileCenter(kDockDoorTiles[0]) + Vec3{ 0.5f * kTileSize, 0.0f, 0.5f * kTileSize };
    }

    Mat4 Place(Vec3 position, float yaw = 0.0f)
    {
        return Mat4::Translation(position) * Mat4::RotationY(yaw);
    }
}

WarehouseScene::MeshPtr WarehouseScene::Load(const SceneServices& services, const char* name) const
{
    const std::string path = std::string(kMeshDir) + name + ".mesh";
    return LoadOrFatal(services.meshes.Load(path.c_str()), path.c_str());
}

WarehouseScene::WarehouseScene(const SceneServices& services)
    : m_state(services.state)
    , m_view(services, WorldLevel::Ground, kStartFocus, MakeLighting(services.state.world))
{
    m_view.GetCamera().SetDistance(kStartDistance);
    m_view.GetCamera().SetYaw(kStartYaw);

    constexpr const char* kCrateMesh = "meshes/props/crate.mesh";
    m_crate        = LoadOrFatal(services.meshes.Load(kCrateMesh), kCrateMesh);
    m_wallUpper    = Load(services, "wall_upper");
    m_wallTrim     = Load(services, "wall_trim");
    m_shutter      = Load(services, "dock_shutter");
    m_truckChassis = Load(services, "truck_chassis");
    m_truckCab     = Load(services, "truck_cab");
    m_truckGlass   = Load(services, "truck_glass");
    m_truckBed     = Load(services, "truck_bed");
    m_truckTarp    = Load(services, "truck_tarp");
    m_truckGate    = Load(services, "truck_gate");
    m_truckWheels  = Load(services, "truck_wheels");
    m_truckLights  = Load(services, "truck_lights");

    BuildProps(services);
}

WarehouseScene::~WarehouseScene()
{
#if defined(GAME_IMGUI_ENABLED)
    if (m_debugPad)
        m_debugPad->UnregisterCategory("Warehouse camera");
#endif
}

void WarehouseScene::BuildProps(const SceneServices& services)
{
    const WorldGrid& world = services.state.world;
    const auto add = [&](const char* name, const Mat4& at, const Color& color) {
        m_props.push_back({ Load(services, name), at, color, 0.0f });
    };

    // ---- Outside: the land, a rough lot on the sides the yard doesn't use, and the road.
    const auto land = [&](float x0, float z0, float x1, float z1) {
        add("weeds_patch", Mat4::TRS({ 0.5f * (x0 + x1), -0.13f, 0.5f * (z0 + z1) }, Mat4::Identity(), { 0.5f * (x1 - x0), 1.0f, 0.5f * (z1 - z0) }), kGround);
    };
    // Everywhere under the tiles, around the hole of the stairwell.
    const Vec3 well0 = world.TileCenter({ WorldLevel::Ground, 20, 15 }) - Vec3{ 1.0f, 0.0f, 1.0f };
    const Vec3 well1 = well0 + Vec3{ 2.0f, 0.0f, 4.0f };
    land(-400.0f, -400.0f, well0.x, 400.0f);
    land(well1.x, -400.0f, 400.0f, 400.0f);
    land(well0.x, well1.z, well1.x, 400.0f);
    land(well0.x, -400.0f, well1.x, well0.z);

    const auto weeds = [&](float x0, float z0, float x1, float z1) {
        add("weeds_patch", Mat4::TRS({ 0.5f * (x0 + x1), 0.005f, 0.5f * (z0 + z1) }, Mat4::Identity(), { 0.5f * (x1 - x0), 1.0f, 0.5f * (z1 - z0) }), kWeeds);
    };
    weeds(-40.0f, -40.0f, -16.0f, 40.0f); // left of the building
    weeds(-16.0f, 8.0f, 28.0f, 40.0f);    // behind it
    weeds(-16.0f, -40.0f, -3.0f, -20.0f); // in front, leaving a walk from the door
    weeds(1.0f, -40.0f, 28.0f, -24.0f);

    add("road", Mat4::Translation({ 32.0f, 0.0f, 0.0f }), kAsphalt);
    add("road_marks", Mat4::Translation({ 32.0f, 0.0f, 0.0f }), kRoadMarks);

    // ---- The yard's fence, with the gate gap on the road side.
    const auto fenceRun = [&](float x0, float z0, float x1, float z1) {
        const int  panels = (int)std::lround(std::max(std::abs(x1 - x0), std::abs(z1 - z0)) / kTileSize);
        const float yaw   = std::abs(x1 - x0) > 0.0f ? 0.0f : kHalfPi;
        for (int i = 0; i < panels; ++i)
        {
            const float t = ((float)i + 0.5f) / (float)panels;
            add("fence_panel", Place({ x0 + (x1 - x0) * t, 0.0f, z0 + (z1 - z0) * t }, yaw), kFence);
        }
    };
    fenceRun(16.0f, -20.0f, 28.0f, -20.0f);
    fenceRun(28.0f, -20.0f, 28.0f, -12.0f);
    fenceRun(16.0f, 8.0f, 28.0f, 8.0f);
    fenceRun(28.0f, 6.0f, 28.0f, 8.0f);

    add("lamp_post", Place({ 27.2f, 0.0f, -13.0f }, -kHalfPi), kLamp);
    add("lamp_post", Place({ 27.2f, 0.0f, 6.0f }, -kHalfPi), kLamp);

    add("container", Place({ 22.0f, 0.0f, 5.8f }), kContainerA);
    add("container", Place({ 21.6f, 2.5f, 5.8f }), kContainerB);
    add("oil_drums", Place({ 17.6f, 0.0f, -18.6f }), kDrums);
    add("oil_drums", Place({ 19.4f, 0.0f, -18.8f }, 1.2f), kBlocks[0]);
    add("pallet", Place({ 23.0f, 0.0f, -18.4f }, 0.3f), kPallet);
    add("pallet", Place({ 23.0f, 0.14f, -18.4f }, 0.6f), kPallet);
    add("pallet", Place({ 25.0f, 0.0f, -18.0f }, -0.2f), kPallet);

    // ---- Across the road.
    const Vec3 blocks[] = { { 47.0f, 0.0f, -30.0f }, { 47.0f, 0.0f, -12.0f }, { 47.0f, 0.0f, 8.0f }, { 47.0f, 0.0f, 28.0f } };
    int        blockIndex = 0;
    for (const Vec3& at : blocks)
    {
        add("neighbour_block", Mat4::Translation(at), kBlocks[blockIndex++ % std::size(kBlocks)]);
        add("neighbour_windows", Mat4::Translation(at), kWindows);
    }

    // ---- The dock.
    const Vec3 dock = DockPosition(world);
    add("dock_frame", Place(dock, kDockFrameYaw), kDockFrame);
    add("dock_bumpers", Place(dock, kDockFrameYaw), kBumper);
    add("dock_paint", Place(dock + Vec3{ 0.0f, 0.003f, 0.0f }, kDockFrameYaw), kHazard);

    // ---- Inside.
    for (const WarehouseRack& rack : kWarehouseRacks)
    {
        const Vec3 at = GetRackPosition(world, rack);
        add("rack_frame", Mat4::Translation(at), kRackFrame);
        add("rack_beams", Mat4::Translation(at), kRackBeams);
    }
    for (const WarehouseRack& rack : kStockRacksAlongX)
    {
        const Vec3 at = world.TileCenter({ WorldLevel::Ground, rack.x, rack.z }) + Vec3{ 0.5f * kTileSize, 0.0f, rack.z == kWarehouseZ0 ? -0.35f : 0.0f };
        add("rack_frame", Mat4::Translation(at), kRackFrame);
        add("rack_beams", Mat4::Translation(at), kRackBeams);
        add("rack_cartons", Mat4::Translation(at), kCarton);
    }
    for (const WarehouseRack& rack : kStockRacksAlongZ)
    {
        const Vec3 at = world.TileCenter({ WorldLevel::Ground, rack.x, rack.z }) + Vec3{ -0.35f, 0.0f, 0.5f * kTileSize };
        add("rack_frame", Place(at, kHalfPi), kRackFrame);
        add("rack_beams", Place(at, kHalfPi), kRackBeams);
        add("rack_cartons", Place(at, kHalfPi), kCartonDark);
    }
    for (const TileCoord& tile : kOverflowTiles)
        add("pallet", Mat4::TRS(world.TileCenter(tile), Mat4::Identity(), { 1.45f, 1.0f, 1.45f }), kPallet);
    int goodsIndex = 0;
    for (const TileCoord& tile : kGoodsTiles)
    {
        const float yaw = 0.15f * (float)((goodsIndex * 7) % 5 - 2);
        add("pallet", Place(world.TileCenter(tile), yaw), kPallet);
        add("goods_stack", Place(world.TileCenter(tile), yaw), goodsIndex % 2 ? kCarton : kCartonDark);
        ++goodsIndex;
    }

    const Vec3 forklift = world.TileCenter(kForkliftTiles[0]) + Vec3{ 0.5f * kTileSize, 0.0f, 0.0f };
    add("forklift_body", Place(forklift, -kHalfPi), kForklift);
    add("forklift_dark", Place(forklift, -kHalfPi), kDark);

    const Vec3 desk = world.TileCenter(kDeskTile) + Vec3{ 0.5f * kTileSize, 0.0f, 0.0f };
    add("office_desk", Place(desk), kDesk);
    add("office_things", Place(desk), kDark);
    add("office_cabinet", Place(world.TileCenter(kCabinetTile) + Vec3{ 0.0f, 0.0f, 0.55f }), kCabinet);

    for (const Stair& stair : world.GetStairs())
        add("stair_rail", Place(world.TileCenter(stair.upper) + Vec3{ 0.0f, 0.0f, 0.5f * kTileSize }), kRail);
}

void WarehouseScene::Update(const SceneFrame& frame)
{
    DrawWarehousePanel(frame.ui, m_state);
    m_view.Update(frame);
}

void WarehouseScene::DrawTallWalls(MeshRenderer::Scope& meshes) const
{
    // A warehouse is taller than the walls the grid draws: raise the ones whose outside isn't
    // facing the camera, so the near ones never hide the floor.
    const Vec3  forward = m_view.GetCamera().GetCamera().GetForward();
    const float flat    = std::max(std::sqrt(forward.x * forward.x + forward.z * forward.z), 1e-4f);
    const Vec3  look    = { forward.x / flat, 0.0f, forward.z / flat };

    const WorldGrid& world = m_state.world;
    const float      half  = 0.5f * kTileSize;

    const auto raise = [&](const TileCoord& tile, EdgeSide side, Vec3 outward, float yaw) {
        if (Dot(outward, look) < kCutawayFacing)
            return;
        for (const TileCoord& door : kDockDoorTiles)
            if (side == EdgeSide::PosX && tile == door)
                return;
        if (world.GetEdge(tile, side) != EdgeKind::Wall)
            return;
        const Mat4 at = Place(world.TileCenter(tile) + outward * half, yaw);
        meshes.Draw(*m_wallUpper, at, kWallUpper);
        meshes.Draw(*m_wallTrim, at, kWallTrim);
    };
    for (int x = kWarehouseX0; x <= kWarehouseX1; ++x)
    {
        raise({ WorldLevel::Ground, x, kWarehouseZ0 }, EdgeSide::NegZ, { 0.0f, 0.0f, -1.0f }, 0.0f);
        raise({ WorldLevel::Ground, x, kWarehouseZ1 }, EdgeSide::PosZ, { 0.0f, 0.0f, 1.0f }, 0.0f);
    }
    for (int z = kWarehouseZ0; z <= kWarehouseZ1; ++z)
    {
        raise({ WorldLevel::Ground, kWarehouseX0, z }, EdgeSide::NegX, { -1.0f, 0.0f, 0.0f }, kHalfPi);
        raise({ WorldLevel::Ground, kWarehouseX1, z }, EdgeSide::PosX, { 1.0f, 0.0f, 0.0f }, kHalfPi);
    }
}

void WarehouseScene::DrawDockDoor(MeshRenderer::Scope& meshes) const
{
    // Rolls up: squeezed toward the top of the opening.
    const float squeeze = 1.0f + (kShutterRolled - 1.0f) * m_state.warehouse.openAmount;
    const Mat4  at = Mat4::Translation(DockPosition(m_state.world) + Vec3{ 0.0f, kShutterTop, 0.0f }) * Mat4::RotationY(kDockFrameYaw) *
                     Mat4::Scale({ 1.0f, squeeze, 1.0f }) * Mat4::Translation({ 0.0f, -kShutterTop, 0.0f });
    meshes.Draw(*m_shutter, at, kShutter);
}

void WarehouseScene::DrawTruck(MeshRenderer::Scope& meshes) const
{
    const WarehouseSim& sim = m_state.warehouse;
    if (sim.phase == TruckPhase::Gone || sim.phase == TruckPhase::OnItsWay)
        return;

    const Mat4 truck = Place(sim.truckPosition, sim.truckYaw);
    meshes.Draw(*m_truckChassis, truck, kTruckChassis);
    meshes.Draw(*m_truckCab, truck, kTruckCab);
    meshes.Draw(*m_truckGlass, truck, kTruckGlass);
    meshes.Draw(*m_truckBed, truck, kTruckBed);
    meshes.Draw(*m_truckWheels, truck, kTruckWheels);
    meshes.Draw(*m_truckLights, truck, kTruckLights, 0.7f);

    // Opening up: the canvas slides to the cab, the tail gates swing out to the sides.
    const float squeeze = 1.0f + (kTarpRolled - 1.0f) * sim.openAmount;
    meshes.Draw(*m_truckTarp,
                truck * Mat4::Translation({ 0.0f, 0.0f, kBedFrontZ }) * Mat4::Scale({ 1.0f, 1.0f, squeeze }) * Mat4::Translation({ 0.0f, 0.0f, -kBedFrontZ }),
                kTruckTarp);
    const float angle = kGateOpenAngle * sim.openAmount;
    meshes.Draw(*m_truckGate, truck * Place({ -kGateHingeX, kBedFloorY, kBedRearZ }, angle), kTruckBed);
    meshes.Draw(*m_truckGate, truck * Place({ kGateHingeX, kBedFloorY, kBedRearZ }, kPi - angle), kTruckBed);

    // The load, first in nearest the cab. Hidden by the canvas until it opens, except from the back.
    for (size_t i = 0; i < sim.cargo.size(); ++i)
    {
        const float row = (float)(i / 2);
        const float x   = (i % 2) ? kCargoColumnX : -kCargoColumnX;
        meshes.Draw(*m_crate, truck * Mat4::TRS({ x, kBedFloorY, kBedFrontZ - 0.55f - row * kCargoRowSpacing }, Mat4::Identity(), { kCargoScale, 1.0f, kCargoScale }), kCrateColors[sim.cargo[i] % std::size(kCrateColors)]);
    }
}

void WarehouseScene::DrawCrates(MeshRenderer::Scope& meshes) const
{
    const WarehouseSim& sim = m_state.warehouse;
    for (const StoredCrate& crate : sim.stored)
        meshes.Draw(*m_crate, Mat4::Translation(crate.position), kCrateColors[crate.kind % (int)std::size(kCrateColors)]);

    for (const CrateCarrier& carrier : sim.carriers)
    {
        if (carrier.kind < 0)
            continue;
        const CrewMember* member = m_state.FindCrew(carrier.crewId);
        if (!member)
            continue;
        const float yaw = member->motion.yaw;
        const Vec3  at  = member->motion.position + Vec3{ -std::sin(yaw) * kCarryForward, kCarryHeight, -std::cos(yaw) * kCarryForward };
        meshes.Draw(*m_crate, Mat4::TRS(at, Mat4::RotationY(yaw), { kCarryScale, kCarryScale, kCarryScale }), kCrateColors[carrier.kind % (int)std::size(kCrateColors)]);
    }
}

void WarehouseScene::Draw(const SceneRenderers& renderers) const
{
    m_view.Draw(renderers, [this](MeshRenderer::Scope& meshes) {
        for (const Prop& prop : m_props)
            meshes.Draw(*prop.mesh, prop.world, prop.color, prop.emissive);
        DrawTallWalls(meshes);
        DrawDockDoor(meshes);
        DrawTruck(meshes);
        DrawCrates(meshes);
    });
}

#if defined(GAME_IMGUI_ENABLED)
void WarehouseScene::RegisterCheats(DebugPad& pad)
{
    m_debugPad = &pad;
    pad.RegisterCategory("Warehouse camera", [this]() { m_view.GetCamera().DrawCheats(); });
}
#endif
