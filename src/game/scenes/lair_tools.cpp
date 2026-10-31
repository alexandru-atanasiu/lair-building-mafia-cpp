#include "lair_tools.h"

#include "../camera/orbit_camera.h"
#include "../ui/ui.h"
#include "platform/window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr WorldLevel kLevel = WorldLevel::Underground;

    constexpr float kBarHeight   = 64.0f;
    constexpr float kToolWidth   = 112.0f;
    constexpr float kItemWidth   = 146.0f;
    constexpr float kItemHeight  = 50.0f;
    constexpr float kGap         = 5.0f;
    constexpr float kWallPickRange = 1.3f; // metres from the cursor on the floor
    constexpr float kDoorPickRange = 0.8f;

    const Color kValid       = { 0.25f, 0.95f, 0.35f, 0.42f };
    const Color kInvalid     = { 0.95f, 0.18f, 0.12f, 0.45f };
    const Color kCancel      = { 0.95f, 0.60f, 0.15f, 0.45f };
    const Color kDeleteHover = { 0.95f, 0.30f, 0.15f, 0.50f };
    const Color kWorksColor  = { 0.60f, 0.92f, 0.50f, 1.0f };
    const Color kProblemColor = { 0.98f, 0.76f, 0.30f, 1.0f };
    const Color kDimText     = { 0.72f, 0.68f, 0.58f, 1.0f };
    const Color kLabelBack   = { 0.05f, 0.05f, 0.05f, 0.72f };
    constexpr float kPersonHeight  = 1.8f;
    constexpr float kPersonHalfWidth = 0.3f; // of his height on screen
    constexpr float kCoveringAlpha = 0.18f;

    std::string Money(int amount)
    {
        char text[32];
        snprintf(text, sizeof(text), "$%d", amount);
        return text;
    }

    LairRect MakeRect(const TileCoord& a, const TileCoord& b)
    {
        return { std::min(a.x, b.x), std::min(a.z, b.z), std::max(a.x, b.x), std::max(a.z, b.z) };
    }

    template <typename F>
    void ForEachBorder(const LairRect& r, F&& f)
    {
        for (int x = r.x0; x <= r.x1; ++x)
        {
            f(MakeLairEdge({ kLevel, x, r.z0 }, EdgeSide::NegZ));
            f(MakeLairEdge({ kLevel, x, r.z1 }, EdgeSide::PosZ));
        }
        for (int z = r.z0; z <= r.z1; ++z)
        {
            f(MakeLairEdge({ kLevel, r.x0, z }, EdgeSide::NegX));
            f(MakeLairEdge({ kLevel, r.x1, z }, EdgeSide::PosX));
        }
    }

    Vec2 CursorOf(const IWindow& window)
    {
        return { (float)window.GetMouseX(), (float)window.GetMouseY() };
    }
}

LairTools::LairTools(const SceneServices& services, const LairView& view)
    : m_state(services.state)
    , m_window(services.window)
    , m_view(view)
{
}

void LairTools::Select(Tool tool)
{
    m_tool       = tool;
    m_dragging   = false;
    m_roomMenu   = tool == Tool::Room;
    m_objectMenu = tool == Tool::Object;
}

void LairTools::Refuse(const std::string& reason)
{
    m_state.PostEvent(GameEventKind::Bad, reason);
}

// ---- The UI ----

void LairTools::BuildUi(Ui& ui, const OrbitCamera& camera)
{
    BuildLabels(ui, camera);
    BuildMenu(ui);
}

