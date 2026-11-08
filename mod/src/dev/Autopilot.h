#pragma once

// Dev builds only: plays the test missions by itself, each one step after the other from its sheet (Sheets.cpp).
namespace Autopilot {
    // Logs what it sees when that changes, then decides and does the next step once the player has control; call it every frame on the game thread.
    void Update();
    // Pauses the autopilot so the player can play a part themselves, or resumes it.
    void TogglePause();
    // Starts a new game from the main menu once the room is connected, like clicking Run Archipelago; call it every frame of the menu.
    void UpdateMenu();
    // Listens to mission starts (to skip the ones before start_at) and starts the watch thread (start-up videos, focus, frozen game); call it once, from DllMain.
    void Install();
}
