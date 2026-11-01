#include "warehouse_sim.h"

#include "../world/warehouse_layout.h"
#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace
{
    // ---- Tuning. A game hour is 5 sim seconds at 1x, so a game day is 120.

    const ShipmentKind kShipmentKinds[] = {
        // name          lower          dev            crates cost  hours heat  sale per crate
        { "Cigarettes", "cigarettes", "cigarettes",    8,    1200,  1,    1.0f, 350 },
        { "Booze",      "booze",      "booze",         8,    2400,  1,    3.0f, 640 },
        { "Contraband", "contraband", "contraband",    6,    4000,  3,    18.0f, 1500 },
    };
    constexpr int   kTruckCapacity     = 12;   // crates a Ship out can take at once
    constexpr int   kPickupHours       = 1;    // Ship out: until the truck turns up
    constexpr int   kPaymentHours      = 1;    // from the truck leaving to the money coming in
    constexpr float kShipOutHeat       = 1.0f; // added when a loaded truck leaves
    constexpr int   kMaxCarriers       = 4;    // crew on a truck at once
    constexpr float kHandlingSeconds   = 0.5f; // sim seconds to lift or put down a crate
    constexpr float kCarrySpeed        = 3.6f; // metres per sim second with a crate; empty-handed is the usual run
    constexpr float kHireEverySeconds  = 0.5f;

    constexpr float kOpenSeconds       = 1.5f;
    constexpr float kClearDoorSeconds  = 5.0f; // most a truck waits for crew to get inside before shutting
    constexpr float kRoadSpeed         = 22.0f;
    constexpr float kReverseSpeed      = 7.0f;
    constexpr float kSpeedPerMetre     = 1.3f; // how quickly it gets up to speed and brakes, by distance
    constexpr float kCrawlSpeed        = 1.2f;

    // Taken by carriers as their CrewMember::jobId; other systems use their own values.
    constexpr int kWarehouseJobId = 1001;
    static_assert(kWarehouseJobId < kLairFirstJobId && kWarehouseJobId != CitySim::kMissionJobId);

    // ---- The truck's routes, in world metres. The big door is at x = 16, z = -4; the road runs
    // along z at x = 32, and the truck drives on the x = 34 side going +z. It passes the gate,
    // reverses in an arc to the door, and leaves forward along the same arc.
    constexpr float kLaneX     = 34.0f;
    constexpr float kDockZ     = -4.0f;
    constexpr float kDockX     = 22.6f; // the truck's middle when docked: its tail is 2.35 from the wall
    constexpr float kArcX      = 26.0f;
    constexpr float kArcRadius = kLaneX - kArcX;
    constexpr float kArcZ      = kDockZ + kArcRadius;
    constexpr float kPastGateZ = kArcZ + 12.0f;
    constexpr float kOffMapZ   = 70.0f; // the truck comes from and goes to this far along the road

    using Route = std::vector<Vec3>;

    void AddArc(Route& route, bool towardLane)
    {
        constexpr int kSteps = 14;
        for (int i = 1; i <= kSteps; ++i)
        {
            // Angle from straight below the arc's center (the dock side) to straight right (the lane).
            float t = (float)i / (float)kSteps;
            if (!towardLane)
                t = 1.0f - t;
            const float a = t * 0.5f * kPi;
            route.push_back({ kArcX + kArcRadius * std::sin(a), 0.0f, kArcZ - kArcRadius * std::cos(a) });
        }
    }

    const Route& DriveInRoute()
    {
        static const Route route = { { kLaneX, 0.0f, -kOffMapZ }, { kLaneX, 0.0f, kPastGateZ } };
        return route;
    }
    const Route& ReverseRoute()
    {
        static const Route route = [] {
            Route r = { { kLaneX, 0.0f, kPastGateZ }, { kLaneX, 0.0f, kArcZ } };
            AddArc(r, false);
            r.push_back({ kDockX, 0.0f, kDockZ });
            return r;
        }();
        return route;
    }
    const Route& DriveOffRoute()
    {
        static const Route route = [] {
            Route r = { { kDockX, 0.0f, kDockZ }, { kArcX, 0.0f, kDockZ } };
            AddArc(r, true);
            r.push_back({ kLaneX, 0.0f, kOffMapZ });
            return r;
        }();
        return route;
    }

    float RouteLength(const Route& route)
    {
        float length = 0.0f;
        for (size_t i = 1; i < route.size(); ++i)
            length += Length(route[i] - route[i - 1]);
        return length;
    }

    // Puts the truck `distance` along the route. `reversing`: its tail leads.
    void PlaceTruckOnRoute(WarehouseSim& sim, const Route& route, float distance, bool reversing)
    {
        for (size_t i = 1; i < route.size(); ++i)
        {
            const Vec3  delta  = route[i] - route[i - 1];
            const float length = Length(delta);
            if (distance <= length || i + 1 == route.size())
            {
                sim.truckPosition = route[i - 1] + delta * (std::min(distance, length) / length);
                const Vec3 nose   = reversing ? Vec3{ -delta.x, 0.0f, -delta.z } : delta;
                sim.truckYaw      = std::atan2(nose.x, nose.z);
                return;
            }
            distance -= length;
        }
    }

    // Moves along the route, easing off at both ends. True when it reached the end.
    bool DriveTruck(WarehouseSim& sim, const Route& route, float topSpeed, bool reversing, float simSeconds)
    {
        const float length = RouteLength(route);
        const float fromStart = sim.routeDistance;
        const float toEnd     = length - sim.routeDistance;
        const float speed     = std::min(topSpeed, kCrawlSpeed + kSpeedPerMetre * std::min(fromStart, toEnd));
        sim.routeDistance = std::min(sim.routeDistance + speed * simSeconds, length);
        PlaceTruckOnRoute(sim, route, sim.routeDistance, reversing);
        return sim.routeDistance >= length;
    }

    void SetPhase(WarehouseSim& sim, TruckPhase phase)
    {
        sim.phase         = phase;
        sim.phaseSeconds  = 0.0f;
        sim.routeDistance = 0.0f;
    }

    void SetDoorEdges(GameState& state, bool open)
    {
        WarehouseSim& sim = state.warehouse;
        if (sim.doorEdgesOpen == open)
            return;
        sim.doorEdgesOpen = open;
        for (const TileCoord& tile : kDockDoorTiles)
            state.world.SetEdge(tile, EdgeSide::PosX, open ? EdgeKind::None : EdgeKind::Wall);
    }

    const char* PluralCrates(int count)
    {
        return count == 1 ? "crate" : "crates";
    }

    // ---- Stored crates

    StoredCrate* FindStored(WarehouseSim& sim, int slot)
    {
        for (StoredCrate& crate : sim.stored)
            if (crate.slot == slot)
                return &crate;
        return nullptr;
    }

    bool IsSlotBusy(const WarehouseSim& sim, int slot)
    {
        for (const StoredCrate& crate : sim.stored)
            if (crate.slot == slot)
                return true;
        for (const CrateCarrier& carrier : sim.carriers)
            if (carrier.slot == slot)
                return true;
        return false;
    }

    int FindFreeSlot(const WarehouseSim& sim)
    {
        for (int slot = 0;; ++slot)
            if (!IsSlotBusy(sim, slot))
                return slot;
    }

    void StoreCrate(GameState& state, int slot, int kind)
    {
        const CrateSlot place = GetCrateSlot(state.world, slot);
        StoredCrate     crate;
        crate.slot     = slot;
        crate.kind     = kind;
        crate.position = place.position;
        crate.tile     = place.tile;
        state.warehouse.stored.push_back(crate);
        state.crates = (int)state.warehouse.stored.size();
    }

    // GameState::crates is what other systems change: follow it.
    void SyncStoredCrates(GameState& state)
    {
        WarehouseSim& sim = state.warehouse;
        while ((int)sim.stored.size() > state.crates && !sim.stored.empty())
            sim.stored.pop_back();
        while ((int)sim.stored.size() < state.crates)
        {
            const int wanted = state.crates;
            StoreCrate(state, FindFreeSlot(sim), 0);
            state.crates = wanted;
        }
    }

    int CratesOnTheWayIn(const WarehouseSim& sim)
    {
        return sim.job == TruckJob::Deliver ? (int)sim.carriers.size() : 0;
    }

    // ---- Carriers

    bool SendTo(CrewMember& member, CrateCarrier& carrier, const GameState& state, const TileCoord& goal)
    {
        carrier.goal = goal;
        return SendCharacterTo(member.motion, state.world, goal);
    }

    // Starts `carrier`'s next trip. False if there's no work for one more trip, or no way there.
    bool StartTrip(GameState& state, CrateCarrier& carrier, CrewMember& member)
    {
        WarehouseSim& sim = state.warehouse;
        const int     stand = (int)(&carrier - sim.carriers.data()) % (int)std::size(kTruckStandTiles);
        carrier.stage      = CrateCarrier::Stage::ToCrate;
        carrier.kind       = -1;
        carrier.slot       = -1;
        carrier.timer      = 0.0f;
        carrier.standIndex = stand;
        member.motion.speed = CharacterMotion{}.speed;

        if (sim.job == TruckJob::Deliver)
        {
            // Every other carrier already has a crate or is walking to one of theirs.
            int walkingToTruck = 0, holding = 0;
            for (const CrateCarrier& other : sim.carriers)
            {
                if (&other == &carrier || other.crewId == 0)
                    continue;
                if (other.IsHolding())
                    ++holding;
                else
                    ++walkingToTruck;
            }
            if ((int)sim.cargo.size() - walkingToTruck <= 0)
                return false;
            if (state.crates + walkingToTruck + holding >= CrateCapacity(state))
                return false;
            carrier.slot = FindFreeSlot(sim);
            return SendTo(member, carrier, state, kTruckStandTiles[stand]);
        }

        // Collect: the oldest crate nobody's after, if the truck has room for it.
        int aboard = (int)sim.cargo.size();
        for (const CrateCarrier& other : sim.carriers)
            if (&other != &carrier && other.crewId != 0)
                ++aboard;
        if (aboard >= kTruckCapacity)
            return false;
        for (StoredCrate& crate : sim.stored)
        {
            if (crate.taken)
                continue;
            carrier.slot = crate.slot;
            if (!SendTo(member, carrier, state, GetCrateSlot(state.world, crate.slot).stand))
            {
                carrier.slot = -1;
                return false;
            }
            crate.taken = true;
            return true;
        }
        return false;
    }

    // The carrier's trip is over for good: hands back whatever they had and frees them.
    void ReleaseCarrier(GameState& state, CrateCarrier& carrier, bool sendInside)
    {
        WarehouseSim& sim = state.warehouse;
        if (carrier.kind >= 0)
        {
            // Dropped mid-trip (sent on a mission, say): the crate goes back where it came from.
            if (sim.job == TruckJob::Deliver)
                sim.cargo.push_back((uint8_t)carrier.kind);
            else
                StoreCrate(state, carrier.slot, carrier.kind);
        }
        else if (sim.job == TruckJob::Collect && carrier.slot >= 0)
        {
            if (StoredCrate* crate = FindStored(sim, carrier.slot))
                crate->taken = false;
        }
        carrier.kind = -1;
        carrier.slot = -1;

        if (CrewMember* member = state.FindCrew(carrier.crewId))
        {
            if (member->jobId == kWarehouseJobId)
                member->jobId = 0;
            member->motion.speed = CharacterMotion{}.speed;
            if (sendInside && !member->away)
                SendCharacterTo(member->motion, state.world, kDockStagingTile);
        }
        carrier.crewId = 0;
    }

    void RemoveFinishedCarriers(WarehouseSim& sim)
    {
        sim.carriers.erase(std::remove_if(sim.carriers.begin(), sim.carriers.end(), [](const CrateCarrier& c) { return c.crewId == 0; }),
                           sim.carriers.end());
    }

    void TickCarrier(GameState& state, CrateCarrier& carrier, const SimStep& step)
    {
        WarehouseSim& sim    = state.warehouse;
        CrewMember*   member = state.FindCrew(carrier.crewId);
        if (!member || member->away || member->jobId != kWarehouseJobId)
        {
            ReleaseCarrier(state, carrier, false);
            return;
        }

        // A walk that stopped short (the world changed): try again, or give up.
        const auto arrived = [&]() {
            if (member->motion.IsMoving())
                return false;
            if (state.world.WorldToTile(carrier.goal.level, member->motion.position) == carrier.goal && member->motion.level == carrier.goal.level)
                return true;
            if (!SendCharacterTo(member->motion, state.world, carrier.goal))
                ReleaseCarrier(state, carrier, false);
            return false;
        };

        switch (carrier.stage)
        {
        case CrateCarrier::Stage::ToCrate:
            if (arrived())
            {
                carrier.stage = CrateCarrier::Stage::Lifting;
                carrier.timer = kHandlingSeconds;
            }
            break;

        case CrateCarrier::Stage::Lifting:
            carrier.timer -= step.simSeconds;
            if (carrier.timer > 0.0f)
                break;
            if (sim.job == TruckJob::Deliver)
            {
                if (sim.cargo.empty())
                {
                    if (!StartTrip(state, carrier, *member))
                        ReleaseCarrier(state, carrier, true);
                    break;
                }
                carrier.kind = sim.cargo.back();
                sim.cargo.pop_back();
                SendTo(*member, carrier, state, GetCrateSlot(state.world, carrier.slot).stand);
            }
            else
            {
                StoredCrate* crate = FindStored(sim, carrier.slot);
                if (!crate)
                {
                    // Gone from under them (the law took it).
                    carrier.slot = -1;
                    if (!StartTrip(state, carrier, *member))
                        ReleaseCarrier(state, carrier, true);
                    break;
                }
                carrier.kind = crate->kind;
                sim.stored.erase(sim.stored.begin() + (crate - sim.stored.data()));
                state.crates = (int)sim.stored.size();
                SendTo(*member, carrier, state, kTruckStandTiles[carrier.standIndex]);
            }
            member->motion.speed = kCarrySpeed;
            carrier.stage = CrateCarrier::Stage::Carrying;
            break;

        case CrateCarrier::Stage::Carrying:
            if (arrived())
            {
                carrier.stage = CrateCarrier::Stage::Putting;
                carrier.timer = kHandlingSeconds;
            }
            break;

        case CrateCarrier::Stage::Putting:
            carrier.timer -= step.simSeconds;
            if (carrier.timer > 0.0f)
                break;
            if (sim.job == TruckJob::Deliver)
                StoreCrate(state, carrier.slot, carrier.kind);
            else
                sim.cargo.push_back((uint8_t)carrier.kind);
            carrier.kind = -1;
            carrier.slot = -1;
            if (!StartTrip(state, carrier, *member))
                ReleaseCarrier(state, carrier, true);
            break;
        }
    }

    // True if `member` can be given a crate: free, here, not the boss, and not partway down a staircase.
    bool CanHire(const GameState& state, const CrewMember& member)
    {
        if (member.away || member.jobId != 0 || member.role == CrewRole::Boss || !IsFitForWork(state, member))
            return false;
        const TileCoord under = state.world.WorldToTile(member.motion.level, member.motion.position);
        return state.world.GetTile(under).kind != TileKind::Stairwell;
    }

    // Someone who'll be free for the truck in a moment: stepping off the stairs, or finishing a
    // bit of lair work (the lair holds back free men while the truck needs them).
    bool IsCrewComingFree(const GameState& state)
    {
        for (const CrewMember& member : state.crew)
        {
            if (member.away || member.role == CrewRole::Boss)
                continue;
            if (member.jobId >= kLairFirstJobId || (member.jobId == 0 && !CanHire(state, member)))
                return true;
        }
        return false;
    }

    // Trips nobody's on yet.
    int WorkLeft(const GameState& state)
    {
        const WarehouseSim& sim = state.warehouse;
        if (sim.job == TruckJob::Deliver)
        {
            int walkingToTruck = 0;
            for (const CrateCarrier& carrier : sim.carriers)
                if (!carrier.IsHolding())
                    ++walkingToTruck;
            const int cargoLeft = (int)sim.cargo.size() - walkingToTruck;
            const int roomLeft  = CrateCapacity(state) - state.crates - CratesOnTheWayIn(sim);
            return std::min(cargoLeft, roomLeft);
        }
        int crates = 0;
        for (const StoredCrate& crate : sim.stored)
            if (!crate.taken)
                ++crates;
        const int aboard = (int)sim.cargo.size() + (int)sim.carriers.size();
        return std::min(crates, kTruckCapacity - aboard);
    }

    void HireCarriers(GameState& state)
    {
        WarehouseSim& sim = state.warehouse;
        int           wanted = std::min(WorkLeft(state), kMaxCarriers - (int)sim.carriers.size());
        for (CrewMember& member : state.crew)
        {
            if (wanted <= 0)
                break;
            if (!CanHire(state, member))
                continue;

            CrateCarrier carrier;
            carrier.crewId = member.id;
            sim.carriers.push_back(carrier);
            if (StartTrip(state, sim.carriers.back(), member))
            {
                member.jobId = kWarehouseJobId;
                --wanted;
            }
            else
            {
                // No path up here (or the work ran out): leave them be.
                member.motion.speed = CharacterMotion{}.speed;
                sim.carriers.pop_back();
            }
        }
    }

    void LeaveDock(GameState& state)
    {
        WarehouseSim& sim = state.warehouse;
        for (CrateCarrier& carrier : sim.carriers)
            ReleaseCarrier(state, carrier, true);
        sim.carriers.clear();
        SetPhase(sim, TruckPhase::Closing);
    }

    void UpdateWorking(GameState& state, const SimStep& step)
    {
        WarehouseSim& sim = state.warehouse;

        for (CrateCarrier& carrier : sim.carriers)
            TickCarrier(state, carrier, step);
        RemoveFinishedCarriers(sim);

        sim.hireTimer -= step.simSeconds;
        if (sim.hireTimer > 0.0f)
            return;
        sim.hireTimer = kHireEverySeconds;
        HireCarriers(state);
        if (!sim.carriers.empty())
        {
            sim.hold = TruckHold::None;
            return;
        }

        // Nobody on it. Done, or stuck?
        TruckHold hold = TruckHold::None;
        if (sim.job == TruckJob::Deliver)
        {
            if (sim.cargo.empty())
            {
                LeaveDock(state);
                sim.hold = TruckHold::None;
                return;
            }
            hold = state.crates >= CrateCapacity(state) ? TruckHold::NoRoom : TruckHold::NoCrew;
        }
        else
        {
            bool cratesLeft = false;
            for (const StoredCrate& crate : sim.stored)
                cratesLeft = cratesLeft || !crate.taken;
            if (!cratesLeft || (int)sim.cargo.size() >= kTruckCapacity)
            {
                LeaveDock(state);
                sim.hold = TruckHold::None;
                return;
            }
            hold = TruckHold::NoCrew;
        }

        if (hold == TruckHold::NoCrew && IsCrewComingFree(state))
            return;

        if (hold != sim.hold)
        {
            if (hold == TruckHold::NoCrew)
            {
                state.PostEvent(GameEventKind::Bad, "The truck is waiting: nobody is free to carry crates.");
            }
            else
            {
                char text[120];
                snprintf(text, sizeof(text), "Warehouse full at %d crates: %d %s stay on the truck.", state.crates, (int)sim.cargo.size(),
                         PluralCrates((int)sim.cargo.size()));
                state.PostEvent(GameEventKind::Bad, text);
            }
        }
        sim.hold = hold;
    }

    void AnnounceDocked(GameState& state)
    {
        WarehouseSim& sim = state.warehouse;
        sim.cargoAtDock   = (int)sim.cargo.size();
        char text[120];
        if (sim.job == TruckJob::Deliver)
        {
            const ShipmentKind& kind = GetShipmentKind(sim.orderKind);
            state.AddHeat(kind.heat);
            snprintf(text, sizeof(text), "The truck docked with %d %s of %s.", sim.cargoAtDock, PluralCrates(sim.cargoAtDock), kind.lowerName);
        }
        else
        {
            snprintf(text, sizeof(text), "The truck docked to take crates away.");
        }
        state.PostEvent(GameEventKind::Info, text);
    }

    void AnnounceLeaving(GameState& state)
    {
        WarehouseSim& sim = state.warehouse;
        if (sim.job != TruckJob::Collect)
            return;

        if (sim.cargo.empty())
        {
            state.PostEvent(GameEventKind::Info, "The truck left empty.");
            return;
        }

        int64_t value = 0;
        for (const uint8_t kind : sim.cargo)
            value += GetShipmentKind(kind).salePerCrate;
        PendingPayment payment;
        payment.amount      = value;
        payment.crates      = (int)sim.cargo.size();
        payment.minutesLeft = (float)kPaymentHours * 60.0f;
        sim.payments.push_back(payment);
        state.AddHeat(kShipOutHeat);

        char text[120];
        snprintf(text, sizeof(text), "The truck left with %d %s: %s due in %d %s.", payment.crates, PluralCrates(payment.crates),
                 WarehouseMoneyText(value).c_str(), kPaymentHours, kPaymentHours == 1 ? "hour" : "hours");
        state.PostEvent(GameEventKind::Info, text);
        sim.cargo.clear();
    }

    // Anyone in the way of the big door?
    bool IsDoorwayClear(const GameState& state)
    {
        for (const CrewMember& member : state.crew)
        {
            if (member.away || member.motion.level != WorldLevel::Ground)
                continue;
            const TileCoord tile = state.world.WorldToTile(WorldLevel::Ground, member.motion.position);
            if (tile.x >= kWarehouseX1 && tile.x <= kWarehouseX1 + 2 && tile.z >= kDockDoorTiles[0].z - 1 && tile.z <= kDockDoorTiles[1].z + 1)
                return false;
        }
        return true;
    }

    void UpdatePayments(GameState& state, const SimStep& step)
    {
        WarehouseSim& sim = state.warehouse;
        for (PendingPayment& payment : sim.payments)
        {
            payment.minutesLeft -= step.gameMinutes;
            if (payment.minutesLeft > 0.0f)
                continue;
            state.money += payment.amount;
            sim.moneyEarned += payment.amount;
            char text[120];
            snprintf(text, sizeof(text), "%s came in for %d %s.", WarehouseMoneyText(payment.amount).c_str(), payment.crates, PluralCrates(payment.crates));
            state.PostEvent(GameEventKind::Good, text);
        }
        sim.payments.erase(std::remove_if(sim.payments.begin(), sim.payments.end(), [](const PendingPayment& p) { return p.minutesLeft <= 0.0f; }),
                           sim.payments.end());
    }
}