void LairTools::BuildLabels(Ui& ui, const OrbitCamera& camera)
{
    const float scale = ui.GetScale();
    const float bottom = ui.GetHeight() - kBarHeight;

    const auto toUi = [&](Vec3 world, Vec2& at) {
        Vec2 screen;
        if (!camera.WorldToScreen(world, screen))
            return false;
        at = { screen.x / scale, screen.y / scale };
        return at.x > 0.0f && at.y > ui.GetContentTop() && at.x < ui.GetWidth() && at.y < bottom;
    };

    // Progress on whatever is being worked on.
    for (const LairJob& job : m_state.lair.jobs)
    {
        if (job.progress <= 0.0f)
            continue;
        Vec3 where;
        switch (job.kind)
        {
        case LairJobKind::Dig:
            where = m_state.world.TileCenter(job.tile) + Vec3{ 0.0f, m_view.GetSurfaceHeight(job.tile) + 0.2f, 0.0f };
            break;
        case LairJobKind::Room:
            if (const LairRoom* room = FindLairRoom(m_state, (uint16_t)job.target))
                where = m_state.world.TileCenter({ kLevel, (room->rect.x0 + room->rect.x1) / 2, (room->rect.z0 + room->rect.z1) / 2 }) + Vec3{ 0.0f, 0.6f, 0.0f };
            break;
        case LairJobKind::Door:
            for (const LairDoor& door : m_state.lair.doors)
                if (door.id == job.target)
                    where = GetLairEdgeCenter(m_state, door.edge) + Vec3{ 0.0f, 1.2f, 0.0f };
            break;
        case LairJobKind::Object:
            for (const LairObject& object : m_state.lair.objects)
                if (object.id == job.target)
                    where = GetLairObjectCenter(m_state, GetLairObjectTypes()[(size_t)object.type], object.tile, object.rotation) + Vec3{ 0.0f, 1.8f, 0.0f };
            break;
        }
        Vec2 at;
        if (toUi(where, at))
            ui.ProgressBar({ at.x - 22.0f, at.y - 4.0f, 44.0f, 8.0f }, job.progress / job.duration);
    }

    // Each room's type, and whether it works.
    for (const LairRoom& room : m_state.lair.rooms)
    {
        const LairRoomType&  type   = GetLairRoomTypes()[(size_t)room.type];
        const LairRoomStatus status = GetLairRoomStatus(m_state, room);
        // On the wall nearest the camera (the middle of a side lowest on screen), where it hides
        // nobody in the room.
        const Vec3  low  = m_state.world.TileCenter({ kLevel, room.rect.x0, room.rect.z0 }) - Vec3{ 0.5f * kTileSize, 0.0f, 0.5f * kTileSize };
        const Vec3  high = m_state.world.TileCenter({ kLevel, room.rect.x1, room.rect.z1 }) + Vec3{ 0.5f * kTileSize, 0.0f, 0.5f * kTileSize };
        const float midX = 0.5f * (low.x + high.x), midZ = 0.5f * (low.z + high.z);
        const Vec3  sides[4] = { { midX, low.y, low.z }, { midX, low.y, high.z }, { low.x, low.y, midZ }, { high.x, low.y, midZ } };
        Vec2 at;
        bool shown = false;
        for (const Vec3& side : sides)
        {
            Vec2 candidate;
            if (toUi(side, candidate) && (!shown || candidate.y > at.y))
            {
                at    = candidate;
                shown = true;
            }
        }
        if (!shown)
            continue;

        const std::string line = status.working ? "Works" : status.problem;
        const float width = std::max(ui.MeasureText(type.name, 15.0f, true), ui.MeasureText(line, 13.0f)) + 14.0f;
        const float height = ui.GetLineHeight(15.0f) + ui.GetLineHeight(13.0f) + 6.0f;
        const UiRect back = { at.x - 0.5f * width, at.y - 0.5f * height, width, height };

        // Nearly gone while it would cover someone: people matter more than the sign.
        bool coversSomeone = false;
        for (const CrewMember& member : m_state.crew)
        {
            if (member.away || member.motion.level != kLevel)
                continue;
            Vec2 feet, head;
            if (!camera.WorldToScreen(member.motion.position, feet) || !camera.WorldToScreen(member.motion.position + Vec3{ 0.0f, kPersonHeight, 0.0f }, head))
                continue;
            // The box he fills on screen, in UI pixels.
            const float size   = Length(Vec3{ head.x - feet.x, head.y - feet.y, 0.0f }) / scale;
            const float left   = std::min(feet.x, head.x) / scale - kPersonHalfWidth * size;
            const float right  = std::max(feet.x, head.x) / scale + kPersonHalfWidth * size;
            const float top    = std::min(feet.y, head.y) / scale;
            const float lowest = std::max(feet.y, head.y) / scale;
            coversSomeone = coversSomeone || (left < back.x + back.w && right > back.x && top < back.y + back.h && lowest > back.y);
        }
        const float alpha = coversSomeone ? kCoveringAlpha : 1.0f;

        Color backColor = kLabelBack;
        backColor.a *= alpha;
        ui.Fill(back, backColor);

        Ui::TextStyle name;
        name.size  = 15.0f;
        name.bold  = true;
        name.align = Ui::Align::Center;
        name.color.a *= alpha;
        ui.Label({ at.x, back.y + 2.0f }, type.name, name);

        Ui::TextStyle detail;
        detail.size  = 13.0f;
        detail.color = status.working ? kWorksColor : kProblemColor;
        detail.align = Ui::Align::Center;
        detail.color.a *= alpha;
        ui.Label({ at.x, back.y + 3.0f + ui.GetLineHeight(15.0f) }, line, detail);
    }
}

