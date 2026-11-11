#pragma once
#include <string>

// Dev builds only: GTA3Archipelago-dev.ini next to the ASI, one key=value per line; a missing file or key keeps the default.
namespace Settings {
    // Reads the file once, from DllMain, and logs what it found.
    void Load(const std::string& path);
    // start_at: the test mission to start the run at, like "luigi3"; the ones before it are forced to pass. Default: the first one.
    const std::string& StartAt();
    // quit_on_stuck=1: when the autopilot is stuck, write why and quit the game instead of pausing. Default: off.
    bool QuitOnStuck();
    // quit_when_done=1: quit the game once every test mission is passed. Default: off.
    bool QuitWhenDone();
    // run_in_background=1: keep the game running when its window loses the focus, as GTA III otherwise stops. Default: off.
    bool RunInBackground();
    // mute=1: silence the game's sound effects and music, without changing the volumes saved in gta3.set. Default: off.
    bool Mute();
    // play=0: the autopilot only skips the missions before start_at and gives the player control back, then lets them play by hand. Default: on.
    bool Play();
    // skip_jobs=1: the run leaves out the vehicle jobs (Taxi Driver, Vigilante, Firefighter, Paramedic), which are no Archipelago locations yet. Default: off.
    bool SkipJobs();
    // skip=rc2,ambulan: sheets the run leaves out, while their automation is being fixed. Default: none.
    bool Skips(const std::string& script);
}
