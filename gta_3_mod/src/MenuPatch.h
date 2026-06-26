#pragma once

namespace MenuPatch {
    // Patches aScreens at runtime to insert our entries.
    // Safe to call once at ASI load time.
    bool Apply();
    bool QueueIntroGmlFallbackMissionLaunch();
    void BeginFrame();
    void EndFrame();
    // Classic-marker mode: per-frame proximity trigger. Standing on a contact
    // point that has a playable mission launches it through the proven
    // LaunchMission pipeline (the vanilla trigger loops stay terminated).
    void TickMarkerLaunch();
}