bool LairTools::MenuButton(Ui& ui, const UiRect& r, const char* icon, const std::string& title, const std::string& detail, const std::string& tooltip, bool selected)
{
    Ui::ButtonStyle style;
    style.selected = selected;
    const bool clicked = ui.Button(r, "", style);

    const float iconSize = std::min(r.h - 12.0f, 36.0f);
    ui.Image({ r.x + 6.0f, r.y + 0.5f * (r.h - iconSize), iconSize, iconSize }, ui.Texture(icon));

    const float textX = r.x + iconSize + 12.0f;
    Ui::TextStyle name;
    name.size = 15.0f;
    name.bold = selected;
    ui.Label({ textX, r.y + 0.5f * r.h - ui.GetLineHeight(15.0f) + 1.0f }, title, name);
    Ui::TextStyle small;
    small.size  = 13.0f;
    small.color = kDimText;
    ui.Label({ textX, r.y + 0.5f * r.h + 1.0f }, detail, small);

    if (ui.IsHovered(r) && !tooltip.empty())
        ui.Tooltip(ui.WrapText(tooltip, 320.0f, 14.0f));
    return clicked;
}

void LairTools::BuildMenu(Ui& ui)
{
    const float width  = ui.GetWidth();
    const float top    = ui.GetHeight() - kBarHeight;

    // The list of rooms or objects, above the bar, wrapped to the window.
    if (m_roomMenu || m_objectMenu)
    {
        const int   count   = m_roomMenu ? (int)GetLairRoomTypes().size() : (int)GetLairObjectTypes().size();
        const int   columns = std::max(1, (int)((width - 20.0f) / (kItemWidth + kGap)));
        const int   rows    = (count + columns - 1) / columns;
        const float height  = (float)rows * (kItemHeight + kGap) + kGap;
        const float panelW  = (float)std::min(columns, count) * (kItemWidth + kGap) + kGap;
        ui.Panel({ 8.0f, top - height - 6.0f, panelW, height });

        for (int i = 0; i < count; ++i)
        {
            const UiRect r = { 8.0f + kGap + (float)(i % columns) * (kItemWidth + kGap),
                               top - height - 6.0f + kGap + (float)(i / columns) * (kItemHeight + kGap), kItemWidth, kItemHeight };
            if (m_roomMenu)
            {
                const LairRoomType& type = GetLairRoomTypes()[(size_t)i];
                char detail[48];
                snprintf(detail, sizeof(detail), "$%d/tile, %dx%d", type.costPerTile, type.minShort, type.minLong);
                std::string tooltip = type.description;
                tooltip += "\nNeeds:";
                for (const LairRequirement& need : type.required)
                    if (need.object)
                        for (const LairObjectType& object : GetLairObjectTypes())
                            if (std::string_view(object.key) == need.object)
                                tooltip += std::string(" ") + object.name + ",";
                tooltip += " a door.";
                if (MenuButton(ui, r, type.icon, type.name, detail, tooltip, m_tool == Tool::Room && m_roomType == i))
                {
                    m_tool     = Tool::Room;
                    m_roomType = i;
                    m_dragging = false;
                }
            }
            else
            {
                const LairObjectType& type = GetLairObjectTypes()[(size_t)i];
                const char*           room = type.room;
                for (const LairRoomType& roomType : GetLairRoomTypes())
                    if (std::string_view(roomType.key) == type.room)
                        room = roomType.name;
                const std::string detail  = Money(type.cost) + std::string("  ") + room;
                const std::string tooltip = std::string(type.description) + "\nGoes in: " + room + ". R turns it.";
                if (MenuButton(ui, r, type.icon, type.name, detail, tooltip, m_tool == Tool::Object && m_objectType == i))
                {
                    m_tool       = Tool::Object;
                    m_objectType = i;
                    m_dragging   = false;
                    // Things show their front to the default camera; seats their back, to face what's in front of them.
                    m_rotation   = type.seat ? 0 : 2;
                }
            }
        }
    }

    // The bar.
    Ui::PanelStyle bar;
    bar.fill = { 0.08f, 0.07f, 0.06f, 0.95f };
    ui.Panel({ -2.0f, top, width + 4.0f, kBarHeight + 2.0f }, bar);

    struct ToolEntry
    {
        Tool        tool;
        const char* icon;
        const char* title;
        std::string detail;
        const char* tooltip;
    };
    const ToolEntry entries[] = {
        { Tool::Dig, "ui/icons/tool_dig.tex", "Dig", Money(kLairDigCost) + "/tile", "Drag over earth to mark it. Free crew dig it out. Right click cancels a mark." },
        { Tool::Room, "ui/icons/tool_rooms.tex", "Rooms", "per tile", "Pick a type, then drag a rectangle over dug floor. Walls go up around it." },
        { Tool::Door, "ui/icons/tool_door.tex", "Doors", Money(kLairDoorCost), "Click a wall. A room needs a door to work." },
        { Tool::Object, "ui/icons/tool_objects.tex", "Objects", "by room", "Pick an object, then click in a room of its type. R turns it." },
        { Tool::Delete, "ui/icons/tool_delete.tex", "Delete", "50% back", "Click an object, a door or a room to remove it. Not yet built: everything back." },
    };

    float x = 8.0f;
    for (const ToolEntry& entry : entries)
    {
        const UiRect r = { x, top + 8.0f, kToolWidth, kBarHeight - 16.0f };
        const bool selected = m_tool == entry.tool || (entry.tool == Tool::Room && m_roomMenu) || (entry.tool == Tool::Object && m_objectMenu);
        if (MenuButton(ui, r, entry.icon, entry.title, entry.detail, entry.tooltip, selected))
        {
            Select(selected ? Tool::None : entry.tool); // pressed again: put it away
        }
        x += kToolWidth + kGap;
    }

    // What the lair adds up to, at the right.
    char stats[96];
    snprintf(stats, sizeof(stats), "Crew %d / %d", (int)m_state.crew.size(), CrewCapacity(m_state));
    char stats2[96];
    snprintf(stats2, sizeof(stats2), "Crates +%d   Safe $%lld", ExtraCrateCapacity(m_state), (long long)ProtectedMoney(m_state));
    Ui::TextStyle line;
    line.size  = 14.0f;
    line.align = Ui::Align::Right;
    ui.Label({ width - 12.0f, top + 12.0f }, stats, line);
    line.color = kDimText;
    ui.Label({ width - 12.0f, top + 32.0f }, stats2, line);

    // A reminder of what the mouse does now.
    const char* hint = nullptr;
    switch (m_tool)
    {
    case Tool::None:   hint = "Pick a tool. Wheel zooms, WASD or middle-drag pans, right-drag or Q/E turns."; break;
    case Tool::Dig:    hint = "Drag to mark earth. Ctrl-drag or right click to cancel marks."; break;
    case Tool::Room:   hint = "Drag a rectangle over dug floor. Right click cancels."; break;
    case Tool::Door:   hint = "Click a wall. Right click cancels."; break;
    case Tool::Object: hint = "Click in a room of its type. R turns it. Right click cancels."; break;
    case Tool::Delete: hint = "Click an object, a door or a room. Right click cancels."; break;
    }
    const float hintX = x + 6.0f;
    const float hintW = width - hintX - 190.0f;
    if (hintW > 60.0f)
    {
        Ui::TextStyle text;
        text.size  = 14.0f;
        text.color = kDimText;
        const std::string wrapped = ui.WrapText(hint, hintW, 14.0f);
        const long long   lines   = 1 + std::count(wrapped.begin(), wrapped.end(), '\n');
        ui.Label({ hintX, top + 0.5f * (kBarHeight - ui.GetLineHeight(14.0f) * (float)lines) }, wrapped, text);
    }
}

