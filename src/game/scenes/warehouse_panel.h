#pragma once

struct GameState;
class Ui;

// The warehouse view's panel: the truck's state, the crate count, who is carrying, orders.
// Immediate mode: call from the scene's Update, before it uses the mouse on the world.
void DrawWarehousePanel(Ui& ui, GameState& state);
