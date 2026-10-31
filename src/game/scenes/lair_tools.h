#pragma once

#include "../scene.h"
#include "../ui/ui_painter.h"
#include "lair_view.h"

#include <string>
#include <vector>

// The player's hands in the lair: the build menu along the bottom and what the mouse then does on
// the world. Mouse first: drag to mark, click to place, right click cancels what's under the
// cursor (or else the tool), R turns an object, Esc drops the tool.
class LairTools
{
public:
    enum class Tool
    {
        None,
        Dig,
        Room,
        Door,
        Object,
        Delete,
    };

    LairTools(const SceneServices& services, const LairView& view);

    // Floating labels and progress bars over the world, then the menu. Call before the camera's
    // update: this is what decides whether the mouse is on the UI.
    void BuildUi(Ui& ui, const OrbitCamera& camera);

    // The mouse on the world and the previews. Call after the camera's update.
    void UpdateWorld(const SceneFrame& frame, const OrbitCamera& camera);

    // Previews, over everything, in a translucent scope.
    void DrawGhosts(MeshRenderer::Scope& translucent) const;

    void CancelDrag() { m_dragging = false; }

private:
    void BuildLabels(Ui& ui, const OrbitCamera& camera);
    void BuildMenu(Ui& ui);
    bool MenuButton(Ui& ui, const UiRect& r, const char* icon, const std::string& title, const std::string& detail, const std::string& tooltip, bool selected);
    void Select(Tool tool);

    void UpdateDig(const SceneFrame& frame, const TileCoord& tile, bool onWorld);
    void UpdateRoom(const SceneFrame& frame, const TileCoord& tile, bool onWorld);
    void UpdateDoor(const SceneFrame& frame, const OrbitCamera& camera, bool onWorld);
    void UpdateObject(const SceneFrame& frame, const TileCoord& tile, bool onWorld);
    void UpdateDelete(const SceneFrame& frame, const OrbitCamera& camera, bool onWorld);
    void UpdateNone(const SceneFrame& frame, const TileCoord& tile, bool onWorld);

    // The wall nearest under the cursor, built or planned. False if nothing's close.
    bool PickWall(const OrbitCamera& camera, LairEdge& edge) const;
    const LairDoor* PickDoor(const OrbitCamera& camera) const;
    const LairObject* PickObject(const OrbitCamera& camera) const;
    void AddRoomBorder(const LairRect& rect, const Color& color);
    void Refuse(const std::string& reason);

    struct TileGhost
    {
        TileCoord tile;
        Color     color;
    };
    struct WallGhost
    {
        LairEdge edge;
        Color    color;
    };

    GameState&      m_state;
    const IWindow&  m_window;
    const LairView& m_view;

    Tool m_tool = Tool::None;
    int  m_roomType = 0;
    int  m_objectType = 0;
    int  m_rotation = 0;
    bool m_roomMenu = false;
    bool m_objectMenu = false;

    bool      m_dragging = false;
    TileCoord m_dragStart;
    TileCoord m_dragEnd; // where the cursor last was on the world

    // What the previews show this frame.
    std::vector<TileGhost> m_tileGhosts;
    std::vector<WallGhost> m_wallGhosts;
    bool      m_hasDoorGhost = false;
    LairEdge  m_doorGhost;
    Color     m_doorGhostColor;
    bool      m_hasObjectGhost = false;
    TileCoord m_objectGhostTile;
    Color     m_objectGhostColor;
    int       m_objectGhostType = 0;
    int       m_objectGhostRotation = 0;
};