// ---- The mouse on the world ----

bool LairTools::PickWall(const OrbitCamera& camera, LairEdge& edge) const
{
    Vec3 origin, direction;
    camera.ScreenRay(CursorOf(m_window), origin, direction);
    if (direction.y >= -1e-5f)
        return false;

    const auto rayAt = [&](float y) { return origin + direction * ((kUndergroundFloorY + y - origin.y) / direction.y); };

    const auto isWall = [&](const LairEdge& e) {
        if (m_state.world.GetEdge(e.tile, e.side) != EdgeKind::None)
            return true;
        TileCoord a, b;
        GetLairEdgeTiles(e, a, b);
        const Tile& ta = m_state.world.GetTile(a);
        const Tile& tb = m_state.world.GetTile(b);
        return ta.room != tb.room; // a wall in the queue
    };

    // Down the ray: a wall the ray passes through at its shown height.
    for (float y = kWallHeight; y >= 0.0f; y -= 0.1f)
    {
        const Vec3      p = rayAt(y);
        const TileCoord c = m_state.world.WorldToTile(kLevel, p);
        const Vec3      center = m_state.world.TileCenter(c);
        const float     dx = p.x - center.x, dz = p.z - center.z;
        LairEdge candidate;
        if (std::abs(dx) > 0.75f)
            candidate = MakeLairEdge(c, dx > 0.0f ? EdgeSide::PosX : EdgeSide::NegX);
        else if (std::abs(dz) > 0.75f)
            candidate = MakeLairEdge(c, dz > 0.0f ? EdgeSide::PosZ : EdgeSide::NegZ);
        else
            continue;
        if (isWall(candidate) && m_view.GetEdgeHeight(candidate) >= y)
        {
            edge = candidate;
            return true;
        }
    }

    // Otherwise the nearest wall to the point on the floor.
    const Vec3      floor = rayAt(0.0f);
    const TileCoord tile  = m_state.world.WorldToTile(kLevel, floor);
    float           best  = kWallPickRange;
    bool            found = false;
    for (int z = tile.z - 1; z <= tile.z + 1; ++z)
    {
        for (int x = tile.x - 1; x <= tile.x + 1; ++x)
        {
            for (const EdgeSide side : { EdgeSide::PosX, EdgeSide::PosZ })
            {
                const LairEdge candidate = { { kLevel, x, z }, side };
                if (!m_state.world.InBounds(candidate.tile) || !isWall(candidate))
                    continue;
                const Vec3  center = GetLairEdgeCenter(m_state, candidate);
                const float distance = Length(Vec3{ center.x - floor.x, 0.0f, center.z - floor.z });
                if (distance < best)
                {
                    best  = distance;
                    edge  = candidate;
                    found = true;
                }
            }
        }
    }
    return found;
}

