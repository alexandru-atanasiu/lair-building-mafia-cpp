#pragma once

#include "../world/world_grid.h"

#include <cstdint>
#include <string>
#include <vector>

struct GameState;
struct SimStep;

// The warehouse on the ground level: the truck that docks with smuggled goods and takes them away.
// GameState holds one of these and Simulation::Advance calls Tick on every step of the clock
// (never while paused). The counts and prices are constants at the top of warehouse_sim.cpp.
//
// To read from elsewhere (law_sim): GameState::crates is how many crates are stored,
// WarehouseSim::stored says where each of them is, and truck/cargo say what's in the yard. To take
// crates away, lower GameState::crates: the newest go first and Tick keeps `stored` in step.

// What the player can order. Data: see kShipmentKinds in warehouse_sim.cpp.
struct ShipmentKind
{
    const char* name;         // "Cigarettes"
    const char* lowerName;    // "cigarettes", in the middle of a sentence
    const char* devName;      // for the dev driver's order command
    int         crates;
    int64_t     cost;         // paid when ordered
    int         hours;        // game hours until the truck turns up
    float       heat;         // added when it docks
    int64_t     salePerCrate; // what shipping a crate out pays
};
int                 GetShipmentKindCount();
const ShipmentKind& GetShipmentKind(int kind);

enum class TruckPhase
{
    Gone,       // no truck, nothing ordered
    OnItsWay,   // ordered: off the map, `phaseMinutes` game minutes from turning up
    DrivingIn,  // along the road and past the gate
    Reversing,  // back to the big door
    Opening,    // door up, truck open
    Working,    // docked: crew carrying
    Closing,    // done: door down, truck shut
    DrivingOff, // leaving along the road
};

enum class TruckJob
{
    Deliver, // bringing crates to the warehouse
    Collect, // taking crates away to be sold
};

// Why a docked truck isn't being worked.
enum class TruckHold
{
    None,
    NoCrew, // nobody free to carry
    NoRoom, // cargo left and the warehouse is full
};

// A crate in the warehouse.
struct StoredCrate
{
    int       slot = 0;     // GetCrateSlot
    int       kind = 0;     // GetShipmentKind
    Vec3      position;     // bottom center, in the world
    TileCoord tile;         // the tile it's on
    bool      taken = false; // a carrier is on the way to it
};

// A crew member working the truck. Holds their jobId (kWarehouseJobId in the .cpp) until done.
struct CrateCarrier
{
    enum class Stage
    {
        ToCrate,  // walking to the crate
        Lifting,  // at it, picking it up
        Carrying, // walking with it
        Putting,  // there, putting it down
    };

    uint32_t  crewId = 0;
    Stage     stage = Stage::ToCrate;
    int       slot = -1;      // the warehouse slot at the other end of this trip
    int       kind = -1;      // the crate's kind while it's in their hands
    int       standIndex = 0; // which of kTruckStandTiles
    float     timer = 0.0f;
    TileCoord goal;

    bool IsHolding() const { return stage == Stage::Carrying || stage == Stage::Putting; }
};

// Money on its way for crates that left.
struct PendingPayment
{
    int64_t amount = 0;
    int     crates = 0;
    float   minutesLeft = 0.0f;
};

struct WarehouseSim
{
    void Tick(GameState& state, const SimStep& step);

    TruckPhase phase = TruckPhase::Gone;
    TruckJob   job   = TruckJob::Deliver;
    TruckHold  hold  = TruckHold::None;
    int        orderKind = 0;       // what a Deliver job brings
    float      phaseMinutes = 0.0f; // OnItsWay: game minutes left
    float      phaseSeconds = 0.0f; // sim seconds in this phase
    int        cargoAtDock = 0;     // cargo when it docked, for progress

    // The truck, in the world. Only there while phase isn't Gone or OnItsWay.
    Vec3  truckPosition;
    float truckYaw = 0.0f;  // the nose is +Z turned by this
    float routeDistance = 0.0f;
    float openAmount = 0.0f; // 0 shut .. 1 open: the big door, the tarp and the tail gates

    std::vector<uint8_t>        cargo;    // kinds, first loaded (nearest the cab) first
    std::vector<StoredCrate>    stored;   // one per GameState::crates
    std::vector<CrateCarrier>   carriers;
    std::vector<PendingPayment> payments;

    float hireTimer = 0.0f;
    bool  doorEdgesOpen = false;
};

// Free men a docked truck could use right now. The truck comes before lair work, which holds this
// many back (DispatchJobs in lair_sim.cpp), so a long dig can't starve it.
int CarriersWanted(const GameState& state);

// Crates that fit: the racks plus ExtraCrateCapacity.
int CrateCapacity(const GameState& state);

// True when a truck can be ordered now. Otherwise `why` says what's stopping it.
bool CanOrderShipment(const GameState& state, int kind, const char** why = nullptr);
bool CanShipOut(const GameState& state, const char** why = nullptr);
// How many crates a Ship out would take now, and what they'd pay.
int     ShipOutCrateCount(const GameState& state);
int64_t ShipOutValue(const GameState& state);
bool    OrderShipment(GameState& state, int kind); // pays and sends for the truck
bool    OrderShipOut(GameState& state);            // sends for the truck, or turns a stuck one round

// "$1,200"
std::string WarehouseMoneyText(int64_t money);
// One line for the panel: "On its way: 2 hours", "Unloading: 3 of 8 crates off", ...
std::string DescribeTruck(const GameState& state);
// 0..1 through what the truck is doing, for a progress bar.
float       GetTruckProgress(const GameState& state);
// For the dev driver's state command.
std::string DescribeWarehouseForDev(const GameState& state);
