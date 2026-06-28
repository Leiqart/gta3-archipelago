#include "MenuPatch.h"

#include <windows.h>
#include <xinput.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ApState.h"
#include "Config.h"
#include "GameStructs.h"
#include "Hooks.h"
#include "Logger.h"
#include "ApBridge.h"
#include "PlayerRuntime.h"
#include "PluginPaths.h"
#include "RunState.h"
#include "ScriptRuntime.h"
#include "TextOverrides.h"

namespace {
    constexpr char kNewGameEntryKey[] = "FE_ARCH";
    constexpr char kPauseEntryKey[]   = "FE_RUNM";
    // Vanilla entry we strip so the player can't start a new run from the pause
    // menu while an Archipelago run is already active.
    constexpr char kStartGameKey[]    = "FEN_STA";
    constexpr char kBusyKey[]         = "APBUSY";
    constexpr char kRedoKey[]         = "APREDO";
    constexpr char kQuitKey[]         = "APQUIT";
    constexpr char kPortlandKey[]     = "APPORT";
    constexpr char kStauntonKey[]     = "APSTAUN";
    constexpr char kShoresideKey[]    = "APSHORE";
    constexpr char kSideKey[]         = "APSIDE";
    constexpr char kIntroKey[]        = "APINTRO";
    // Retry/fallback for the vanilla EIGHT block. Despite the legacy variant
    // name, this runs Give Me Liberty and then continues into Luigi's Girls.
    constexpr ScriptRuntime::VisibleMissionEntry kGiveMeLibertyRedoMission = {
        21, "EIGHT", "APGMLIB", ScriptRuntime::MissionBucket::Intro, false,
        ScriptRuntime::MissionLaunchVariant::GiveMeLibertySplit, "APGMLIB"
    };

    enum class MissionRegion : std::uint8_t {
        Portland = 0,
        Staunton,
        Shoreside,
        Side,
    };

    enum class ListMode : std::uint8_t {
        Groups = 0,
        Missions,
    };

    enum class PendingMissionAction : std::uint8_t {
        None = 0,
        Redo,
        Quit,
    };

    struct MissionGroupDef {
        MissionRegion region;
        ScriptRuntime::MissionBucket bucket;
        const char* key;
    };