const LairDoor* LairTools::PickDoor(const OrbitCamera& camera) const
{
    LairEdge edge;
    if (!PickWall(camera, edge))
        return nullptr;
    const LairDoor* door = FindLairDoor(m_state, edge);
    if (!door)
        return nullptr;

    // Picking a wall is generous; picking a door to delete shouldn't take the room around it.
    Vec3 origin, direction;
    camera.ScreenRay(CursorOf(m_window), origin, direction);
    const float height = std::min(m_view.GetEdgeHeight(edge), 1.2f);
    const Vec3  center = GetLairEdgeCenter(m_state, edge);
    for (const float y : { 0.0f, height })
    {
        const Vec3 p = origin + direction * ((kUndergroundFloorY + y - origin.y) / direction.y);
        if (Length(Vec3{ p.x - center.x, 0.0f, p.z - center.z }) < kDoorPickRange)
            return door;
    }
    return nullptr;
}

const LairObject* LairTools::PickObject(const OrbitCamera& camera) const
{
    Vec3 origin, direction;
    camera.ScreenRay(CursorOf(m_window), origin, direction);
    if (direction.y >= -1e-5f)
        return nullptr;
    for (float y = 2.2f; y >= 0.0f; y -= 0.1f)
    {
        const Vec3      p = origin + direction * ((kUndergroundFloorY + y - origin.y) / direction.y);
        const TileCoord c = m_state.world.WorldToTile(kLevel, p);
        const LairObject* object = FindLairObjectAt(m_state, c);
        if (object && GetLairObjectTypes()[(size_t)object->type].height >= y)
            return object;
    }
    return nullptr;
}

void LairTools::AddRoomBorder(const LairRect& rect, const Color& color)
{
    ForEachBorder(rect, [&](const LairEdge& edge) {
        if (m_state.world.GetEdge(edge.tile, edge.side) == EdgeKind::None)
            m_wallGhosts.push_back({ edge, color });
    });
}