std::string WarehouseMoneyText(int64_t money)
{
    std::string digits = std::to_string(money < 0 ? -money : money);
    for (int i = (int)digits.size() - 3; i > 0; i -= 3)
        digits.insert((size_t)i, ",");
    return (money < 0 ? "-$" : "$") + digits;
}

int GetShipmentKindCount()
{
    return (int)std::size(kShipmentKinds);
}

const ShipmentKind& GetShipmentKind(int kind)
{
    return kShipmentKinds[std::clamp(kind, 0, GetShipmentKindCount() - 1)];
}

int CarriersWanted(const GameState& state)
{
    const WarehouseSim& sim = state.warehouse;
    if (sim.phase != TruckPhase::Working)
        return 0;
    return std::max(std::min(WorkLeft(state), kMaxCarriers - (int)sim.carriers.size()), 0);
}

int CrateCapacity(const GameState& state)
{
    return GetRackSlotCount() + ExtraCrateCapacity(state);
}

bool CanOrderShipment(const GameState& state, int kind, const char** why)
{
    const auto refuse = [&](const char* reason) {
        if (why)
            *why = reason;
        return false;
    };
    if (state.warehouse.phase != TruckPhase::Gone)
        return refuse("The truck is busy.");
    if (state.money < GetShipmentKind(kind).cost)
        return refuse("Not enough money.");
    return true;
}

