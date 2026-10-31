#include "character_motion.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTurnSpeed = 12.0f; // radians per second
    constexpr int   kWalkableSearchRings = 8;

    // Yaw that turns the model's forward (-Z) toward `dir` (x/z).
    float YawFromDirection(Vec3 dir)
    {
        return std::atan2(-dir.x, -dir.z);
    }

    float WrapAngle(float a)
    {
        while (a > kPi)
            a -= 2.0f * kPi;
        while (a < -kPi)
            a += 2.0f * kPi;
        return a;
    }

    void Stop(CharacterMotion& motion)
    {
        motion.path.clear();
        motion.nextWaypoint = 0;
    }
}

void PlaceCharacter(CharacterMotion& motion, const WorldGrid& world, const TileCoord& tile)
{
    motion.position = world.TileCenter(tile);
    motion.level    = tile.level;
    motion.goal     = tile;
    Stop(motion);
}

bool SendCharacterTo(CharacterMotion& motion, const WorldGrid& world, const TileCoord& goal)
{
    // From the tile underfoot; on a stairwell (or anything else unwalkable) from the nearest
    // tile that isn't. Motion on the stairs themselves finishes first, see UpdateCharacterMotion.
    TileCoord from = world.WorldToTile(motion.level, motion.position);
    if (!world.IsWalkable(from))
        FindNearestWalkable(world, from, kWalkableSearchRings, from);

    motion.goal         = goal;
    motion.gridRevision = world.GetRevision();
    motion.nextWaypoint = 0;
    if (!FindWorldPath(world, motion.position, from, goal, motion.radius, motion.path))
    {
        Stop(motion);
        return false;
    }
    return true;
}

void UpdateCharacterMotion(CharacterMotion& motion, const WorldGrid& world, float simSeconds)
{
    if (!motion.IsMoving() || simSeconds <= 0.0f)
        return;

    // Walking between two levels: finish the stairs before thinking again.
    const WorldWaypoint& ahead = motion.path[motion.nextWaypoint];
    if (motion.gridRevision != world.GetRevision() && ahead.level == motion.level)
        SendCharacterTo(motion, world, motion.goal);

    float remaining = motion.speed * simSeconds;
    while (remaining > 0.0f && motion.IsMoving())
    {
        const WorldWaypoint& target = motion.path[motion.nextWaypoint];
        const Vec3           delta  = target.position - motion.position;
        const float          dist   = Length(delta);

        if (delta.x * delta.x + delta.z * delta.z > 1e-6f)
        {
            const float wanted = YawFromDirection(delta);
            const float turn   = WrapAngle(wanted - motion.yaw);
            const float step   = kTurnSpeed * simSeconds;
            motion.yaw = WrapAngle(motion.yaw + std::clamp(turn, -step, step));
        }

        if (dist <= remaining)
        {
            motion.position = target.position;
            motion.level    = target.level;
            remaining -= dist;
            ++motion.nextWaypoint;
        }
        else
        {
            motion.position += delta * (remaining / dist);
            remaining = 0.0f;
        }
    }

    if (!motion.IsMoving())
        Stop(motion);
}
