#include "lair_sim.h"

// The lair's content. A new room type or object is one entry here, plus its meshes in
// py/create_meshes.py and its icon in py/create_ui_assets.py: no code knows these names. Effects
// and uses are the fields at the end of an object's entry (see LairObjectType).

namespace
{
    const Color kDarkWood  = { 0.30f, 0.19f, 0.11f, 1.0f };
    const Color kWood      = { 0.48f, 0.33f, 0.19f, 1.0f };
    const Color kPaleWood  = { 0.64f, 0.50f, 0.32f, 1.0f };
    const Color kMahogany  = { 0.32f, 0.13f, 0.08f, 1.0f };
    const Color kSteel     = { 0.42f, 0.44f, 0.47f, 1.0f };
    const Color kGunmetal  = { 0.15f, 0.16f, 0.18f, 1.0f };
    const Color kOlive     = { 0.28f, 0.31f, 0.18f, 1.0f };
    const Color kLinen     = { 0.78f, 0.75f, 0.66f, 1.0f };
    const Color kCardboard = { 0.60f, 0.46f, 0.29f, 1.0f };
    const Color kBrass     = { 0.78f, 0.62f, 0.25f, 1.0f };
    const Color kGreenFelt = { 0.12f, 0.30f, 0.20f, 1.0f };
    const Color kLeather   = { 0.26f, 0.16f, 0.12f, 1.0f };
    const Color kBottle    = { 0.22f, 0.45f, 0.32f, 1.0f };

    const LairRoomType kRoomTypes[] = {
        { .key = "bunks", .name = "Bunks", .description = "Where the crew sleep. Each bed lets you keep one more man.",
          .icon = "ui/icons/room_bunks.tex", .minShort = 3, .minLong = 3, .costPerTile = 15,
          .floorTint = { 0.36f, 0.42f, 0.50f, 1.0f }, .required = { { "bunk", 1 } } },
        { .key = "bar", .name = "Bar", .description = "Somewhere to drink and talk. Crew off work come here.",
          .icon = "ui/icons/room_bar.tex", .minShort = 3, .minLong = 4, .costPerTile = 25,
          .floorTint = { 0.52f, 0.36f, 0.22f, 1.0f }, .required = { { "counter", 1 }, { "stool", 1 } } },
        { .key = "storage", .name = "Storage", .description = "Shelves hold more crates from the warehouse.",
          .icon = "ui/icons/room_storage.tex", .minShort = 2, .minLong = 3, .costPerTile = 10,
          .floorTint = { 0.46f, 0.44f, 0.30f, 1.0f }, .required = { { "shelf", 1 } } },
        { .key = "armoury", .name = "Armoury", .description = "Guns and the bench to keep them working.",
          .icon = "ui/icons/room_armoury.tex", .minShort = 3, .minLong = 3, .costPerTile = 30,
          .floorTint = { 0.34f, 0.38f, 0.32f, 1.0f }, .required = { { "gun_rack", 1 } } },
        { .key = "office", .name = "Office", .description = "The boss's own room. One only. He sits at his desk.",
          .icon = "ui/icons/room_office.tex", .minShort = 3, .minLong = 4, .costPerTile = 40, .unique = true,
          .floorTint = { 0.44f, 0.24f, 0.26f, 1.0f }, .required = { { "boss_desk", 1 }, { "boss_chair", 1 } } },
        { .key = "vault", .name = "Vault", .description = "Safes protect money from a raid.",
          .icon = "ui/icons/room_vault.tex", .minShort = 2, .minLong = 2, .costPerTile = 60,
          .floorTint = { 0.40f, 0.42f, 0.46f, 1.0f }, .required = { { "safe", 1 } } },
    };