void LairTools::UpdateWorld(const SceneFrame& frame, const OrbitCamera& camera)
{
    m_tileGhosts.clear();
    m_wallGhosts.clear();
    m_hasDoorGhost   = false;
    m_hasObjectGhost = false;

    if (frame.keyboardAvailable)
    {
        if (m_window.WasKeyPressed(Key::R))
            m_rotation = (m_rotation + 1) % 4;
        if (m_window.WasKeyPressed(Key::Escape))
        {
            if (m_dragging)
                m_dragging = false;
            else
                Select(Tool::None);
        }
    }

    const bool onWorld = frame.mouseAvailable && !frame.ui.WantsMouse() && !camera.IsOrbiting();

    Vec3 origin, direction;
    camera.ScreenRay(CursorOf(m_window), origin, direction);
    TileCoord tile;
    bool      hasTile = false;
    if (direction.y < -1e-5f)
    {
        if (m_tool == Tool::Dig)
            hasTile = m_view.PickTile(origin, direction, tile);
        else
        {
            // Rooms and objects are placed on the floor, whatever stands in the way.
            tile    = m_state.world.WorldToTile(kLevel, origin + direction * ((kUndergroundFloorY - origin.y) / direction.y));
            hasTile = m_state.world.InBounds(tile);
        }
    }

    // Right click: cancels a drag, else the job under the cursor, else the tool.
    if (camera.WasRightClicked())
    {
        bool done = false;
        if (m_dragging)
        {
            m_dragging = false;
            done       = true;
        }
        if (!done)
        {
            TileCoord under;
            if (m_view.PickTile(origin, direction, under) && CancelLairJobAt(m_state, under))
                done = true;
            if (!done)
            {
                LairEdge edge;
                if (PickWall(camera, edge))
                {
                    const LairDoor* door = FindLairDoor(m_state, edge);
                    if (door && !door->built)
                    {
                        DeleteLairDoor(m_state, door->id);
                        done = true;
                    }
                }
            }
        }
        if (!done)
            Select(Tool::None);
    }

    const bool usable = hasTile && onWorld;
    switch (m_tool)
    {
    case Tool::None:   if (hasTile) UpdateNone(frame, tile, onWorld); break;
    case Tool::Dig:    UpdateDig(frame, tile, usable); break;
    case Tool::Room:   UpdateRoom(frame, tile, usable); break;
    case Tool::Door:   UpdateDoor(frame, camera, onWorld); break;
    case Tool::Object: UpdateObject(frame, tile, usable); break;
    case Tool::Delete: UpdateDelete(frame, camera, onWorld); break;
    }
}

void LairTools::UpdateNone(const SceneFrame& frame, const TileCoord& tile, bool onWorld)
{
    if (!onWorld)
        return;
    const LairRoom* room = FindLairRoomAt(m_state, tile);
    if (!room)
        return;
    const LairRoomType&  type   = GetLairRoomTypes()[(size_t)room->type];
    const LairRoomStatus status = GetLairRoomStatus(m_state, *room);
    frame.ui.Tooltip(frame.ui.WrapText(std::string(type.name) + ": " + (status.working ? "works" : status.problem) + ". " + type.description, 320.0f, 14.0f));
}

void LairTools::UpdateDig(const SceneFrame& frame, const TileCoord& tile, bool onWorld)
{
    if (!m_dragging && !onWorld)
        return;

    const bool cancelling = m_window.IsKeyDown(Key::Control);
    if (onWorld && m_window.WasMouseButtonPressed(MouseButton::Left))
    {
        m_dragging  = true;
        m_dragStart = tile;
    }
    if (onWorld)
        m_dragEnd = tile;

    const LairRect rect = m_dragging ? MakeRect(m_dragStart, m_dragEnd) : MakeRect(m_dragEnd, m_dragEnd);

    // What the rectangle would do: green marks, as far as the money goes, and red where it can't.
    int         count = 0, cost = 0;
    int64_t     budget = m_state.money;
    std::string reason;
    for (int z = rect.z0; z <= rect.z1; ++z)
    {
        for (int x = rect.x0; x <= rect.x1; ++x)
        {
            const TileCoord c = { kLevel, x, z };
            if (cancelling)
            {
                if (FindLairDigJob(m_state, c))
                {
                    m_tileGhosts.push_back({ c, kCancel });
                    ++count;
                }
                continue;
            }
            const LairCheck check = CheckLairDig(m_state, c);
            if (check.ok && budget >= check.cost)
            {
                budget -= check.cost;
                cost += check.cost;
                ++count;
                m_tileGhosts.push_back({ c, kValid });
            }
            else if (m_state.world.GetTile(c).kind == TileKind::Earth && !FindLairDigJob(m_state, c))
            {
                m_tileGhosts.push_back({ c, kInvalid });
                reason = check.ok ? "Not enough money" : check.reason;
            }
            else if (rect.x0 == rect.x1 && rect.z0 == rect.z1)
            {
                reason = FindLairDigJob(m_state, c) && !IsLairDigReachable(m_state, c) ? "Marked. Nobody can reach it yet: it waits for the tiles next to it" : check.reason;
            }
        }
    }

    if (onWorld)
    {
        if (cancelling)
        {
            if (count > 0)
                frame.ui.Tooltip("Cancel " + std::to_string(count) + " marks");
        }
        else if (count > 0)
            frame.ui.Tooltip("Dig " + std::to_string(count) + (count == 1 ? " tile: " : " tiles: ") + Money(cost) + (reason.empty() ? "" : "\n" + reason));
        else if (!reason.empty())
            frame.ui.Tooltip(reason);
    }

    if (m_dragging && !m_window.IsMouseButtonDown(MouseButton::Left))
    {
        for (int z = rect.z0; z <= rect.z1; ++z)
        {
            for (int x = rect.x0; x <= rect.x1; ++x)
            {
                const TileCoord c = { kLevel, x, z };
                if (cancelling)
                    CancelLairDig(m_state, c);
                else
                    MarkLairDig(m_state, c);
            }
        }
        m_dragging = false;
        m_tileGhosts.clear();
    }
}