bool CanShipOut(const GameState& state, const char** why)
{
    const auto refuse = [&](const char* reason) {
        if (why)
            *why = reason;
        return false;
    };
    const WarehouseSim& sim = state.warehouse;
    if (sim.phase == TruckPhase::Working && sim.hold == TruckHold::NoRoom)
        return true; // what's on the truck goes straight on to the buyer, with some of the warehouse
    if (sim.phase != TruckPhase::Gone)
        return refuse("The truck is busy.");
    if (state.crates <= 0)
        return refuse("No crates to ship.");
    return true;
}

int ShipOutCrateCount(const GameState& state)
{
    // What a truck is holding only counts when it's stuck: a working truck is still delivering it.
    const WarehouseSim& sim = state.warehouse;
    const int aboard = sim.hold == TruckHold::NoRoom ? (int)sim.cargo.size() : 0;
    return std::min(state.crates + aboard, kTruckCapacity);
}

int64_t ShipOutValue(const GameState& state)
{
    const WarehouseSim& sim = state.warehouse;
    int     left  = kTruckCapacity;
    int64_t value = 0;
    if (sim.hold == TruckHold::NoRoom)
    {
        for (const uint8_t kind : sim.cargo)
        {
            if (left-- <= 0)
                break;
            value += GetShipmentKind(kind).salePerCrate;
        }
    }
    for (const StoredCrate& crate : sim.stored)
    {
        if (left-- <= 0)
            break;
        value += GetShipmentKind(crate.kind).salePerCrate;
    }
    return value;
}