    const LairObjectType kObjectTypes[] = {
        { .key = "bunk", .name = "Bunk", .description = "A bed for one more man.", .icon = "ui/icons/obj_bunk.tex", .room = "bunks",
          .cost = 120, .buildSeconds = 4.0f, .height = 1.7f,
          .parts = { { "meshes/objects/bunk_frame.mesh", kSteel }, { "meshes/objects/bunk_mattress.mesh", kOlive }, { "meshes/objects/bunk_pillow.mesh", kLinen } },
          .crewBeds = 1, .useSeconds = 8.0f },
        { .key = "footlocker", .name = "Footlocker", .description = "A man's things. Makes the place his.", .icon = "ui/icons/obj_footlocker.tex", .room = "bunks",
          .cost = 40, .buildSeconds = 2.0f, .height = 0.6f,
          .parts = { { "meshes/objects/footlocker_box.mesh", kOlive }, { "meshes/objects/footlocker_trim.mesh", kBrass } },
          .useSeconds = 4.0f },

        { .key = "counter", .name = "Bar counter", .description = "The bar itself. Two tiles long.", .icon = "ui/icons/obj_counter.tex", .room = "bar",
          .width = 2, .cost = 300, .buildSeconds = 7.0f, .height = 1.5f,
          .parts = { { "meshes/objects/counter_body.mesh", kDarkWood }, { "meshes/objects/counter_top.mesh", kPaleWood }, { "meshes/objects/counter_bottles.mesh", kBottle } },
          .useSeconds = 10.0f },
        { .key = "stool", .name = "Bar stool", .description = "Somewhere to lean. Doesn't block the way.", .icon = "ui/icons/obj_stool.tex", .room = "bar",
          .cost = 40, .buildSeconds = 2.0f, .solid = false, .height = 0.8f,
          .parts = { { "meshes/objects/stool_legs.mesh", kSteel }, { "meshes/objects/stool_seat.mesh", kLeather } },
          .useSeconds = 12.0f, .seat = true },
        { .key = "table", .name = "Card table", .description = "For a quiet game.", .icon = "ui/icons/obj_table.tex", .room = "bar",
          .cost = 90, .buildSeconds = 3.0f, .height = 0.9f,
          .parts = { { "meshes/objects/table_legs.mesh", kDarkWood }, { "meshes/objects/table_top.mesh", kGreenFelt } },
          .useSeconds = 10.0f },

        { .key = "shelf", .name = "Shelf", .description = "Holds 10 more crates.", .icon = "ui/icons/obj_shelf.tex", .room = "storage",
          .cost = 100, .buildSeconds = 3.0f, .height = 2.1f,
          .parts = { { "meshes/objects/shelf_frame.mesh", kSteel }, { "meshes/objects/shelf_boxes.mesh", kCardboard } },
          .crateCapacity = 10, .useSeconds = 5.0f },

        { .key = "gun_rack", .name = "Gun rack", .description = "Tommy guns, oiled and counted.", .icon = "ui/icons/obj_gun_rack.tex", .room = "armoury",
          .cost = 350, .buildSeconds = 5.0f, .height = 1.9f,
          .parts = { { "meshes/objects/gun_rack_frame.mesh", kWood }, { "meshes/objects/gun_rack_guns.mesh", kGunmetal } },
          .useSeconds = 6.0f },
        { .key = "workbench", .name = "Workbench", .description = "Vice and tools. Two tiles long.", .icon = "ui/icons/obj_workbench.tex", .room = "armoury",
          .width = 2, .cost = 250, .buildSeconds = 5.0f, .height = 1.2f,
          .parts = { { "meshes/objects/workbench_body.mesh", kWood }, { "meshes/objects/workbench_tools.mesh", kGunmetal } },
          .useSeconds = 10.0f },

        { .key = "boss_desk", .name = "Boss's desk", .description = "Where the family's business gets done. Two tiles long.", .icon = "ui/icons/obj_boss_desk.tex", .room = "office",
          .width = 2, .cost = 600, .buildSeconds = 7.0f, .height = 1.1f,
          .parts = { { "meshes/objects/boss_desk_body.mesh", kMahogany }, { "meshes/objects/boss_desk_top.mesh", kGreenFelt }, { "meshes/objects/boss_desk_lamp.mesh", kBrass } } },
        { .key = "boss_chair", .name = "Boss's chair", .description = "Don Carlo sits here when the office works.", .icon = "ui/icons/obj_boss_chair.tex", .room = "office",
          .cost = 150, .buildSeconds = 3.0f, .solid = false, .height = 1.3f,
          .parts = { { "meshes/objects/boss_chair_frame.mesh", kGunmetal }, { "meshes/objects/boss_chair_cushion.mesh", kMahogany } },
          .seat = true, .bossSeat = true },

        { .key = "safe", .name = "Safe", .description = "Protects $5,000 from a raid.", .icon = "ui/icons/obj_safe.tex", .room = "vault",
          .cost = 800, .buildSeconds = 8.0f, .height = 1.3f,
          .parts = { { "meshes/objects/safe_body.mesh", kGunmetal }, { "meshes/objects/safe_dial.mesh", kBrass } },
          .protectedMoney = 5000 },
    };
}

std::span<const LairRoomType> GetLairRoomTypes()
{
    return kRoomTypes;
}

std::span<const LairObjectType> GetLairObjectTypes()
{
    return kObjectTypes;
}