void LairTools::UpdateRoom(const SceneFrame& frame, const TileCoord& tile, bool onWorld)
{
    const LairRoomType& type = GetLairRoomTypes()[(size_t)m_roomType];

    if (onWorld && m_window.WasMouseButtonPressed(MouseButton::Left))
    {
        m_dragging  = true;
        m_dragStart = tile;
    }
    if (!m_dragging && !onWorld)
        return;

    if (onWorld)
        m_dragEnd = tile;
    const LairRect  rect = m_dragging ? MakeRect(m_dragStart, m_dragEnd) : MakeRect(m_dragEnd, m_dragEnd);
    const LairCheck check = CheckLairRoom(m_state, m_roomType, rect);

    // Hovering before a drag: a single tile, which is only a hint unless it's already wrong.
    const Color color = check.ok ? kValid : kInvalid;
    for (int z = rect.z0; z <= rect.z1; ++z)
        for (int x = rect.x0; x <= rect.x1; ++x)
            m_tileGhosts.push_back({ { kLevel, x, z }, color });
    AddRoomBorder(rect, color);

    if (onWorld)
    {
        char size[32];
        snprintf(size, sizeof(size), "%d x %d", rect.Width(), rect.Depth());
        if (check.ok)
            frame.ui.Tooltip(std::string(type.name) + " " + size + ": " + Money(check.cost));
        else if (!m_dragging && rect.x0 == rect.x1)
            frame.ui.Tooltip(std::string("Drag a rectangle for the ") + type.name + "\n" + check.reason);
        else
            frame.ui.Tooltip(std::string(type.name) + " " + size + "\n" + check.reason);
    }

    if (m_dragging && !m_window.IsMouseButtonDown(MouseButton::Left))
    {
        m_dragging = false;
        const LairCheck placed = PlaceLairRoom(m_state, m_roomType, rect);
        if (!placed.ok)
            Refuse(placed.reason);
    }
}

void LairTools::UpdateDoor(const SceneFrame& frame, const OrbitCamera& camera, bool onWorld)
{
    if (!onWorld)
        return;
    LairEdge edge;
    if (!PickWall(camera, edge))
    {
        frame.ui.Tooltip("Doors go on a wall");
        return;
    }
    const LairCheck check = CheckLairDoor(m_state, edge);
    m_hasDoorGhost   = true;
    m_doorGhost      = edge;
    m_doorGhostColor = check.ok ? kValid : kInvalid;
    frame.ui.Tooltip(check.ok ? "Door: " + Money(check.cost) : check.reason);

    if (m_window.WasMouseButtonPressed(MouseButton::Left))
    {
        const LairCheck placed = PlaceLairDoor(m_state, edge);
        if (!placed.ok)
            Refuse(placed.reason);
    }
}