bool OrderShipment(GameState& state, int kind)
{
    if (!CanOrderShipment(state, kind))
        return false;

    WarehouseSim&       sim      = state.warehouse;
    const ShipmentKind& shipment = GetShipmentKind(kind);
    state.money -= shipment.cost;
    sim.job          = TruckJob::Deliver;
    sim.orderKind    = kind;
    sim.hold         = TruckHold::None;
    sim.phaseMinutes = (float)shipment.hours * 60.0f;
    sim.cargo.assign((size_t)shipment.crates, (uint8_t)kind);
    SetPhase(sim, TruckPhase::OnItsWay);

    char text[120];
    snprintf(text, sizeof(text), "Ordered %d crates of %s for %s: the truck is due in %d %s.", shipment.crates, shipment.lowerName,
             WarehouseMoneyText(shipment.cost).c_str(), shipment.hours, shipment.hours == 1 ? "hour" : "hours");
    state.PostEvent(GameEventKind::Info, text);
    return true;
}

bool OrderShipOut(GameState& state)
{
    if (!CanShipOut(state))
        return false;

    WarehouseSim& sim = state.warehouse;
    sim.job  = TruckJob::Collect;
    sim.hold = TruckHold::None;
    if (sim.phase == TruckPhase::Working)
    {
        // Stuck with cargo and no room: it stays aboard and goes to the buyer with the rest.
        state.PostEvent(GameEventKind::Info, "The truck will take what it's holding, and crates from the racks, to the buyer.");
        return true;
    }

    sim.phaseMinutes = (float)kPickupHours * 60.0f;
    sim.cargo.clear();
    SetPhase(sim, TruckPhase::OnItsWay);
    state.PostEvent(GameEventKind::Info, "Sent for the truck to ship crates out.");
    return true;
}