    constexpr MissionGroupDef kMissionGroups[] = {
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Intro, kIntroKey},
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Luigi, "APLUIGI"},
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Joey, "APJOEY"},
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Toni, "APTONI"},
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Frankie, "APFRANK"},
        {MissionRegion::Portland, ScriptRuntime::MissionBucket::Diablo, "APDIABL"},
        {MissionRegion::Staunton, ScriptRuntime::MissionBucket::Asuka, "APASUKA"},
        {MissionRegion::Staunton, ScriptRuntime::MissionBucket::Kenji, "APKENJI"},
        {MissionRegion::Staunton, ScriptRuntime::MissionBucket::Ray, "APRAY"},
        {MissionRegion::Staunton, ScriptRuntime::MissionBucket::DonaldLove, "APLOVE"},
        {MissionRegion::Staunton, ScriptRuntime::MissionBucket::Yardie, "APYARD"},
        {MissionRegion::Shoreside, ScriptRuntime::MissionBucket::Hood, "APHOOD"},
        {MissionRegion::Shoreside, ScriptRuntime::MissionBucket::Catalina, "APCATAL"},
        {MissionRegion::Shoreside, ScriptRuntime::MissionBucket::AsukaSuburban, "APASUB"},
        {MissionRegion::Side, ScriptRuntime::MissionBucket::Rc, "APRCMIS"},
        {MissionRegion::Side, ScriptRuntime::MissionBucket::OffRoad, "AP4X4"},
        {MissionRegion::Side, ScriptRuntime::MissionBucket::MeatFactory, "APMEAT"},
        {MissionRegion::Side, ScriptRuntime::MissionBucket::Mayhem, "APMAYHM"},
    };

    struct FrameState {
        MenuPage beforePage            = MenuPage::None;
        std::int32_t beforeEntry       = 0;
        char beforeKey[9]              = {};
        // Resolved key of the mission-list entry actually selected AFTER the
        // engine processed input this frame (m_nCurrOption post-g_origProcess).
        // beforeKey is snapshotted in BeginFrame, before the engine moves the
        // highlight; a mouse click moves+activates the clicked entry within the
        // same process call, so only this post-process value reflects what the
        // user really clicked. Updated every frame the list stays open.
        char missionListSelectedKey[9] = {};
        // Same post-process selection capture, for the Archipelago ROOT page
        // (REDO/QUIT/BUSY entries) — resolves the entry the mouse actually
        // clicked rather than the stale BeginFrame snapshot.
        char rootSelectedKey[9]        = {};
        // Dwell tracking: how many consecutive frames the current page has been
        // shown. A confirm release only counts as a selection once the page has
        // settled (>= kSelectDwellFrames), so the click that NAVIGATED into a
        // page (its release lands a frame or two after the page appears) can
        // never be mistaken for selecting an entry on the new page.
        MenuPage dwellPage             = MenuPage::None;
        int dwellFrames                = 0;
        CMenuEntry normalStartEntry    = {};
        bool hasNormalStartEntry       = false;
        bool runStartSelectionActive   = false;
        int navigationCooldownFrames   = 0;
        MenuPage parentPage            = MenuPage::PauseMenu;
        MissionRegion selectedRegion   = MissionRegion::Portland;
        ListMode selectedListMode      = ListMode::Groups;
        ScriptRuntime::MissionBucket selectedBucket = ScriptRuntime::MissionBucket::Luigi;
        const ScriptRuntime::VisibleMissionEntry* pendingMissionLaunch = nullptr;
        const ScriptRuntime::VisibleMissionEntry* pendingMissionActionEntry = nullptr;
        PendingMissionAction pendingMissionAction = PendingMissionAction::None;
        bool installed                 = false;
        bool pauseMissionListConfirmThisFrame = false;
        // Same release-edge signal as the mission list, but for the Archipelago
        // ROOT page (REDO/QUIT entries). Requiring a confirm press+release while
        // on page 57 — instead of "a confirm key is held now" — stops the click
        // that OPENED the page (Enter/LMB still down as 52->57 lands) from
        // instantly arming REDO and relaunching the active mission.
        bool pauseRootConfirmThisFrame = false;
        // Frames remaining where a recent back/cancel (ESC etc.) press
        // suppresses any mission launch. Set while on an Archipelago page and
        // back is held; decays so it still covers the frame the frontend
        // actually closes. Guarantees "ESC never launches a mission".
        int menuBackLatchFrames = 0;
        // Sticky latch: once the user has lined up FE_ARCH on the NewGame
        // page (BeginFrame sets pendingRunStart TRUE), keep this set even if
        // beforePage no longer reads as NewGame on a later frame. EndFrame
        // consumes it as soon as the engine has navigated to any non-menu/
        // non-Archipelago page, sidestepping the timing race where the menu
        // engine processes the click between two BeginFrame snapshots.
        bool runStartLatched           = false;
    };

    FrameState g_state;
    bool g_launchFromClassicMarker = false;

    struct ScopedClassicMarkerLaunch {
        ScopedClassicMarkerLaunch() {
            g_launchFromClassicMarker = true;
        }
        ~ScopedClassicMarkerLaunch() {
            g_launchFromClassicMarker = false;
        }
    };

    // Number of consecutive frames a page must have been shown before a confirm
    // PRESS on it counts as selecting an entry. The GTA3 frontend acts on the
    // mouse button PRESS (not release), and closes the page on that press, so we
    // arm on the press edge. A press edge on the child page is already a fresh,
    // deliberate click (the click that navigated in was pressed on the PARENT
    // page); this small dwell only skips any transient on the entry frame.
    constexpr int kSelectDwellFrames = 3;

    // Tracks the combined confirm input (Enter / LMB / gamepad A) across frames.
    struct MenuConfirmState {
        bool initialized = false;
        bool down = false;
    };

    MenuConfirmState g_menuConfirm;

    using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

    XInputGetStateFn ResolveXInputGetState() {
        static XInputGetStateFn fn = []() -> XInputGetStateFn {
            const char* dlls[] = {
                "xinput1_4.dll",
                "xinput9_1_0.dll",
                "xinput1_3.dll",
            };
            for (const char* dll : dlls) {
                if (HMODULE module = LoadLibraryA(dll)) {
                    if (auto proc = reinterpret_cast<XInputGetStateFn>(
                            GetProcAddress(module, "XInputGetState"))) {
                        return proc;
                    }
                }
            }
            return nullptr;
        }();
        return fn;
    }

    bool IsAnyGamepadConfirmDown() {
        const XInputGetStateFn xinputGetState = ResolveXInputGetState();
        if (!xinputGetState) {
            return false;
        }

        XINPUT_STATE state{};
        for (DWORD userIndex = 0; userIndex < XUSER_MAX_COUNT; ++userIndex) {
            if (xinputGetState(userIndex, &state) == ERROR_SUCCESS &&
                (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0) {
                return true;
            }
        }
        return false;
    }

    bool IsAnyGamepadBackDown() {
        const XInputGetStateFn xinputGetState = ResolveXInputGetState();
        if (!xinputGetState) {
            return false;
        }
        XINPUT_STATE state{};
        for (DWORD userIndex = 0; userIndex < XUSER_MAX_COUNT; ++userIndex) {
            if (xinputGetState(userIndex, &state) == ERROR_SUCCESS &&
                (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0) {
                return true;
            }
        }
        return false;
    }

    // Back / cancel input: ESC, Backspace, right-mouse, or gamepad B. Used to
    // guarantee that leaving a menu via "back" NEVER launches a mission, even
    // if some confirm key state lingers as the frontend shuts down.
    bool IsMenuBackDownNow() {
        return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 ||
               (GetAsyncKeyState(VK_BACK) & 0x8000) != 0 ||
               (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
               IsAnyGamepadBackDown();
    }

    // True on the frame the combined confirm input (Enter / LMB / gamepad A) is
    // pressed (up->down). The GTA3 frontend acts on the button PRESS and closes
    // the page on it, so a selection (mission entry, REDO/QUIT) is detected on
    // the press edge. Callers additionally require the page to have settled
    // (dwell) and no back/cancel in flight, so the click that opened the page
    // never selects on it, and ESC never launches.
    bool PollMenuConfirmPressed() {
        const bool down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
                          (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
                          IsAnyGamepadConfirmDown();
        const bool pressed = g_menuConfirm.initialized && !g_menuConfirm.down && down;
        g_menuConfirm.initialized = true;
        g_menuConfirm.down = down;
        return pressed;
    }

    template <typename T>
    bool WriteProtected(T* dst, const T& src) {
        DWORD oldProtect = 0;
        if (!VirtualProtect(dst, sizeof(T), PAGE_READWRITE, &oldProtect)) {
            Logger::Log("VirtualProtect RW failed at %p (err=%lu)", dst, GetLastError());
            return false;
        }
        *dst = src;
        DWORD tmp = 0;
        VirtualProtect(dst, sizeof(T), oldProtect, &tmp);
        return true;
    }

    void CopyKey(char (&dst)[8], const char* src) {
        std::memset(dst, 0, sizeof(dst));
        std::memcpy(dst, src, std::min<std::size_t>(std::strlen(src), sizeof(dst)));
    }

    bool KeyEquals(const char* lhs, const char* rhs) {
        return std::strncmp(lhs, rhs, 8) == 0;
    }

    bool IsArchipelagoPage(MenuPage page) {
        return page == MenuPage::Archipelago ||
               page == MenuPage::ArchipelagoMissions ||
               page == MenuPage::ArchipelagoMissionList;
    }

    int FindEntryByKey(const CMenuScreen* page, const char* key) {
        for (int i = 0; i < 18; ++i) {
            if (KeyEquals(page->m_aEntries[i].m_EntryName, key)) {
                return i;
            }
        }
        return -1;
    }

    int FindFirstEmptyEntry(const CMenuScreen* page) {
        for (int i = 0; i < 18; ++i) {
            const CMenuEntry& entry = page->m_aEntries[i];
            if (entry.m_nAction == MENUACTION_NOTHING && entry.m_EntryName[0] == 0) {
                return i;
            }
        }
        return -1;
    }

    // Remove the entry whose GXT key matches, compacting the slots above it up
    // by one and clearing the now-duplicated tail. Idempotent: a missing key
    // is treated as already-hidden (success). Mirrors the shift logic used by
    // InsertArchipelagoIntoPauseMenu, in reverse.
    bool RemoveEntryByKey(MenuPage pageId, const char* key, const char* label) {
        CMenuScreen* page = Screens::Page(pageId);
        const int idx = FindEntryByKey(page, key);
        if (idx < 0) {
            return true;
        }

        CMenuScreen patched = *page;
        for (int slot = idx; slot < 17; ++slot) {
            patched.m_aEntries[slot] = patched.m_aEntries[slot + 1];
        }
        patched.m_aEntries[17] = CMenuEntry{};
        if (!WriteProtected(page, patched)) {
            return false;
        }

        Logger::Log("Menu patch: hid '%s' (%s) on page %d (was slot %d)",
                    key, label, static_cast<int>(pageId), idx);
        return true;
    }

    CMenuEntry MakeMenuEntry(const char* key, MenuPage target) {
        CMenuEntry entry{};
        entry.m_nAction = MENUACTION_CHANGEMENU;
        CopyKey(entry.m_EntryName, key);
        entry.m_nSaveSlot = 0;
        entry.m_nTargetMenu = static_cast<std::int32_t>(target);
        return entry;
    }

    CMenuEntry MakeActionEntry(const char* key, MenuAction action, MenuPage target = MenuPage::None) {
        CMenuEntry entry{};
        entry.m_nAction = action;
        CopyKey(entry.m_EntryName, key);
        entry.m_nSaveSlot = 0;
        entry.m_nTargetMenu = static_cast<std::int32_t>(target);
        return entry;
    }

    CMenuEntry CloneEntryWithKey(const CMenuEntry& source, const char* key) {
        CMenuEntry entry = source;
        CopyKey(entry.m_EntryName, key);
        return entry;
    }

    void BuildPageHeader(CMenuScreen* page, const char* titleKey, MenuPage previousPage) {
        CopyKey(page->m_ScreenName, titleKey);
        page->m_nPreviousPage = static_cast<std::int32_t>(previousPage);
        page->m_nPreviousGamePage = static_cast<std::int32_t>(g_state.parentPage);
        page->m_nParentEntry = 0;
        page->m_nParentGameEntry = 0;
    }

    bool HasMissionRuntime() {
        return ScriptRuntime::HasLiveScriptEngine();
    }

    bool CanLaunchFromPause() {
        return g_state.parentPage == MenuPage::PauseMenu && HasMissionRuntime();
    }

    bool CanLaunchFromNewGame() {
        return g_state.parentPage == MenuPage::NewGame && g_state.hasNormalStartEntry;
    }

    bool HasCurrentMissionContext() {
        const int actualMissionIndex = RunState::LastLaunchedMission();
        return actualMissionIndex >= 0 &&
               (ScriptRuntime::AlreadyRunningAMissionScript() ||
                ScriptRuntime::IsPlayerOnMission());
    }

    bool HasPendingMissionLaunchContext() {
        return PlayerRuntime::IsIntroSequenceLaunchPending();
    }

    const ScriptRuntime::VisibleMissionEntry* FindCurrentMissionEntry() {
        if (PlayerRuntime::IsGiveMeLibertyPhaseActive()) {
            return &kGiveMeLibertyRedoMission;
        }
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        if (const ScriptRuntime::VisibleMissionEntry* syntheticEntry =
                ScriptRuntime::FindVisibleMissionBySyntheticValidationKey(syntheticKey)) {
            return syntheticEntry;
        }
        const int actualMissionIndex = RunState::LastLaunchedMission();
        if (actualMissionIndex < 0) {
            return nullptr;
        }
        return ScriptRuntime::FindVisibleMissionByActualIndex(actualMissionIndex);
    }

    const char* RegionKey(MissionRegion region) {
        switch (region) {
            case MissionRegion::Portland:
                return kPortlandKey;
            case MissionRegion::Staunton:
                return kStauntonKey;
            case MissionRegion::Shoreside:
                return kShoresideKey;
            case MissionRegion::Side:
            default:
                return kSideKey;
        }
    }

    bool TryGetRegionForKey(const char* key, MissionRegion* outRegion) {
        if (KeyEquals(key, kPortlandKey)) {
            *outRegion = MissionRegion::Portland;
            return true;
        }
        if (KeyEquals(key, kStauntonKey)) {
            *outRegion = MissionRegion::Staunton;
            return true;
        }
        if (KeyEquals(key, kShoresideKey)) {
            *outRegion = MissionRegion::Shoreside;
            return true;
        }
        if (KeyEquals(key, kSideKey)) {
            *outRegion = MissionRegion::Side;
            return true;
        }
        return false;
    }

    int RootEntryIndexForRegion(MissionRegion region) {
        int entryIndex = 0;
        for (MissionRegion candidate : {
                 MissionRegion::Portland,
                 MissionRegion::Staunton,
                 MissionRegion::Shoreside,
                 MissionRegion::Side,
             }) {
            if (candidate == region) {
                return entryIndex;
            }
            ++entryIndex;
        }
        return 0;
    }

    const char* GroupKey(ScriptRuntime::MissionBucket bucket) {
        for (const MissionGroupDef& group : kMissionGroups) {
            if (group.bucket == bucket) {
                return group.key;
            }
        }
        return kPauseEntryKey;
    }

    bool TryGetBucketForKey(const char* key, ScriptRuntime::MissionBucket* outBucket) {
        for (const MissionGroupDef& group : kMissionGroups) {
            if (KeyEquals(key, group.key)) {
                *outBucket = group.bucket;
                return true;
            }
        }
        return false;
    }

    // A region is shown in the root page only when at least one of its contacts
    // has a visible (compiled + character-unlocked) mission.
    bool RegionHasVisibleMissions(MissionRegion region) {
        for (const MissionGroupDef& group : kMissionGroups) {
            if (group.region == region &&
                ScriptRuntime::VisibleMissionCount(group.bucket) > 0) {
                return true;
            }
        }
        return false;
    }

    // A region is "open" once Archipelago granted its island-unlock item (or
    // Portland, the start island, which is always open). This makes the region
    // enterable in RUN MISSION even before any of its contacts are unlocked, so
    // the player can see its (locked) contacts.
    bool RegionOpenedByApItem(MissionRegion region) {
        switch (region) {
            case MissionRegion::Portland:  return true;
            case MissionRegion::Staunton:  return ApState::HasItem("unlock_commercial");
            case MissionRegion::Shoreside: return ApState::HasItem("unlock_suburban");
            default:                       return false;
        }
    }

    // True when the region is enterable: it has an unlocked (character-unlocked)
    // compiled contact, OR Archipelago opened the island. Drives the region's
    // (O) available vs (X) locked marker and its clickability.
    bool RegionHasUnlockedMissions(MissionRegion region) {
        if (RegionOpenedByApItem(region)) {
            return true;
        }
        for (const MissionGroupDef& group : kMissionGroups) {
            if (group.region == region &&
                ScriptRuntime::HasUnlockedMissionInBucket(group.bucket) &&
                ScriptRuntime::VisibleMissionCount(group.bucket) > 0) {
                return true;
            }
        }
        return false;
    }

    // True when every compiled mission of a contact (bucket) is validated, and
    // it has at least one. Drives the contact's (V) marker.
    bool BucketAllValidated(ScriptRuntime::MissionBucket bucket) {
        int count = 0;
        for (const ScriptRuntime::VisibleMissionEntry& entry : ScriptRuntime::kVisibleMissions) {
            if (entry.bucket != bucket || !ScriptRuntime::IsVisibleMissionAvailable(entry)) {
                continue;
            }
            ++count;
            if (!ScriptRuntime::IsMissionEntryValidated(entry)) {
                return false;
            }
        }
        return count > 0;
    }

    // True when every contact in the region has all its missions validated, and
    // the region has at least one compiled contact. Drives the region (V) marker.
    bool RegionAllValidated(MissionRegion region) {
        int count = 0;
        for (const MissionGroupDef& group : kMissionGroups) {
            if (group.region != region ||
                ScriptRuntime::VisibleMissionCount(group.bucket) <= 0) {
                continue;
            }
            ++count;
            if (!BucketAllValidated(group.bucket)) {
                return false;
            }
        }
        return count > 0;
    }

    // Resolve a marker state for an unlockable item: X locked, V fully
    // validated, O (available) otherwise.
    char MarkerState(bool unlocked, bool allValidated) {
        if (!unlocked) {
            return 'X';
        }
        return allValidated ? 'V' : 'A';
    }

    // Add a region entry marked (O) when it has an unlocked contact, (V) when
    // every mission under it is validated, else (X).
    void AddRegionEntry(CMenuScreen* page, int& entryIndex, MissionRegion region,
                        const char* regionKey) {
        if (!RegionHasVisibleMissions(region)) {
            return;
        }
        const bool unlocked = RegionHasUnlockedMissions(region);
        char markedKey[9] = {};
        TextOverrides::RegisterMarked(markedKey, regionKey,
                                      MarkerState(unlocked, RegionAllValidated(region)));
        // A locked region is shown (with its X marker) but not enterable: point
        // it back at its own page so a click is a no-op.
        const MenuPage target =
            unlocked ? MenuPage::ArchipelagoMissions : MenuPage::Archipelago;
        page->m_aEntries[entryIndex++] = MakeMenuEntry(markedKey, target);
    }

    bool RefreshMissionRootPage() {
        CMenuScreen page{};
        BuildPageHeader(&page, kPauseEntryKey, g_state.parentPage);

        int entryIndex = 0;
        if (!HasMissionRuntime()) {
            page.m_aEntries[entryIndex++] = MakeActionEntry(kBusyKey, MENUACTION_RESUME_FROM_SAVEZONE);
        } else if (g_state.parentPage == MenuPage::PauseMenu && HasPendingMissionLaunchContext()) {
            page.m_aEntries[entryIndex++] = MakeActionEntry(kBusyKey, MENUACTION_RESUME_FROM_SAVEZONE);
        } else if (g_state.parentPage == MenuPage::PauseMenu && HasCurrentMissionContext()) {
            page.m_aEntries[entryIndex++] = MakeActionEntry(kRedoKey, MENUACTION_RESUME_FROM_SAVEZONE);
            page.m_aEntries[entryIndex++] = MakeActionEntry(kQuitKey, MENUACTION_RESUME_FROM_SAVEZONE);
        } else {
            AddRegionEntry(&page, entryIndex, MissionRegion::Portland, kPortlandKey);
            AddRegionEntry(&page, entryIndex, MissionRegion::Staunton, kStauntonKey);
            AddRegionEntry(&page, entryIndex, MissionRegion::Shoreside, kShoresideKey);
            AddRegionEntry(&page, entryIndex, MissionRegion::Side, kSideKey);
        }

        page.m_aEntries[entryIndex++] = MakeMenuEntry("FEDS_TB", g_state.parentPage);
        return WriteProtected(Screens::Page(MenuPage::Archipelago), page);
    }

    bool IsBucketLaunchableFromContext(ScriptRuntime::MissionBucket bucket) {
        (void)bucket;
        return true;
    }

    bool RefreshMissionGroupsPage() {
        CMenuScreen page{};

        BuildPageHeader(&page, RegionKey(g_state.selectedRegion), MenuPage::Archipelago);

        int entryIndex = 0;
        for (const MissionGroupDef& group : kMissionGroups) {
            if (group.region != g_state.selectedRegion) {
                continue;
            }
            if (!IsBucketLaunchableFromContext(group.bucket)) {
                continue;
            }
            if (ScriptRuntime::VisibleMissionCount(group.bucket) <= 0) {
                continue;
            }
            if (entryIndex >= 17) {
                break;
            }
            const bool contactUnlocked = Config::UnlockMissionsAtomically()
                ? ScriptRuntime::HasUnlockedMissionInBucket(group.bucket)
                : ScriptRuntime::IsCharacterUnlocked(group.bucket);
            char contactKey[9] = {};
            TextOverrides::RegisterMarked(contactKey, group.key,
                                          MarkerState(contactUnlocked, BucketAllValidated(group.bucket)));
            // Locked contacts are shown (X) but not enterable: self-target no-op.
            const MenuPage contactTarget =
                contactUnlocked ? MenuPage::ArchipelagoMissionList : MenuPage::ArchipelagoMissions;
            page.m_aEntries[entryIndex++] = MakeMenuEntry(contactKey, contactTarget);
        }

        page.m_aEntries[entryIndex++] = MakeMenuEntry("FEDS_TB", MenuPage::Archipelago);
        Logger::Log("Mission group list built: region=%d entries=%d",
                    static_cast<int>(g_state.selectedRegion), entryIndex);
        return WriteProtected(Screens::Page(MenuPage::ArchipelagoMissions), page);
    }

    bool RefreshMissionListPage() {
        CMenuScreen page{};
        BuildPageHeader(&page, GroupKey(g_state.selectedBucket), MenuPage::ArchipelagoMissions);

        int entryIndex = 0;
        const int missionCount = ScriptRuntime::VisibleMissionCount(g_state.selectedBucket);
        const bool launchFromPause = CanLaunchFromPause();

        for (int visibleIndex = 0; visibleIndex < missionCount && entryIndex < 17; ++visibleIndex) {
            const int actualIndex = ScriptRuntime::VisibleToActualMissionIndex(visibleIndex, g_state.selectedBucket);
            if (actualIndex < 0) {
                continue;
            }

            char key[9] = {};
            if (!ScriptRuntime::TryGetMissionDisplayKey(visibleIndex, key, g_state.selectedBucket)) {
                if (!ScriptRuntime::TryGetMissionDisplayKeyByActualIndex(actualIndex, key)) {
                    continue;
                }
            }

            // Show every mission; convey its state with a marker on the label via
            // an "APK<state>" key the CText hook composes against the real mission
            // name. (X) = locked (character not unlocked), (V) = validated
            // (completed), (O) = available but not yet validated, (*) = bugged
            // (validated by default).
            const ScriptRuntime::VisibleMissionEntry* entry =
                ScriptRuntime::FindVisibleMissionByVisibleIndex(visibleIndex, g_state.selectedBucket);
            const bool missionUnlocked = entry && ScriptRuntime::IsMissionEntryUnlocked(*entry);
            const char markerState =
                (entry && entry->bugged)
                    ? '*'
                    : MarkerState(missionUnlocked,
                                  entry && ScriptRuntime::IsMissionEntryValidated(*entry));
            char markedKey[9] = {};
            TextOverrides::RegisterMarked(markedKey, key, markerState);

            const bool bugged = entry && entry->bugged;
            if (bugged || !missionUnlocked) {
                // Bugged missions are shown (B) but not launchable: self-target
                // the current page so a click is a no-op, and never arm a launch.
                page.m_aEntries[entryIndex++] =
                    MakeMenuEntry(markedKey, MenuPage::ArchipelagoMissionList);
            } else {
                // From the pause menu, click requests frontend shutdown directly
                // (action 49) instead of routing through MENUACTION_RESUME. That
                // keeps mission selection distinct from ESC/back, which otherwise
                // shares the same "return to pause page" path from our submenu.
                // From New Game we keep CHANGEMENU as a no-op self-target.
                page.m_aEntries[entryIndex++] = launchFromPause
                    ? MakeActionEntry(markedKey, MENUACTION_RESUME_FROM_SAVEZONE)
                    : (CanLaunchFromNewGame()
                        ? CloneEntryWithKey(g_state.normalStartEntry, markedKey)
                        : MakeMenuEntry(markedKey, MenuPage::ArchipelagoMissionList));
            }
        }

        page.m_aEntries[entryIndex++] = MakeMenuEntry("FEDS_TB", MenuPage::ArchipelagoMissions);
        Logger::Log("Mission list built: bucket=%d missions=%d entries=%d launchFromPause=%d",
                    static_cast<int>(g_state.selectedBucket), missionCount, entryIndex,
                    launchFromPause ? 1 : 0);
        return WriteProtected(Screens::Page(MenuPage::ArchipelagoMissionList), page);
    }

    // Read a page entry's GXT key into `out`, resolving any synthetic "APKnnnn"
    // marker key back to its original (APPORT / APLUIGI / mission key) so region,
    // contact and mission matching keeps working. Returns false if out of range.
    bool ResolveEntryKey(MenuPage pageId, std::int32_t entry, char (&out)[9]) {
        std::memset(out, 0, sizeof(out));
        if (entry < 0 || entry >= 18) {
            return false;
        }
        const CMenuEntry& menuEntry = Screens::Page(pageId)->m_aEntries[entry];
        std::memcpy(out, menuEntry.m_EntryName, 8);
        out[8] = 0;

        const char* original = nullptr;
        char markerState = 0;
        if (TextOverrides::ResolveMarked(out, &original, &markerState)) {
            std::memset(out, 0, sizeof(out));
            std::memcpy(out, original, std::min<std::size_t>(std::strlen(original), 8));
        }
        return true;
    }

    void SnapshotSelectedKey() {
        std::memset(g_state.beforeKey, 0, sizeof(g_state.beforeKey));

        if (!IsArchipelagoPage(g_state.beforePage) && g_state.beforePage != MenuPage::NewGame) {
            return;
        }
        ResolveEntryKey(g_state.beforePage, g_state.beforeEntry, g_state.beforeKey);
    }

    void ReopenPage(MenuPage pageId, std::int32_t selectedEntry) {
        bool ok = false;
        switch (pageId) {
            case MenuPage::Archipelago:
                ok = RefreshMissionRootPage();
                break;
            case MenuPage::ArchipelagoMissions:
                ok = RefreshMissionGroupsPage();
                break;
            case MenuPage::ArchipelagoMissionList:
                ok = RefreshMissionListPage();
                // Fresh list session: drop any selection key carried over from a
                // previous bucket so a launch can never resolve against it. The
                // per-frame post-process capture in EndFrame repopulates it while
                // the list is open (the navigation cooldown blocks launches until
                // then).
                std::memset(g_state.missionListSelectedKey, 0,
                            sizeof(g_state.missionListSelectedKey));
                break;
            default:
                return;
        }
        if (!ok) {
            return;
        }

        MenuMgr::SetCurrentPage(pageId);
        MenuMgr::SetCurrentEntry(selectedEntry);
        g_state.navigationCooldownFrames = 2;
    }

    bool LoadMissionBytecode(std::int32_t fileOffset) {
        // Mirror re3's CTheScripts::SwitchToMission: read SIZE_MISSION_SCRIPT
        // bytes from data/main.scm at fileOffset directly into
        // ScriptSpace[SIZE_MAIN_SCRIPT]. StartNewScript will then execute
        // those opcodes from that memory IP.
        const std::string path = PluginPaths::InGameDir("data\\main.scm");
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            Logger::Log("LoadMissionBytecode: cannot open '%s' (err=%lu)",
                        path.c_str(), GetLastError());
            return false;
        }

        const LONG seekResult = SetFilePointer(file, fileOffset, nullptr, FILE_BEGIN);
        if (seekResult == INVALID_SET_FILE_POINTER) {
            Logger::Log("LoadMissionBytecode: seek to %d failed (err=%lu)",
                        fileOffset, GetLastError());
            CloseHandle(file);
            return false;
        }

        std::uint8_t* dest = ScriptRuntime::ScriptSpace() + ScriptRuntime::kSizeMainScript;
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(file, dest,
                                 static_cast<DWORD>(ScriptRuntime::kSizeMissionScript),
                                 &bytesRead, nullptr);
        CloseHandle(file);

        if (!ok || bytesRead == 0) {
            Logger::Log("LoadMissionBytecode: ReadFile failed (ok=%d bytes=%lu err=%lu)",
                        ok, bytesRead, GetLastError());
            return false;
        }

        Logger::Log("LoadMissionBytecode: loaded %lu bytes from offset %d into ScriptSpace[%zu]",
                    bytesRead, fileOffset, ScriptRuntime::kSizeMainScript);
        return true;
    }

    // Scratch area for our pre-launch safe-state bytecode stub. Placed just
    // before the mission slot (SIZE_MAIN_SCRIPT = 131072). The trailing
    // ~16-32KB of the main thread region is reliably padding (zeros) because
    // the compiled main.scm main section is well under 131072 bytes after
    // the global var table. The stub is 7 bytes; we round up to 16 for a
    // safety margin. Each launch overwrites the stub fresh.
    constexpr std::uint32_t kSafeStateStubOffset =
        PlayerRuntime::ScmSlots::SafeStateOffset;

    // Bytecode: MAKE_PLAYER_SAFE_FOR_CUTSCENE &528 + TERMINATE_THIS_SCRIPT.
    //   Opcode 0x03EF = MAKE_PLAYER_SAFE_FOR_CUTSCENE, 1 arg (player handle).
    //   Opcode 0x004E = TERMINATE_THIS_SCRIPT, 0 args.
    //   Arg type byte 0x02 = global var (2-byte LE offset follows).
    //   Player handle global is &528 (sourced from CREATE_PLAYER in the
    //   decompiled main_ap.scm: "&528"). Stored as 0x0210 little-endian.
    constexpr std::uint8_t kSafeStateStub[] = {
        0xEF, 0x03,             // MAKE_PLAYER_SAFE_FOR_CUTSCENE
        0x02,                   //   arg type: global var
        0x10, 0x02,             //   var offset 528 (LE)
        0x4E, 0x00,             // TERMINATE_THIS_SCRIPT
    };
    static_assert(sizeof(kSafeStateStub) == 7,
                  "Pre-launch stub bytecode size mismatch");

    // Vanilla contact loops prepare cutscene missions before
    // LOAD_AND_LAUNCH_MISSION. AP marker launches bypass those loops, so this
    // compact stub restores the same critical state for Luigi's cutscene chain:
    // MAKE_PLAYER_SAFE_FOR_CUTSCENE, black fade out, streaming off.
    constexpr std::uint32_t kVanillaPrelaunchStubOffset =
        PlayerRuntime::ScmSlots::VanillaPrelaunchOffset;
    constexpr std::uint8_t kVanillaPrelaunchStub[] = {
        0xEF, 0x03,             // MAKE_PLAYER_SAFE_FOR_CUTSCENE
        0x02, 0x10, 0x02,       //   player handle global &528
        0x69, 0x01,             // SET_FADING_COLOUR
        0x04, 0x00,             //   r = 0
        0x04, 0x00,             //   g = 0
        0x04, 0x00,             //   b = 0
        0x6A, 0x01,             // DO_FADE
        0x05, 0xDC, 0x05,       //   1500 ms
        0x04, 0x00,             //   FADE_OUT
        0xAF, 0x03,             // SWITCH_STREAMING
        0x04, 0x00,             //   OFF
        0x4E, 0x00,             // TERMINATE_THIS_SCRIPT
    };
    static_assert(sizeof(kVanillaPrelaunchStub) <= PlayerRuntime::ScmSlots::VanillaPrelaunchBytes,
                  "Vanilla prelaunch stub exceeds its slot");

    bool RunStubSynchronously(CRunningScript* stub, int maxSteps, const char* tag) {
        if (!stub) {
            Logger::Log("%s stub sync skipped: no script", tag ? tag : "SCM");
            return false;
        }

        auto processOneCommand = ScriptRuntime::ProcessOneCommand();
        int steps = 0;
        bool terminated = false;
        bool excepted = false;
        __try {
            while (steps < maxSteps) {
                ++steps;
                if (processOneCommand(stub)) {
                    terminated = true;
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            excepted = true;
        }

        Logger::Log("%s stub ran synchronously: steps=%d terminated=%d excepted=%d",
                    tag ? tag : "SCM",
                    steps,
                    terminated ? 1 : 0,
                    excepted ? 1 : 0);
        return terminated && !excepted;
    }

    bool RunSafeStateStubSynchronously(const char* reason) {
        CRunningScript* stub = PlayerRuntime::LaunchScmStub(
            kSafeStateStubOffset,
            kSafeStateStub,
            PlayerRuntime::ScmSlots::SafeStateBytes,
            "safe-state");
        if (!stub) {
            Logger::Log("Safe-state stub launch failed");
            return false;
        }
        return RunStubSynchronously(stub, 8, reason ? reason : "safe-state");
    }

    bool MissionNeedsVanillaCutscenePrelaunch(int actualMissionIndex) {
        switch (actualMissionIndex) {
            case 22: // LUIGI2 / LM2
            case 23: // LUIGI3 / LM3
            case 24: // LUIGI4 / LM4
            case 25: // LUIGI5 / LM5
                return true;
            default:
                return false;
        }
    }

    bool RunVanillaCutscenePrelaunchSynchronously(const char* key,
                                                  int actualMissionIndex) {
        CRunningScript* stub = PlayerRuntime::LaunchScmStub(
            kVanillaPrelaunchStubOffset,
            kVanillaPrelaunchStub,
            PlayerRuntime::ScmSlots::VanillaPrelaunchBytes,
            "vanilla-prelaunch");
        if (!stub) {
            Logger::Log("Vanilla cutscene prelaunch failed to start: key='%.8s' actual=%d",
                        key ? key : "",
                        actualMissionIndex);
            return false;
        }
        const bool ok = RunStubSynchronously(stub, 16, "vanilla-prelaunch");
        Logger::Log("Vanilla cutscene prelaunch applied: key='%.8s' actual=%d ok=%d",
                    key ? key : "",
                    actualMissionIndex,
                    ok ? 1 : 0);
        return ok;
    }

    bool LaunchMission(const ScriptRuntime::VisibleMissionEntry* entry) {
        if (!HasMissionRuntime()) {
            Logger::Log("Mission launch skipped: script runtime unavailable");
            return false;
        }
        if (!entry) {
            return false;
        }
        const int actualMissionIndex = entry->actualIndex;
        if (actualMissionIndex < 0) {
            return false;
        }
        // AP unlock parity with the marker mode: the pause-menu selector must
        // refuse a mission whose unlock_mission_* item has not arrived (the
        // list may still display it as upcoming).
        if (!ScriptRuntime::IsMissionEntryUnlocked(*entry)) {
            PlayerRuntime::QueueMarkerBlockedToast(entry);
            Logger::Log("Menu launch BLOCKED (AP unlock missing): key='%.8s' actual=%d",
                        entry->displayKey ? entry->displayKey : "", actualMissionIndex);
            return false;
        }

        const int compiledMissionIndex = ScriptRuntime::ResolveCompiledMissionIndex(*entry);
        const std::int32_t missionFileOffset =
            compiledMissionIndex >= 0
                ? ScriptRuntime::MissionOffset(compiledMissionIndex)
                : 0;
        if (compiledMissionIndex < 0 || missionFileOffset <= 0) {
            Logger::Log("Mission launch rejected: key='%.8s' actual=%d compiled=%d offset=%d",
                        entry->displayKey ? entry->displayKey : "",
                        actualMissionIndex,
                        compiledMissionIndex,
                        missionFileOffset);
            return false;
        }

        // Mirror re3's CTheScripts::SwitchToMission: for every active
        // m_bIsMissionScript that still has deatharrest enabled, unwind the
        // stack, set m_bDeatharrestExecuted=true, and loop ProcessOneCommand
        // until the script terminates itself. That runs the script's own
        // deatharrest cleanup which releases peds, vehicles, blips, timers
        // it owns before our new mission overwrites ScriptSpace[131072..].
        const int aborted = ScriptRuntime::ProperAbortMissionScripts();
        if (aborted > 0) {
            Logger::Log("Properly aborted %d active mission script(s) before launch", aborted);
        }
        ScriptRuntime::SetPlayerOnMission(false);
        RunState::ActivateRun();

        if (entry->launchVariant == ScriptRuntime::MissionLaunchVariant::IntroToLuigisGirls) {
            // The live-world INTRO launch is retired: injected into a running
            // session the cinematic renders with the offset/ghosted frame no
            // matter how the world is staged. Instead, restart the game
            // session through the engine's own new-game machinery
            // (m_bWantToRestart): the main loop tears the world down and
            // reloads main.scm, where the armed boot selector routes the boot
            // into the vanilla-style intro launch. The persisted request is a
            // belt-and-suspenders fallback: if anything interrupts the
            // restart, the next "Run Archipelago" still boots the intro.
            RunState::RequestBootIntro();
            Hooks::g_patchApBootIntroLaunch = true;
            PlayerRuntime::ArmBootIntroSequence();
            if (GameAddr::TryRequestGameRestart()) {
                TextOverrides::SetBridgeToast("RESTARTING: INTRO");
                PlayerRuntime::LaunchPrintNowStub("APBRG", 4000);
                Logger::Log("APINTRO click -> engine restart requested, boot-intro armed");
            } else {
                TextOverrides::SetBridgeToast("INTRO ARMED: QUIT + RUN ARCHIPELAGO");
                PlayerRuntime::LaunchPrintNowStub("APBRG", 5000);
                Logger::Log("APINTRO click -> restart request FAILED; persisted boot-intro request remains");
            }
            return true;
        }
        if (!LoadMissionBytecode(missionFileOffset)) {
            Logger::Log("Mission launch aborted: bytecode load failed for key='%.8s' actual=%d compiled=%d",
                        entry->displayKey ? entry->displayKey : "",
                        actualMissionIndex,
                        compiledMissionIndex);
            return false;
        }
        if (!PlayerRuntime::ConfigureMissionLaunch(*entry)) {
            Logger::Log("Mission launch aborted: launch config failed for key='%.8s'",
                        entry->displayKey);
            return false;
        }

        // Preload the destination scene ONCE, synchronously, before the script
        // runs. CStreaming::LoadScene calls RemoveModel on every non-priority
        // request still in the queue (re3 Streaming.cpp:2710), so pumping it
        // every few frames during the cutscene was nuking the script's own
        // REQUEST_MODEL calls (the destroyed-bridge variants -128..-133 the
        // INTRO mission needs after the skip). One synchronous call here
        // primes the area without fighting the script later.
        if (actualMissionIndex == 2) {
            // INTRO: SCM teleports the player to (811.9, -939.94, 35.8) right
            // after the cutscene/skip (main_current_decompiled.ir2:13356). The
            // SWAP_NEAREST_BUILDING_MODEL calls at lines 13578-13589 need the
            // Callahan Bridge buildings already loaded to find swap targets.
            PlayerRuntime::TryLoadSceneAt(PlayerRuntime::kGiveMeLibertyScene,
                                          "INTRO pre-launch");
            // The cutscene's own LoadScene at (1490,1311,130) almost certainly
            // evicts what we preloaded. Arm a watcher that re-LoadScenes once
            // the player teleports back to the bridge post-cutscene.
            PlayerRuntime::ArmIntroTeleportWatch();
        }
        // For non-INTRO missions Claude already stands near the mission start
        // (the safehouse triggers Luigi missions, etc.), so models are
        // resident. We deliberately do NOT LoadScene here: it calls
        // DeleteAllRwObjects (re3 Streaming.cpp:2718) which voids RW handles
        // still referenced by the mission scripts we just aborted, and the
        // first opcode of the new mission can crash on a dangling pointer.

        // Authorize BEFORE launching the safe-state stub: the stub's own
        // StartNewScript still flows through our hook, and without the
        // authorization the engine-block path would patch the stub itself
        // to TERMINATE before MAKE_PLAYER_SAFE_FOR_CUTSCENE runs.
        const std::uint32_t memoryIp = static_cast<std::uint32_t>(ScriptRuntime::kSizeMainScript);
        ScriptRuntime::AuthorizeMissionLaunch(actualMissionIndex);
        const bool playerStartedInVehicle = PlayerRuntime::PlayerVehicle() != nullptr;
        const bool launchFromClassicMarker = g_launchFromClassicMarker;
        const bool preparedVehicleLaunch =
            PlayerRuntime::TryPrepareVehicleForMissionLaunch(
                actualMissionIndex, entry->launchVariant, aborted > 0);
        // Vanilla contact loops call MAKE_PLAYER_SAFE_FOR_CUTSCENE before
        // LOAD_AND_LAUNCH_MISSION_INTERNAL when cleaning up a dirty player
        // state. Direct AP marker launches bypass those loops. Run the helper
        // synchronously so it cannot interleave with the mission's own
        // LOAD_SPECIAL_* / LOAD_CUTSCENE setup.
        const bool needsVanillaPrelaunchSafeState =
            actualMissionIndex != 2 &&
            actualMissionIndex != 21 &&
            !preparedVehicleLaunch &&
            (aborted > 0 || playerStartedInVehicle);
        const bool needsVanillaCutscenePrelaunch =
            !preparedVehicleLaunch &&
            MissionNeedsVanillaCutscenePrelaunch(actualMissionIndex);
        if (needsVanillaCutscenePrelaunch) {
            if (!RunVanillaCutscenePrelaunchSynchronously(entry->displayKey,
                                                          actualMissionIndex)) {
                ScriptRuntime::ClearAuthorizedMissionLaunch();
                return false;
            }
        } else if (needsVanillaPrelaunchSafeState) {
            RunSafeStateStubSynchronously("safe-state");
        } else if (!preparedVehicleLaunch && aborted <= 0 && !playerStartedInVehicle) {
            Logger::Log("Safe-state stub skipped: clean on-foot launch actual=%d",
                        actualMissionIndex);
        }
        // Pre-teleport Claude to the mission's trigger location (table in
        // PlayerRuntime). vanilla *_mission_loop blocks wait for Claude to
        // step onto the marker before LOAD_AND_LAUNCH_MISSION, so the
        // mission's intro cutscene opens on him; the pause-menu path skips
        // that wait, leaving Claude wherever he stood (the safehouse for
        // EIGHT, the previous mission's failure spot otherwise).
        if (launchFromClassicMarker) {
            Logger::Log("Mission spawn teleport skipped: classic marker launch actual=%d",
                        actualMissionIndex);
        } else if (!preparedVehicleLaunch) {
            PlayerRuntime::TryTeleportToMissionSpawn(actualMissionIndex, entry->launchVariant);
        }
        CRunningScript* script = ScriptRuntime::StartNewScript()(memoryIp);
        ScriptRuntime::ClearAuthorizedMissionLaunch();
        if (!script) {
            Logger::Log("Mission launch failed: StartNewScript returned null for actual index=%d",
                        actualMissionIndex);
            return false;
        }

        ScriptRuntime::MarkAsMissionScript(script);
        ScriptRuntime::SetMissionFlag(script);
        // Re-assert the engine's "mission active" globals: the previous
        // mission's TERMINATE_THIS_SCRIPT (run by our proper abort) cleared
        // bAlreadyRunningAMissionScript and may have left FailCurrentMission
        // non-zero. Without re-setting them the engine has a state mismatch
        // (m_bMissionFlag=true on our new script but "no mission running"
        // globally), which leads to crashes once the new script starts
        // executing opcodes that depend on the global mission flag.
        ScriptRuntime::AlreadyRunningAMissionScript() = true;
        ScriptRuntime::FailCurrentMission() = 0;
        // The mission no longer drags Claude back to his safehouse on
        // completion: when it ends (validate_mission debug key for now, real
        // pass/fail detection later) he stays exactly where he is, in his
        // vehicle. So we do NOT arm the position-based teleport-home watcher.
        // Remember which mission is in flight so it can be validated when it
        // finishes.
        RunState::SetLastLaunchedMission(actualMissionIndex);
        if (entry->syntheticValidationKey && entry->syntheticValidationKey[0] != 0) {
            RunState::SetPendingValidationMission(-1, entry->syntheticValidationKey);
        } else {
            RunState::SetPendingValidationMission(actualMissionIndex);
        }
        // Preload this mission's recorded objective positions into the F1 list
        // up front, so the warp key just reads a ready in-memory list (like F5).
        PlayerRuntime::PreloadObjectivePositions(actualMissionIndex);
        Logger::Log("Mission launch queued: key='%.8s' actual index=%d compiled index=%d variant=%d fileOffset=%d memoryIp=%u bucket=%d vehiclePrep=%d",
                    entry->displayKey, actualMissionIndex,
                    compiledMissionIndex,
                    static_cast<int>(entry->launchVariant),
                    missionFileOffset, memoryIp,
                    static_cast<int>(g_state.selectedBucket),
                    preparedVehicleLaunch ? 1 : 0);
        return true;
    }

    bool FlushPendingMissionLaunch(MenuPage currentPage) {
        if (!g_state.pendingMissionLaunch) {
            return false;
        }
        if (currentPage != MenuPage::None) {
            return false;
        }
        if (!HasMissionRuntime()) {
            return false;
        }

        const ScriptRuntime::VisibleMissionEntry* entry = g_state.pendingMissionLaunch;
        g_state.pendingMissionLaunch = nullptr;
        if (!LaunchMission(entry)) {
            Logger::Log("Deferred mission launch skipped for key='%.8s' actual index=%d",
                        entry->displayKey, entry->actualIndex);
        }
        return true;
    }

    bool FlushPendingMissionAction(MenuPage currentPage) {
        if (g_state.pendingMissionAction == PendingMissionAction::None) {
            return false;
        }
        if (currentPage != MenuPage::None) {
            return false;
        }
        if (!HasMissionRuntime()) {
            return false;
        }

        const PendingMissionAction action = g_state.pendingMissionAction;
        const ScriptRuntime::VisibleMissionEntry* entry = g_state.pendingMissionActionEntry;
        g_state.pendingMissionAction = PendingMissionAction::None;
        g_state.pendingMissionActionEntry = nullptr;
        const bool introSequenceActive = PlayerRuntime::IsIntroSequenceMissionActive();
        const int exitingMission = RunState::LastLaunchedMission();
        char exitingSyntheticKey[9] = {};
        if (const char* syntheticKey = RunState::PendingValidationSyntheticMission()) {
            std::memcpy(exitingSyntheticKey,
                        syntheticKey,
                        std::min<std::size_t>(std::strlen(syntheticKey), 8));
        }

        PlayerRuntime::EndMissionInPlace();
        PlayerRuntime::OnMissionExit(
            exitingMission,
            exitingSyntheticKey,
            action == PendingMissionAction::Quit
                ? PlayerRuntime::MissionExitReason::Quit
                : PlayerRuntime::MissionExitReason::Aborted,
            action == PendingMissionAction::Quit ? "mission quit" : "mission redo");
        PlayerRuntime::ResetMissionValidationWatchState();
        RunState::ClearPendingValidationMission();
        RunState::SetLastLaunchedMission(-1);
        g_state.pendingMissionLaunch = nullptr;

        if (action == PendingMissionAction::Quit) {
            if (introSequenceActive) {
                // The intro is a special boot state (Claude in a cutscene void);
                // it still needs its dedicated recovery rather than a stop-in-place.
                PlayerRuntime::RecoverFromIntroSequenceQuit();
            } else {
                // Leave Claude exactly where the mission ended, like a genuine
                // mission failure (timer-out / protected target destroyed): the
                // mission's own deatharrest cleanup already ran in
                // EndMissionInPlace and terminated the script net. The
                // cancel-restart stub queued there blocks any engine override
                // respawn, so no warp — Claude stays free-roaming on the spot.
                Logger::Log("Mission quit: left player in place (no safehouse warp)");
            }
            Logger::Log("Deferred mission action applied: quit current mission");
            return true;
        }

        if (!entry || entry->bugged) {
            Logger::Log("Deferred mission action skipped: redo target unavailable");
            return true;
        }

        if (!LaunchMission(entry)) {
            Logger::Log("Deferred mission action failed: redo key='%.8s' actual index=%d",
                        entry->displayKey, entry->actualIndex);
        } else {
            Logger::Log("Deferred mission action applied: redo key='%.8s' actual index=%d",
                        entry->displayKey, entry->actualIndex);
        }
        return true;
    }

    bool InsertArchipelagoIntoNewGame() {
        CMenuScreen* page = Screens::Page(MenuPage::NewGame);
        if (!g_state.hasNormalStartEntry) {
            g_state.normalStartEntry = page->m_aEntries[0];
            g_state.hasNormalStartEntry = true;
        }

        if (KeyEquals(page->m_aEntries[0].m_EntryName, kNewGameEntryKey)) {
            return true;
        }

        const CMenuEntry archipelago = CloneEntryWithKey(g_state.normalStartEntry, kNewGameEntryKey);
        if (!WriteProtected(&page->m_aEntries[0], archipelago)) {
            return false;
        }

        Logger::Log("NewGame patch applied: slot 0 relabelled to FE_ARCH (action=%d target=%d)",
                    archipelago.m_nAction, archipelago.m_nTargetMenu);
        return true;
    }

    // Add non-launching Archipelago status rows below FE_ARCH on the shared
    // NewGame page while preserving the normal LOAD/DELETE/BACK entries.
    bool AddConnectionStatusToNewGame() {
        CMenuScreen* page = Screens::Page(MenuPage::NewGame);
        if (FindEntryByKey(page, kNewGameEntryKey) < 0) {
            return false;
        }

        CMenuScreen patched = *page;
        int write = 0;
        bool insertedStatus = false;
        for (int i = 0; i < 18; ++i) {
            const CMenuEntry& entry = page->m_aEntries[i];
            if (entry.m_nAction == MENUACTION_NOTHING && entry.m_EntryName[0] == 0) {
                continue;
            }
            if (KeyEquals(entry.m_EntryName, "APCONN") ||
                KeyEquals(entry.m_EntryName, "APCASH")) {
                continue;
            }
            if (KeyEquals(entry.m_EntryName, "GMLOAD") ||
                KeyEquals(entry.m_EntryName, "FES_DGA")) {
                continue;
            }
            if (write >= 18) {
                return false;
            }
            patched.m_aEntries[write++] = entry;
            if (!insertedStatus && KeyEquals(entry.m_EntryName, kNewGameEntryKey)) {
                if (write >= 18) {
                    return false;
                }
                patched.m_aEntries[write++] = MakeMenuEntry("APCONN", MenuPage::NewGame);
                insertedStatus = true;
            }
        }
        if (!insertedStatus) {
            return false;
        }
        for (int i = write; i < 18; ++i) {
            patched.m_aEntries[i] = CMenuEntry{};
        }
        return WriteProtected(page, patched);
    }

    bool RemoveCashStatusFromPauseMenu() {
        return RemoveEntryByKey(MenuPage::PauseMenu, "APCASH", "CASH");
    }

    bool HasPersistedApProgress() {
        // Source of truth for "is there a run to resume" is the durable RunState
        // (.ini), NOT the Archipelago sync JSON. The AP bridge rewrites
        // locations_checked/received_items from the server on connect, so a slot
        // the server has no checks for empties that JSON to [] — which used to
        // make this return false and trigger BeginNewRun(), wiping an in-progress
        // run (validated missions, unlocked contacts) on every boot. We resume
        // whenever an active run carries any real local progress, and still fall
        // back to the AP JSON in case run_active was somehow not latched.
        // ANY persisted progress counts — money, ammo, packages, a saved
        // position, the synthetic GML validation... The old test only looked
        // at checks/validated/contacts, so a young run (cash and ammo earned
        // but nothing validated yet) was wiped by BeginNewRun on every boot.
        if (RunState::IsRunActive() && RunState::HasAnyRunProgress()) {
            return true;
        }
        return !ApState::LocationsChecked().empty();
    }

    void DumpPauseMenuActions() {
        const CMenuScreen* page = Screens::Page(MenuPage::PauseMenu);
        Logger::Log("PauseMenu dump: screenName=%.8s prev=%d", page->m_ScreenName,
                    page->m_nPreviousPage);
        for (int i = 0; i < 18; ++i) {
            const CMenuEntry& entry = page->m_aEntries[i];
            if (entry.m_nAction == MENUACTION_NOTHING && entry.m_EntryName[0] == 0) {
                continue;
            }
            Logger::Log("  slot %2d: key=%-8.8s action=%d target=%d slot=%d",
                        i, entry.m_EntryName, entry.m_nAction,
                        entry.m_nTargetMenu, entry.m_nSaveSlot);
        }
    }

    bool InsertArchipelagoIntoPauseMenu() {
        CMenuScreen* page = Screens::Page(MenuPage::PauseMenu);
        if (FindEntryByKey(page, kPauseEntryKey) >= 0) {
            return true;
        }

        DumpPauseMenuActions();

        const int emptySlot = FindFirstEmptyEntry(page);
        constexpr int insertSlot = 1;
        if (emptySlot < insertSlot) {
            Logger::Log("PauseMenu patch aborted: no empty slot for FE_RUNM");
            return false;
        }

        CMenuScreen patched = *page;
        for (int slot = emptySlot; slot > insertSlot; --slot) {
            patched.m_aEntries[slot] = patched.m_aEntries[slot - 1];
        }

        patched.m_aEntries[insertSlot] = MakeMenuEntry(kPauseEntryKey, MenuPage::Archipelago);
        if (!WriteProtected(page, patched)) {
            return false;
        }

        Logger::Log("PauseMenu patch applied: FE_RUNM inserted at slot %d", insertSlot);
        return true;
    }

    // Strip the vanilla save/load/new-game flow so it can't be used mid-run:
    //   - FEN_STA "START GAME" off the pause menu (page 52) — its NewGame
    //     submenu is meaningless while a run is live.
    //   - GMLOAD "LOAD GAME" and FES_DGA "DELETE GAME" off the NewGame page
    //     (page 2), which is shared with the main menu, leaving just our
    //     FE_ARCH "RUN ARCHIPELAGO" entry and the Back button there.
    bool HideUnwantedMenuEntries() {
        return RemoveEntryByKey(MenuPage::PauseMenu, kStartGameKey, "START GAME");
    }
}

bool MenuPatch::QueueIntroGmlFallbackMissionLaunch() {
    Hooks::g_allowNextEngineIntroLaunch = false;
    Hooks::g_allowNextEngineEightballLaunch = false;
    return LaunchMission(&kGiveMeLibertyRedoMission);
}

bool MenuPatch::QueueGiveMeLibertyRetryMissionLaunch() {
    Hooks::g_allowNextEngineIntroLaunch = false;
    Hooks::g_allowNextEngineEightballLaunch = false;
    return LaunchMission(&kGiveMeLibertyRedoMission);
}

void MenuPatch::TickMarkerLaunch() {
    // The vanilla per-mission trigger loops stay TERMINATED in AP builds
    // (their expiry guards and story-driven start chain do not survive AP
    // unlock order). This watcher re-creates the marker behaviour: standing
    // on a contact point that has a playable mission launches that mission
    // through the same pipeline as the pause-menu selector.
    static int s_cooldownFrames = 0;
    if (s_cooldownFrames > 0) {
        --s_cooldownFrames;
        return;
    }
    if (!Config::UseClassicMissionMarkers() || !RunState::IsRunLive()) {
        return;
    }
    if (!HasMissionRuntime() || MenuMgr::CurrentPage() != MenuPage::None) {
        return;
    }
    if (ScriptRuntime::IsPlayerOnMission() ||
        ScriptRuntime::AlreadyRunningAMissionScript() ||
        PlayerRuntime::IsIntroSequenceLaunchPending()) {
        return;
    }
    // PlayerVehicle() now checks CPed::m_bInVehicle. The adjacent vehicle
    // pointer can retain a deleted/previous car, which used to block this watcher.
    if (PlayerRuntime::PlayerVehicle() != nullptr) {
        return; // vanilla markers are on-foot triggers
    }
    PlayerRuntime::CVector pos{};
    if (!PlayerRuntime::TryReadPlayerPos(&pos)) {
        return;
    }
    constexpr float kTriggerRadiusSq = 1.8f * 1.8f;
    for (const PlayerRuntime::ContactBlipDef& def : PlayerRuntime::kContactBlipDefs) {
        const float dx = pos.x - def.x;
        const float dy = pos.y - def.y;
        if (dx * dx + dy * dy > kTriggerRadiusSq) {
            continue;
        }
        const ScriptRuntime::VisibleMissionEntry* playable =
            ScriptRuntime::FindPlayableMissionInBucket(def.bucket);
        if (!playable) {
            // Standing on a contact whose chain has nothing playable: hint
            // once, then back off a few seconds so the toast cannot spam.
            PlayerRuntime::QueueMarkerBlockedToast(
                ScriptRuntime::FindNextUnvalidatedMissionInBucket(def.bucket));
            s_cooldownFrames = 5 * 60;
            return;
        }
        Logger::Log("Marker proximity launch: bucket=%d key='%.8s' actual=%d",
                    static_cast<int>(def.bucket),
                    playable->displayKey ? playable->displayKey : "",
                    playable->actualIndex);
        ScopedClassicMarkerLaunch markerLaunch;
        if (LaunchMission(playable)) {
            s_cooldownFrames = 10 * 60; // safety re-arm window after a launch
        } else {
            s_cooldownFrames = 3 * 60;
        }
        return;
    }
}

bool MenuPatch::Apply() {
    g_state.parentPage = MenuPage::PauseMenu;
    g_state.selectedRegion = MissionRegion::Portland;
    g_state.selectedListMode = ListMode::Groups;
    g_state.selectedBucket = ScriptRuntime::MissionBucket::Luigi;

    const bool newGameReady = InsertArchipelagoIntoNewGame();
    // Classic-marker mode: missions launch from their vanilla street markers,
    // so the pause menu stays VANILLA (no AP mission selector). The run is
    // still started from the main-menu New Game entry, and the unwanted
    // entries (start/load) are still stripped below.
    const bool pauseReady = Config::UseClassicMissionMarkers()
        ? true
        : InsertArchipelagoIntoPauseMenu();
    // Strip the pause-menu-only entry after FE_ARCH / FE_RUNM are in place.
    const bool hiddenReady = HideUnwantedMenuEntries();
    const bool newGameStatusReady = AddConnectionStatusToNewGame();
    const bool pauseCashReady = RemoveCashStatusFromPauseMenu();
    const bool rootReady = RefreshMissionRootPage();
    const bool groupsReady = RefreshMissionGroupsPage();
    const bool listReady = RefreshMissionListPage();

    g_state.installed = newGameReady && pauseReady && hiddenReady &&
                        newGameStatusReady && pauseCashReady &&
                        rootReady && groupsReady && listReady;
    return g_state.installed;
}

void MenuPatch::BeginFrame() {
    if (!g_state.installed) {
        return;
    }

    // Offline = no Archipelago run: while the bridge is disconnected the
    // "Run Archipelago" slot shows the vanilla start entry instead. The
    // status line on the page already explains why (ARCHIPELAGO: OFFLINE).
    if (g_state.hasNormalStartEntry) {
        CMenuScreen* newGamePage = Screens::Page(MenuPage::NewGame);
        const bool showingRunEntry =
            KeyEquals(newGamePage->m_aEntries[0].m_EntryName, kNewGameEntryKey);
        if (ApBridge::g_connected && !showingRunEntry) {
            InsertArchipelagoIntoNewGame();
            Logger::Log("Run Archipelago entry restored: bridge connected");
        } else if (!ApBridge::g_connected && showingRunEntry) {
            WriteProtected(&newGamePage->m_aEntries[0], g_state.normalStartEntry);
            Logger::Log("Run Archipelago entry hidden: bridge offline");
        }
    }

    g_state.beforePage = MenuMgr::CurrentPage();
    g_state.beforeEntry = MenuMgr::CurrentEntry();
    SnapshotSelectedKey();
    // Track back/cancel input on the Archipelago pages so a launch can never
    // fire on the same transition ESC closes the frontend. Held back re-arms
    // the latch; otherwise it decays over a few frames to cover the close.
    if ((IsArchipelagoPage(g_state.beforePage) ||
         g_state.beforePage == MenuPage::PauseMenu) &&
        IsMenuBackDownNow()) {
        g_state.menuBackLatchFrames = 5;
    } else if (g_state.menuBackLatchFrames > 0) {
        --g_state.menuBackLatchFrames;
    }
    // Dwell: count consecutive frames on the current page (reset on a change).
    if (g_state.beforePage == g_state.dwellPage) {
        if (g_state.dwellFrames < 100000) {
            ++g_state.dwellFrames;
        }
    } else {
        g_state.dwellPage = g_state.beforePage;
        g_state.dwellFrames = 0;
    }

    g_state.pauseMissionListConfirmThisFrame = false;
    g_state.pauseRootConfirmThisFrame = false;
    if (PollMenuConfirmPressed() &&
        g_state.parentPage == MenuPage::PauseMenu &&
        g_state.dwellFrames >= kSelectDwellFrames) {
        if (g_state.beforePage == MenuPage::ArchipelagoMissionList) {
            g_state.pauseMissionListConfirmThisFrame = true;
        } else if (g_state.beforePage == MenuPage::Archipelago) {
            g_state.pauseRootConfirmThisFrame = true;
        }
    }

    const bool pendingRunStart =
        g_state.beforePage == MenuPage::NewGame &&
        KeyEquals(g_state.beforeKey, kNewGameEntryKey);
    if (pendingRunStart != g_state.runStartSelectionActive) {
        g_state.runStartSelectionActive = pendingRunStart;
        RunState::SetRunStartPending(pendingRunStart);
        if (pendingRunStart) {
            // A brand-new run (no checks yet) must start on island 1. Reset map
            // progression to 0 HERE, before the engine boots the SCM, so
            // PushMapStateToScript never feeds a stale persisted map_state (e.g.
            // 1 or 2 left by a prior session's F7) into unused_2 during the boot
            // window — that race fires the mission_start unlock and tears down
            // the broken-bridge sections the boot just created. A resume
            // (checks>0) keeps its already-unlocked islands.
            if (!HasPersistedApProgress()) {
                RunState::SetMapState(0);
            }
            // The Archipelago SCM (main_ap.scm) is always the active main
            // script: its main thread already skips INTRO via GOTO mission_start,
            // so no IS_PLAYER_PLAYING bytecode patch is applied (that story-only
            // patch would clobber unrelated branches).
            Logger::Log("Run start pending from NewGame selection (main_ap.scm, no bytecode patch)");
            // Latch persists across BeginFrame snapshots. EndFrame consumes it
            // the first time afterPage settles outside NewGame/Archipelago,
            // even if pendingRunStart has already flipped back to false by
            // then because the engine pre-processed the click.
            g_state.runStartLatched = true;
        } else {
            Hooks::g_patchMainThreadIntroSkip = false;
        }
    }
}

void MenuPatch::EndFrame() {
    if (!g_state.installed) {
        return;
    }

    const MenuPage afterPage = MenuMgr::CurrentPage();
    if (PollMenuConfirmPressed() &&
        g_state.parentPage == MenuPage::PauseMenu &&
        g_state.dwellFrames >= kSelectDwellFrames) {
        if (g_state.beforePage == MenuPage::ArchipelagoMissionList) {
            g_state.pauseMissionListConfirmThisFrame = true;
        } else if (g_state.beforePage == MenuPage::Archipelago) {
            g_state.pauseRootConfirmThisFrame = true;
        }
    }

    // Track the post-process mission-list selection (m_nCurrOption AFTER
    // g_origProcess) on every frame the list is open. With a mouse, the engine
    // moves the highlight to the clicked entry and activates it inside this same
    // process call, so BeginFrame's beforeKey points at the previously-
    // highlighted entry. Recording the post-process selection here means the
    // close frame (afterPage==None) launches the entry the user actually clicked.
    if (afterPage == MenuPage::ArchipelagoMissionList) {
        ResolveEntryKey(MenuPage::ArchipelagoMissionList, MenuMgr::CurrentEntry(),
                        g_state.missionListSelectedKey);
    }
    // Same post-process capture for the root page (REDO/QUIT/BUSY), so the
    // REDO/QUIT decision below reads the entry actually under the mouse.
    if (afterPage == MenuPage::Archipelago) {
        ResolveEntryKey(MenuPage::Archipelago, MenuMgr::CurrentEntry(),
                        g_state.rootSelectedKey);
    }

    if (FlushPendingMissionAction(afterPage)) {
        return;
    }

    if (FlushPendingMissionLaunch(afterPage)) {
        return;
    }

    if (g_state.navigationCooldownFrames > 0 &&
        IsArchipelagoPage(g_state.beforePage) &&
        g_state.beforePage == afterPage) {
        --g_state.navigationCooldownFrames;
        return;
    }

    // Two trigger paths, both gated on the sticky latch set in BeginFrame:
    //   1. Within-frame: BeginFrame saw NewGame + FE_ARCH, engine flipped the
    //      page to non-NewGame inside g_origProcess this very frame. The
    //      classic condition catches this — beforePage was 51 at BeginFrame
    //      and afterPage is anything else now.
    //   2. Across-frame: the engine pre-processed the click before our hook
    //      ran, so by the time BeginFrame runs, CurrentPage is already past
    //      NewGame. beforePage will read as MainMenu/0/whatever — but the
    //      latch from a prior frame (when the user was still hovering
    //      Run Archipelago on NewGame) survives the BeginFrame reset and
    //      tells us the run-start is in flight.
    const bool classicTrigger =
        g_state.beforePage == MenuPage::NewGame &&
        afterPage != MenuPage::NewGame &&
        !IsArchipelagoPage(afterPage) &&
        KeyEquals(g_state.beforeKey, kNewGameEntryKey);
    const bool latchedTrigger =
        g_state.runStartLatched &&
        afterPage != MenuPage::NewGame &&
        !IsArchipelagoPage(afterPage);

    if (classicTrigger || latchedTrigger) {
        g_state.runStartLatched = false;
        // Belt for the click-vs-disconnect race: the entry is hidden offline,
        // but if the link dropped between the click and this trigger, refuse
        // to activate the run (the world boots as a plain vanilla session).
        if (!ApBridge::g_connected) {
            PlayerRuntime::QueueOfflineRunToast();
            Logger::Log("Run start REFUSED: Archipelago bridge offline");
            return;
        }
        const bool needsGiveMeLibertyBoot =
            !PlayerRuntime::IsGiveMeLibertyCompleted();
        const bool freshRun = needsGiveMeLibertyBoot || !HasPersistedApProgress();
        if (!freshRun) {
            RunState::ActivateRun();
            Logger::Log("Run mode resumed from NewGame");
        } else {
            RunState::BeginNewRun();
            PlayerRuntime::QueueStartupApStateReset();
            PlayerRuntime::ResetMoneyPersistenceSession();
            PlayerRuntime::ResetPlayerPersistenceSession();
            Logger::Log("Run mode started from NewGame");
        }
        RunState::SetSkipIntroOnNextNewGame(false);
        // The Archipelago SCM (main_ap.scm) is always active: its main thread
        // skips INTRO via ARCHIPELAGO_BOOT (GOTO mission_start) and spawns Claude
        // at the safehouse directly. A FRESH run instead routes the boot into the
        // INTRO -> Give Me Liberty chain (below); resumed runs boot freeroam. We
        // arm the engine block as a safety net and queue the safehouse warp
        // idempotently in case the SCM spawn ever drifts.
        Hooks::g_blockEngineMissionLaunches = true;
        const bool introRequested = RunState::IsBootIntroRequested();
        if (introRequested) {
            RunState::ClearBootIntroRequest();
        }
        if (needsGiveMeLibertyBoot) {
            // Vanilla-style intro boot for FRESH runs only: the ASI patches
            // the SCM boot selector so the new game opens on the INTRO ->
            // Give Me Liberty chain in pristine boot conditions. Resumed runs
            // always boot freeroam — never lock a returning player out of the
            // selector behind GML.
            // The safehouse warp below stays queued as the self-healing
            // fallback: if the engine really restarts, the selector patch
            // cancels it (the intro owns the boot); if this run start turned
            // out to be in the live world (no SCM reload), the warp completes
            // and disarms the whole boot-intro state.
            Hooks::g_patchApBootIntroLaunch = true;
            PlayerRuntime::ArmBootIntroSequence();
            // This trigger fires AFTER the engine already reloaded main.scm
            // for the new game (StartNewScript(0) ran seconds ago, unpatched),
            // so the selector patch missed its window. Request one more engine
            // restart with the flags pre-armed — the same proven mechanism as
            // the APINTRO click — and the second boot routes into the intro.
            if (GameAddr::TryRequestGameRestart()) {
                Logger::Log("Run start: vanilla-style INTRO boot armed; engine restart requested (fresh run, GML not validated)");
            } else {
                Logger::Log("Run start: INTRO boot armed but restart request FAILED; 30 s fallback will disarm into freeroam");
            }
        } else {
            Logger::Log("Freeroam-at-safehouse armed (AP SCM): engine block + safehouse warp (no INTRO skip needed)");
        }
        // A freeroam start must never inherit intro-sequence leftovers from an
        // earlier run in this same process (pending APINTRO validation, armed
        // watches): they lock the mission selector behind "FINISH CURRENT
        // MISSION" with no mission running. Guarded so it cannot tear down a
        // boot-intro the branch above just armed.
        if (!Hooks::g_patchApBootIntroLaunch) {
            PlayerRuntime::DisarmBootIntroSequence("freeroam run start");
        }
        PlayerRuntime::QueueSafehouseWarp();
        return;
    }

    if (!IsArchipelagoPage(g_state.beforePage) && afterPage == MenuPage::Archipelago) {
        g_state.parentPage = g_state.beforePage;
        g_state.selectedListMode = ListMode::Groups;
        RefreshMissionRootPage();
        MenuMgr::SetCurrentEntry(0);
        // Cooldown so the confirm release from the click that opened this page
        // is swallowed instead of being read as a REDO/QUIT selection. The
        // mission list gets this for free via ReopenPage; the root page is
        // entered directly by the engine (52->57) and would otherwise instantly
        // arm REDO and relaunch the active mission.
        g_state.navigationCooldownFrames = 2;
        return;
    }

    // Archipelago (regions) → ArchipelagoMissions (groups): user picked a region.
    if (g_state.beforePage == MenuPage::Archipelago && afterPage == MenuPage::ArchipelagoMissions) {
        MissionRegion region = MissionRegion::Portland;
        if (TryGetRegionForKey(g_state.beforeKey, &region)) {
            g_state.selectedRegion = region;
            g_state.selectedListMode = ListMode::Groups;
            ReopenPage(MenuPage::ArchipelagoMissions, 0);
            return;
        }
    }

    // ArchipelagoMissions (groups) → Archipelago: back navigation.
    if (g_state.beforePage == MenuPage::ArchipelagoMissions && afterPage == MenuPage::Archipelago) {
        RefreshMissionRootPage();
        MenuMgr::SetCurrentEntry(RootEntryIndexForRegion(g_state.selectedRegion));
        return;
    }

    // REDO/QUIT confirm: a real confirm press+release on the settled root page
    // (pauseRootConfirmThisFrame, dwell-gated), never a held key. Back/cancel
    // (ESC) never counts. We arm the action and let FlushPendingMissionAction
    // fire it once the frontend finishes closing (the close is multi-frame, so
    // we must NOT also require afterPage to be None on this exact frame).
    const bool confirmedPauseRootAction =
        g_state.beforePage == MenuPage::Archipelago &&
        g_state.parentPage == MenuPage::PauseMenu &&
        g_state.pauseRootConfirmThisFrame &&
        g_state.menuBackLatchFrames == 0;
    if (confirmedPauseRootAction) {
        if (g_state.pendingMissionAction != PendingMissionAction::None) {
            return;
        }
        if (KeyEquals(g_state.rootSelectedKey, kBusyKey)) {
            Logger::Log("Mission action page confirmed busy state");
            return;
        }
        if (KeyEquals(g_state.rootSelectedKey, kRedoKey)) {
            g_state.pendingMissionLaunch = nullptr;
            g_state.pendingMissionAction = PendingMissionAction::Redo;
            g_state.pendingMissionActionEntry = FindCurrentMissionEntry();
            Logger::Log("Deferred mission action armed: redo actual index=%d",
                        RunState::LastLaunchedMission());
            return;
        }
        if (KeyEquals(g_state.rootSelectedKey, kQuitKey)) {
            g_state.pendingMissionLaunch = nullptr;
            g_state.pendingMissionAction = PendingMissionAction::Quit;
            g_state.pendingMissionActionEntry = nullptr;
            Logger::Log("Deferred mission action armed: quit actual index=%d",
                        RunState::LastLaunchedMission());
            return;
        }
    }

    // ArchipelagoMissions (groups) → ArchipelagoMissionList: user picked a bucket.
    if (g_state.beforePage == MenuPage::ArchipelagoMissions &&
        afterPage == MenuPage::ArchipelagoMissionList) {
        ScriptRuntime::MissionBucket bucket = ScriptRuntime::MissionBucket::Luigi;
        if (TryGetBucketForKey(g_state.beforeKey, &bucket)) {
            g_state.selectedBucket = bucket;
            g_state.selectedListMode = ListMode::Missions;
            ReopenPage(MenuPage::ArchipelagoMissionList, 0);
            return;
        }
    }

    // ArchipelagoMissionList → ArchipelagoMissions: back navigation.
    if (g_state.beforePage == MenuPage::ArchipelagoMissionList &&
        afterPage == MenuPage::ArchipelagoMissions) {
        g_state.selectedListMode = ListMode::Groups;
        RefreshMissionGroupsPage();
        return;
    }

    if (g_state.beforePage != MenuPage::ArchipelagoMissionList) {
        return;
    }

    if (g_state.parentPage != MenuPage::PauseMenu &&
        g_state.parentPage != MenuPage::NewGame) {
        return;
    }

    if (g_state.parentPage == MenuPage::PauseMenu) {
        // Pause-menu mission entries use RESUME_FROM_SAVEZONE (action 49), which
        // closes the frontend over several frames (56 -> 52 -> 0). We ARM here on
        // a genuine confirm release (press+release on the settled list, dwell-
        // gated in Begin/EndFrame) and let FlushPendingMissionLaunch fire it once
        // the menu reaches None — so we must NOT couple to afterPage this frame.
        if (g_state.pendingMissionLaunch) {
            return;
        }
        const bool confirmOnList = g_state.pauseMissionListConfirmThisFrame;
        const bool backActive = g_state.menuBackLatchFrames > 0;
        // Hard rule: a back/cancel press in flight (ESC etc.) NEVER launches.
        if (!confirmOnList || backActive) {
            return;
        }
    } else if (IsArchipelagoPage(afterPage)) {
        return;
    }

    // Prefer the post-process selection (handles mouse clicks, which only move
    // m_nCurrOption to the clicked entry during g_origProcess); fall back to the
    // BeginFrame snapshot if no list frame captured a selection yet.
    const char* selectedKey =
        g_state.missionListSelectedKey[0] ? g_state.missionListSelectedKey : g_state.beforeKey;
    const ScriptRuntime::VisibleMissionEntry* entry =
        ScriptRuntime::FindVisibleMissionByDisplayKey(selectedKey, g_state.selectedBucket);
    // Only a launchable entry arms. Locked/bugged entries are shown as
    // self-target no-ops in RefreshMissionListPage (the menu stays on page 56),
    // so a press on one must NOT arm a pending launch — otherwise it would
    // linger and fire the locked mission the next time the frontend closes (ESC).
    if (entry && !entry->bugged && ScriptRuntime::IsMissionEntryUnlocked(*entry)) {
        // Bug fix: never launch a mission while one is already running. Selecting a
        // mission mid-mission previously force-cleared the engine's mission flag and
        // hot-swapped scripts, which corrupts state and crashes. Require the player
        // to finish or abort the current mission first.
        if (g_state.parentPage == MenuPage::PauseMenu &&
            (ScriptRuntime::AlreadyRunningAMissionScript() ||
             PlayerRuntime::IsIntroSequenceLaunchPending())) {
            Logger::Log("Mission select ignored: a mission launch is already in progress (key='%.8s')",
                        entry->displayKey);
            PlayerRuntime::LaunchPrintNowStub("APBUSY");
            return;
        }
        if (g_state.parentPage == MenuPage::NewGame) {
            if (!HasPersistedApProgress()) {
                RunState::SetMapState(0);
                RunState::BeginNewRun();
                PlayerRuntime::QueueStartupApStateReset();
                PlayerRuntime::ResetMoneyPersistenceSession();
                PlayerRuntime::ResetPlayerPersistenceSession();
                Logger::Log("Run mode started from NewGame mission selection");
            } else {
                RunState::ActivateRun();
                Logger::Log("Run mode resumed from NewGame mission selection");
            }
            RunState::SetSkipIntroOnNextNewGame(false);
        }
        g_state.pendingMissionLaunch = entry;
        Logger::Log("Deferred mission launch armed for key='%.8s' actual index=%d variant=%d (afterPage=%d parent=%d)",
                    entry->displayKey, entry->actualIndex,
                    static_cast<int>(entry->launchVariant),
                    static_cast<int>(afterPage),
                    static_cast<int>(g_state.parentPage));
    }
}