void LairTools::UpdateObject(const SceneFrame& frame, const TileCoord& tile, bool onWorld)
{
    if (!onWorld)
        return;
    const LairObjectType& type = GetLairObjectTypes()[(size_t)m_objectType];
    const LairCheck       check = CheckLairObject(m_state, m_objectType, tile, m_rotation);

    m_hasObjectGhost      = true;
    m_objectGhostType     = m_objectType;
    m_objectGhostTile     = tile;
    m_objectGhostRotation = m_rotation;
    m_objectGhostColor    = check.ok ? kValid : kInvalid;
    for (const TileCoord& c : GetLairObjectTiles(type, tile, m_rotation))
        m_tileGhosts.push_back({ c, check.ok ? kValid : kInvalid });
    frame.ui.Tooltip(check.ok ? std::string(type.name) + ": " + Money(check.cost) : check.reason);

    if (m_window.WasMouseButtonPressed(MouseButton::Left))
    {
        const LairCheck placed = PlaceLairObject(m_state, m_objectType, tile, m_rotation);
        if (!placed.ok)
            Refuse(placed.reason);
    }
}

void LairTools::UpdateDelete(const SceneFrame& frame, const OrbitCamera& camera, bool onWorld)
{
    if (!onWorld)
        return;

    // The door under the cursor, or the object, or the room.
    const bool click = m_window.WasMouseButtonPressed(MouseButton::Left);

    if (const LairDoor* door = PickDoor(camera))
    {
        const LairCheck check = CheckDeleteLairDoor(m_state, *door);
        m_hasDoorGhost   = true;
        m_doorGhost      = door->edge;
        m_doorGhostColor = check.ok ? kDeleteHover : kInvalid;
        frame.ui.Tooltip(check.ok ? "Remove door: " + Money(check.cost) + " back" : "Can't remove this door\n" + check.reason);
        if (click)
        {
            const LairCheck done = DeleteLairDoor(m_state, door->id);
            if (!done.ok)
                Refuse(done.reason);
        }
        return;
    }

    if (const LairObject* object = PickObject(camera))
    {
        const LairObjectType& type  = GetLairObjectTypes()[(size_t)object->type];
        const LairCheck       check = CheckDeleteLairObject(m_state, *object);
        m_hasObjectGhost      = true;
        m_objectGhostType     = object->type;
        m_objectGhostTile     = object->tile;
        m_objectGhostRotation = object->rotation;
        m_objectGhostColor    = kDeleteHover;
        for (const TileCoord& c : GetLairObjectTiles(type, object->tile, object->rotation))
            m_tileGhosts.push_back({ c, kDeleteHover });
        frame.ui.Tooltip(std::string("Remove ") + type.name + ": " + Money(check.cost) + " back");
        if (click)
            DeleteLairObject(m_state, object->id);
        return;
    }

    Vec3 origin, direction;
    camera.ScreenRay(CursorOf(m_window), origin, direction);
    if (direction.y >= -1e-5f)
        return;
    const TileCoord tile = m_state.world.WorldToTile(kLevel, origin + direction * ((kUndergroundFloorY - origin.y) / direction.y));
    if (const LairRoom* room = FindLairRoomAt(m_state, tile))
    {
        const LairCheck check = CheckDeleteLairRoom(m_state, *room);
        for (int z = room->rect.z0; z <= room->rect.z1; ++z)
            for (int x = room->rect.x0; x <= room->rect.x1; ++x)
                m_tileGhosts.push_back({ { kLevel, x, z }, kDeleteHover });
        ForEachBorder(room->rect, [&](const LairEdge& edge) {
            if (m_state.world.GetEdge(edge.tile, edge.side) != EdgeKind::None)
                m_wallGhosts.push_back({ edge, kDeleteHover });
        });
        frame.ui.Tooltip(std::string("Remove ") + GetLairRoomTypes()[(size_t)room->type].name + ", its doors and objects: " + Money(check.cost) + " back");
        if (click)
            DeleteLairRoom(m_state, room->id);
        return;
    }
    frame.ui.Tooltip("Nothing to remove here");
}

void LairTools::DrawGhosts(MeshRenderer::Scope& translucent) const
{
    for (const TileGhost& ghost : m_tileGhosts)
        m_view.DrawTileOverlay(translucent, ghost.tile, ghost.color);
    for (const WallGhost& ghost : m_wallGhosts)
        m_view.DrawWallGhost(translucent, ghost.edge, std::max(m_view.GetEdgeHeight(ghost.edge), 0.6f), ghost.color);
    if (m_hasDoorGhost)
        m_view.DrawDoorGhost(translucent, m_doorGhost, m_doorGhostColor);
    if (m_hasObjectGhost)
        m_view.DrawObjectGhost(translucent, m_objectGhostType, m_objectGhostTile, m_objectGhostRotation, m_objectGhostColor, true);
}