int AbandonWarehouse(GameState& state)
{
    WarehouseSim& sim = state.warehouse;
    for (CrateCarrier& carrier : sim.carriers)
        ReleaseCarrier(state, carrier, false); // a crate in his hands goes back, to be counted
    sim.carriers.clear();

    const bool  docked = sim.phase != TruckPhase::Gone && sim.phase != TruckPhase::DrivingOff;
    const int   lost   = (int)sim.stored.size() + (docked ? (int)sim.cargo.size() : 0);
    sim.stored.clear();
    sim.cargo.clear();
    state.crates = 0;

    SetDoorEdges(state, false);
    sim.openAmount = 0.0f;
    sim.hold       = TruckHold::None;
    SetPhase(sim, TruckPhase::Gone);
    return lost;
}

std::string DescribeTruck(const GameState& state)
{
    const WarehouseSim& sim = state.warehouse;
    char text[160];
    switch (sim.phase)
    {
    case TruckPhase::Gone:
        return "No truck. Order a shipment, or ship crates out.";
    case TruckPhase::OnItsWay:
    {
        const int hours = (int)std::ceil(sim.phaseMinutes / 60.0f);
        snprintf(text, sizeof(text), "On its way, %s: due in %d %s", sim.job == TruckJob::Deliver ? GetShipmentKind(sim.orderKind).lowerName : "to collect",
                 hours, hours == 1 ? "hour" : "hours");
        return text;
    }
    case TruckPhase::DrivingIn:
        return "Coming down the road.";
    case TruckPhase::Reversing:
        return "Reversing to the dock.";
    case TruckPhase::Opening:
        return "Docked: opening up.";
    case TruckPhase::Working:
        if (sim.hold == TruckHold::NoCrew)
            return "Waiting: nobody free to carry crates.";
        if (sim.hold == TruckHold::NoRoom)
        {
            snprintf(text, sizeof(text), "Waiting: warehouse full, %d %s still on the truck. Ship out, or add storage.", (int)sim.cargo.size(),
                     PluralCrates((int)sim.cargo.size()));
            return text;
        }
        if (sim.job == TruckJob::Deliver)
        {
            snprintf(text, sizeof(text), "Unloading: %d of %d %s left on the truck.", (int)sim.cargo.size(), sim.cargoAtDock, PluralCrates(sim.cargoAtDock));
            return text;
        }
        snprintf(text, sizeof(text), "Loading: %d %s on the truck.", (int)sim.cargo.size(), PluralCrates((int)sim.cargo.size()));
        return text;
    case TruckPhase::Closing:
        return sim.job == TruckJob::Collect && !sim.cargo.empty() ? "Loaded: shutting up to leave." : "Done: shutting up to leave.";
    case TruckPhase::DrivingOff:
        return "Leaving.";
    }
    return {};
}

