#pragma once

class Ui;
struct GameState;

// The screens that stop the game: "You got away" and "Busted". Call after everything else's UI: it
// covers the whole window and takes the mouse from it. No simulation runs while one is up.
void DrawLawScreen(Ui& ui, GameState& state);