float GetTruckProgress(const GameState& state)
{
    const WarehouseSim& sim = state.warehouse;
    switch (sim.phase)
    {
    case TruckPhase::Gone:
        return 0.0f;
    case TruckPhase::OnItsWay:
    {
        const float total = (sim.job == TruckJob::Deliver ? (float)GetShipmentKind(sim.orderKind).hours : (float)kPickupHours) * 60.0f;
        return 1.0f - sim.phaseMinutes / total;
    }
    case TruckPhase::DrivingIn:
        return sim.routeDistance / RouteLength(DriveInRoute());
    case TruckPhase::Reversing:
        return sim.routeDistance / RouteLength(ReverseRoute());
    case TruckPhase::Opening:
    case TruckPhase::Closing:
        return sim.openAmount;
    case TruckPhase::Working:
        if (sim.job == TruckJob::Deliver)
            return sim.cargoAtDock > 0 ? 1.0f - (float)sim.cargo.size() / (float)sim.cargoAtDock : 1.0f;
        return (float)sim.cargo.size() / (float)std::max(kTruckCapacity, 1);
    case TruckPhase::DrivingOff:
        return sim.routeDistance / RouteLength(DriveOffRoute());
    }
    return 0.0f;
}

std::string DescribeWarehouseForDev(const GameState& state)
{
    static const char* const kPhases[] = { "gone", "onitsway", "drivingin", "reversing", "opening", "working", "closing", "drivingoff" };
    static const char* const kHolds[]  = { "none", "nocrew", "noroom" };

    const WarehouseSim& sim = state.warehouse;
    std::string out = " crates=" + std::to_string(state.crates) + " capacity=" + std::to_string(CrateCapacity(state));
    out += " truck=" + std::string(kPhases[(int)sim.phase]);
    out += " truckjob=" + std::string(sim.job == TruckJob::Deliver ? "deliver" : "collect");
    out += " hold=" + std::string(kHolds[(int)sim.hold]);
    out += " cargo=" + std::to_string(sim.cargo.size());
    out += " carriers=";
    bool first = true;
    for (const CrateCarrier& carrier : sim.carriers)
    {
        for (const CrewMember& member : state.crew)
        {
            if (member.id == carrier.crewId)
            {
                out += (first ? "" : ",") + member.name + (carrier.IsHolding() ? "*" : "");
                first = false;
            }
        }
    }
    int64_t due = 0;
    for (const PendingPayment& payment : sim.payments)
        due += payment.amount;
    out += " paymentsdue=" + std::to_string(due);
    return out;
}

void WarehouseSim::Tick(GameState& state, const SimStep& step)
{
    SyncStoredCrates(state);
    UpdatePayments(state, step);

    phaseSeconds += step.simSeconds;
    switch (phase)
    {
    case TruckPhase::Gone:
        break;

    case TruckPhase::OnItsWay:
        phaseMinutes -= step.gameMinutes;
        if (phaseMinutes <= 0.0f)
        {
            SetPhase(*this, TruckPhase::DrivingIn);
            PlaceTruckOnRoute(*this, DriveInRoute(), 0.0f, false);
        }
        break;

    case TruckPhase::DrivingIn:
        if (DriveTruck(*this, DriveInRoute(), kRoadSpeed, false, step.simSeconds))
            SetPhase(*this, TruckPhase::Reversing);
        break;

    case TruckPhase::Reversing:
        if (DriveTruck(*this, ReverseRoute(), kReverseSpeed, true, step.simSeconds))
        {
            SetPhase(*this, TruckPhase::Opening);
            SetDoorEdges(state, true);
            AnnounceDocked(state);
        }
        break;

    case TruckPhase::Opening:
        openAmount = std::min(openAmount + step.simSeconds / kOpenSeconds, 1.0f);
        if (openAmount >= 1.0f)
        {
            SetPhase(*this, TruckPhase::Working);
            hireTimer = 0.0f;
        }
        break;

    case TruckPhase::Working:
        UpdateWorking(state, step);
        break;

    case TruckPhase::Closing:
        if (!IsDoorwayClear(state) && phaseSeconds < kClearDoorSeconds)
            break;
        SetDoorEdges(state, false);
        openAmount = std::max(openAmount - step.simSeconds / kOpenSeconds, 0.0f);
        if (openAmount <= 0.0f)
        {
            AnnounceLeaving(state);
            SetPhase(*this, TruckPhase::DrivingOff);
        }
        break;

    case TruckPhase::DrivingOff:
        if (DriveTruck(*this, DriveOffRoute(), kRoadSpeed, false, step.simSeconds))
        {
            SetPhase(*this, TruckPhase::Gone);
            hold = TruckHold::None;
        }
        break;
    }
}
