#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ApState.h"
#include "Config.h"
#include "GameAddresses.h"
#include "Logger.h"
#include "PluginPaths.h"
#include "RunState.h"

struct CRunningScript;

namespace ScriptRuntime {
    constexpr int kMaxMissionKeys = 128;
    constexpr int kThreadNameSize = 8;
    // CRunningScript layout (re3 PC build, MAX_STACK_DEPTH=6):
    //   0x00 next      0x04 prev      0x08 m_abScriptName[8]
    //   0x10 m_nIp     0x14 m_anStack[6]                  0x2C m_nStackPointer (u16)
    //   0x30 m_anLocalVariables[18]                       0x78 m_bCondResult
    //   0x79 m_bIsMissionScript    0x7A m_bSkipWakeTime   0x7C m_nWakeTime (u32)
    //   0x80 m_nAndOrState (u16)   0x82 m_bNotFlag
    //   0x83 m_bDeatharrestEnabled 0x84 m_bDeatharrestExecuted
    //   0x85 m_bMissionFlag        struct ends at 0x88
    constexpr std::ptrdiff_t kOff_Next                  = 0x00;
    constexpr std::ptrdiff_t kOff_ScriptName            = 0x08;
    constexpr std::ptrdiff_t kOff_Ip                    = 0x10;
    constexpr std::ptrdiff_t kOff_Stack                 = 0x14;
    constexpr std::ptrdiff_t kOff_StackPointer          = 0x2C;
    constexpr std::ptrdiff_t kOff_IsMissionScript       = 0x79;
    constexpr std::ptrdiff_t kOff_WakeTime              = 0x7C;
    constexpr std::ptrdiff_t kOff_DeatharrestEnabled    = 0x83;
    constexpr std::ptrdiff_t kOff_DeatharrestExecuted   = 0x84;
    constexpr std::ptrdiff_t kOff_MissionFlag           = 0x85;
    // Script bytecode layout (re3 GTA III, post PS2_160 PC build):
    //   ScriptSpace[0 .. SIZE_MAIN_SCRIPT)               = main.scm main chunk
    //   ScriptSpace[SIZE_MAIN_SCRIPT .. +SIZE_MISSION..) = currently running mission
    // MultiScriptArray entries are FILE offsets in main.scm — to launch a
    // mission we must first copy SIZE_MISSION_SCRIPT bytes from that file
    // offset into ScriptSpace[SIZE_MAIN_SCRIPT], then StartNewScript with
    // the memory IP SIZE_MAIN_SCRIPT.
    constexpr std::size_t kSizeMainScript    = 128 * 1024;
    constexpr std::size_t kSizeMissionScript = 32 * 1024;
    constexpr std::size_t kScriptNameScanBytes = 1024;
    constexpr std::uint8_t kScriptNameOpcodeLo = 0xA4;
    constexpr std::uint8_t kScriptNameOpcodeHi = 0x03;
    inline int g_AuthorizedMissionIndex = -1;
    inline int g_CachedMissionCount = -1;
    inline std::uint32_t g_CachedMissionTableSignature = 0;
    inline std::array<std::array<char, 9>, kMaxMissionKeys> g_CachedMissionThreadNames = {};

    enum class MissionBucket : std::uint8_t {
        Intro = 0,
        Luigi,
        Joey,
        Toni,
        Frankie,
        Diablo,
        Asuka,
        Kenji,
        Ray,
        DonaldLove,
        Yardie,
        Hood,
        Catalina,
        AsukaSuburban,
        Rc,
        OffRoad,
        MeatFactory,
        Mayhem,
    };

    enum class MissionLaunchVariant : std::uint8_t {
        Standard = 0,
        IntroToLuigisGirls,
        // Legacy name: this starts EIGHT at Give Me Liberty, then lets the
        // vanilla chain continue through Luigi's Girls / LM1 as one AP block.
        GiveMeLibertySplit,
        LuigisGirlsDirect,
    };

    struct VisibleMissionEntry {
        int actualIndex;
        const char* threadName;
        const char* displayKey;
        MissionBucket bucket;
        // A "bugged" mission cannot be cleanly launched/validated (e.g. the
        // combined 8ball.sc intro and the "Luigi's Girls" half folded into it):
        // it is shown with a (*) marker and counts as validated by default.
        bool bugged = false;
        // Some menu entries reuse a compiled mission slot but request a
        // different boot path inside that script.
        MissionLaunchVariant launchVariant = MissionLaunchVariant::Standard;
        // Synthetic selector entries can validate against a dedicated local key
        // instead of the compiled mission index / AP location.
        const char* syntheticValidationKey = nullptr;
    };

    // Ordered mission list mirrored from the current compiled main.scm mission table.
    inline constexpr VisibleMissionEntry kVisibleMissions[] = {
        // INTRO entry: selecting it no longer live-launches the cinematic
        // (that path rendered ghosted/offset no matter the staging). Instead
        // MenuPatch persists a boot-intro request and restarts the session
        // through the engine's own new-game machinery, so the cinematic plays
        // in pristine vanilla boot conditions. Must stay launchable
        // (bugged=false): with the fresh-run boot it is a gateway to Give Me
        // Liberty, an AP location that gates the Luigi missions.
        {2, "INTRO", "APINTRO", MissionBucket::Intro, false,
            MissionLaunchVariant::IntroToLuigisGirls, "APINTRO"},
        {5, "RC1", "RC1", MissionBucket::Rc},
        {6, "RC2", "RC2", MissionBucket::Rc},
        {7, "RC3", "RC4", MissionBucket::Rc},
        {8, "RC4", "RC3", MissionBucket::Rc},
        {9, "T4X4_1", "T4X4_1", MissionBucket::OffRoad},
        {10, "T4X4_2", "T4X4_2", MissionBucket::OffRoad},
        {11, "T4X4_3", "T4X4_3", MissionBucket::OffRoad},
        {12, "MAYHEM", "MM_1", MissionBucket::Mayhem},
        {17, "MEAT1", "MEA1", MissionBucket::MeatFactory},
        {18, "MEAT2", "MEA2", MissionBucket::MeatFactory},
        {19, "MEAT3", "MEA3", MissionBucket::MeatFactory},
        {20, "MEAT4", "MEA4", MissionBucket::MeatFactory},
        // mission 21 in main_ap.scm is the combined 8ball.sc script: it
        // opens with "Give Me Liberty" (PRINT_BIG 'EBAL') and chains straight
        // into "Luigi's Girls" (PRINT_BIG 'LM1') in the second half. There is
        // no standalone LUIGI1 entry in the mission table. Label the menu with
        // 'EBAL' so the user sees the same title the mission itself displays
        // when it starts ("À moi la liberté" in the French GXT).
        // "Les filles de Luigi" (Luigi's Girls) is the second half of the same
        // combined script 21 — no standalone index — so it is shown as a
        // synthetic entry pointing at 21, flagged bugged (validated by default).
        {22, "LUIGI2", "LM2", MissionBucket::Luigi},
        {23, "LUIGI3", "LM3", MissionBucket::Luigi},
        {24, "LUIGI4", "LM4", MissionBucket::Luigi},
        {25, "LUIGI5", "LM5", MissionBucket::Luigi},
        {26, "JOEY1", "JM1", MissionBucket::Joey},
        {27, "JOEY2", "JM2", MissionBucket::Joey},
        {28, "JOEY3", "JM3", MissionBucket::Joey},
        {29, "JOEY4", "JM4", MissionBucket::Joey},
        {30, "JOEY5", "JM5", MissionBucket::Joey},
        {31, "JOEY6", "JM6", MissionBucket::Joey},
        {32, "TONI1", "TM1", MissionBucket::Toni},
        {33, "TONI2", "TM2", MissionBucket::Toni},
        {34, "TONI3", "TM3", MissionBucket::Toni},
        {35, "TONI4", "TM4", MissionBucket::Toni},
        {36, "TONI5", "TM5", MissionBucket::Toni},
        {37, "FRANK1", "FM1", MissionBucket::Frankie},
        {38, "FRANK2", "FM2", MissionBucket::Frankie},
        {39, "FRANK21", "FM21", MissionBucket::Frankie},
        {40, "FRANK3", "FM3", MissionBucket::Frankie},
        {41, "FRANK4", "FM4", MissionBucket::Frankie},
        {42, "DIABLO1", "DIAB1", MissionBucket::Diablo},
        {43, "DIABLO2", "DIAB2", MissionBucket::Diablo},
        {44, "DIABLO3", "DIAB3", MissionBucket::Diablo},
        {45, "DIABLO4", "DIAB4", MissionBucket::Diablo},
        {46, "ASUKA1", "AM1", MissionBucket::Asuka},
        {47, "ASUKA2", "AM2", MissionBucket::Asuka},
        {48, "ASUKA3", "AM3", MissionBucket::Asuka},
        {49, "ASUKA4", "AM4", MissionBucket::Asuka},
        {50, "ASUKA5", "AM5", MissionBucket::Asuka},
        {51, "KENJI1", "KM1", MissionBucket::Kenji},
        {52, "KENJI2", "KM2", MissionBucket::Kenji},
        {53, "KENJI3", "KM3", MissionBucket::Kenji},
        {54, "KENJI4", "KM4", MissionBucket::Kenji},
        {55, "KENJI5", "KM5", MissionBucket::Kenji},
        {56, "RAY1", "RM1", MissionBucket::Ray},
        {57, "RAY2", "RM2", MissionBucket::Ray},
        {58, "RAY3", "RM3", MissionBucket::Ray},
        {59, "RAY4", "RM4", MissionBucket::Ray},
        {60, "RAY5", "RM5", MissionBucket::Ray},
        {61, "RAY6", "RM6", MissionBucket::Ray},
        {62, "LOVE1", "LOVE1", MissionBucket::DonaldLove},
        {63, "LOVE2", "LOVE2", MissionBucket::DonaldLove},
        {64, "LOVE3", "LOVE3", MissionBucket::DonaldLove},
        {65, "YARD1", "YD1", MissionBucket::Yardie},
        {66, "YARD2", "YD2", MissionBucket::Yardie},
        {67, "YARD3", "YD3", MissionBucket::Yardie},
        {68, "YARD4", "YD4", MissionBucket::Yardie},
        {69, "LOVE4", "LOVE4", MissionBucket::DonaldLove},
        {70, "LOVE5", "LOVE5", MissionBucket::DonaldLove},
        {71, "LOVE6", "LOVE6", MissionBucket::DonaldLove},
        {72, "LOVE7", "LOVE7", MissionBucket::DonaldLove},
        {73, "ASUSB1", "AS1", MissionBucket::AsukaSuburban},
        {74, "ASUSB2", "AS2", MissionBucket::AsukaSuburban},
        {75, "ASUSB3", "AS3", MissionBucket::AsukaSuburban},
        {76, "HOOD1", "HM_1", MissionBucket::Hood},
        {77, "HOOD2", "HM_2", MissionBucket::Hood},
        {78, "HOOD3", "HM_3", MissionBucket::Hood},
        {79, "HOOD4", "HM_4", MissionBucket::Hood},
        {80, "HOOD5", "HM_5", MissionBucket::Hood},
        {81, "CAT1", "CAT2", MissionBucket::Catalina},
    };

    // Archipelago location_id for each compiled mission (generated by joining
    // ARCHIPELAGO_CHECKS.csv register_tag with the kVisibleMissions displayKey).
    // Drives check emission (mission passed -> AddLocationChecked) and the (V)
    // marker (a location in the AP state shows validated). Intro and the
    // synthetic EBAL half have no canonical location and are intentionally absent.
    struct MissionLocation {
        int         actualIndex;
        const char* locationId;
    };
    inline constexpr MissionLocation kMissionLocations[] = {
        {5, "mission_rc1"},     {6, "mission_rc2"},     {7, "mission_rc3"},
        {8, "mission_rc4"},     {9, "mission_4x4_1"},   {10, "mission_4x4_2"},
        {11, "mission_4x4_3"},  {12, "mission_mayhem1"},{17, "mission_meat1"},
        {18, "mission_meat2"},  {19, "mission_meat3"},  {20, "mission_meat4"},
        {22, "mission_luigi2"}, {23, "mission_luigi3"},
        {24, "mission_luigi4"}, {25, "mission_luigi5"}, {26, "mission_joey1"},
        {27, "mission_joey2"},  {28, "mission_joey3"},  {29, "mission_joey4"},
        {30, "mission_joey5"},  {31, "mission_joey6"},  {32, "mission_toni1"},
        {33, "mission_toni2"},  {34, "mission_toni3"},  {35, "mission_toni4"},
        {36, "mission_toni5"},  {37, "mission_frank1"}, {38, "mission_frank2"},
        {39, "mission_frank2_1"},{40, "mission_frank3"},{41, "mission_frank4"},
        {42, "mission_diablo1"},{43, "mission_diablo2"},{44, "mission_diablo3"},
        {45, "mission_diablo4"},{46, "mission_asuka1"}, {47, "mission_asuka2"},
        {48, "mission_asuka3"}, {49, "mission_asuka4"}, {50, "mission_asuka5"},
        {51, "mission_kenji1"}, {52, "mission_kenji2"}, {53, "mission_kenji3"},
        {54, "mission_kenji4"}, {55, "mission_kenji5"}, {56, "mission_ray1"},
        {57, "mission_ray2"},   {58, "mission_ray3"},   {59, "mission_ray4"},
        {60, "mission_ray5"},   {61, "mission_ray6"},   {62, "mission_love1"},
        {63, "mission_love2"},  {64, "mission_love3"},  {65, "mission_yard1"},
        {66, "mission_yard2"},  {67, "mission_yard3"},  {68, "mission_yard4"},
        {69, "mission_love4"},  {70, "mission_love5"},  {71, "mission_love6"},
        {72, "mission_love7"},  {73, "mission_asusb1"}, {74, "mission_asusb2"},
        {75, "mission_asusb3"}, {76, "mission_hood1"},  {77, "mission_hood2"},
        {78, "mission_hood3"},  {79, "mission_hood4"},  {80, "mission_hood5"},
        {81, "mission_cat1"},
    };

    struct SyntheticMissionLocation {
        const char* syntheticKey;
        const char* locationId;
    };
    inline constexpr SyntheticMissionLocation kSyntheticMissionLocations[] = {
        {"APGMLIB", "mission_give_me_liberty"},
        {"APINTRO", "mission_give_me_liberty"},
    };

    inline bool IsIntroSequenceSyntheticKey(const char* syntheticKey) {
        return syntheticKey &&
               syntheticKey[0] != 0 &&
               std::strncmp(syntheticKey, "APINTRO", 8) == 0;
    }

    inline bool IsGiveMeLibertySyntheticKey(const char* syntheticKey) {
        return syntheticKey &&
               syntheticKey[0] != 0 &&
               std::strncmp(syntheticKey, "APGMLIB", 8) == 0;
    }

    inline bool IsIntroOrGiveMeLibertySyntheticKey(const char* syntheticKey) {
        return IsIntroSequenceSyntheticKey(syntheticKey) ||
               IsGiveMeLibertySyntheticKey(syntheticKey);
    }

    inline bool IsSyntheticMissionLocationChecked(const char* syntheticKey) {
        if (IsIntroSequenceSyntheticKey(syntheticKey)) {
            return ApState::IsLocationChecked("mission_give_me_liberty");
        }
        if (!syntheticKey || syntheticKey[0] == 0) {
            return false;
        }
        for (const SyntheticMissionLocation& e : kSyntheticMissionLocations) {
            if (std::strncmp(e.syntheticKey, syntheticKey, 8) == 0) {
                return ApState::IsLocationChecked(e.locationId);
            }
        }
        return false;
    }

    inline const char* MissionLocationId(int actualIndex) {
        for (const MissionLocation& e : kMissionLocations) {
            if (e.actualIndex == actualIndex) {
                return e.locationId;
            }
        }
        return nullptr;
    }

    inline bool BucketUsesMissionUnlockItems(MissionBucket bucket) {
        switch (bucket) {
            case MissionBucket::Luigi:
            case MissionBucket::Joey:
            case MissionBucket::Toni:
            case MissionBucket::Frankie:
            case MissionBucket::Diablo:
            case MissionBucket::Asuka:
            case MissionBucket::Kenji:
            case MissionBucket::Ray:
            case MissionBucket::DonaldLove:
            case MissionBucket::Yardie:
            case MissionBucket::Hood:
            case MissionBucket::Catalina:
            case MissionBucket::AsukaSuburban:
                return true;
            default:
                return false;
        }
    }

    inline bool HasMissionUnlockItem(int actualIndex) {
        const char* loc = MissionLocationId(actualIndex);
        if (!loc || loc[0] == '\0') {
            return true;
        }
        return ApState::HasItem(std::string("unlock_") + loc);
    }

    inline const char* SyntheticMissionLocationId(const char* syntheticKey) {
        if (!syntheticKey || syntheticKey[0] == 0) {
            return nullptr;
        }
        for (const SyntheticMissionLocation& e : kSyntheticMissionLocations) {
            if (std::strncmp(e.syntheticKey, syntheticKey, 8) == 0) {
                return e.locationId;
            }
        }
        return nullptr;
    }

    // AP item-id suffix (chain name) for a contact bucket, e.g. Joey -> "joey".
    // The unlock item is "unlock_<suffix>" (matches ARCHIPELAGO_CHECKS chains).
    inline const char* BucketChain(MissionBucket bucket) {
        switch (bucket) {
            case MissionBucket::Luigi:         return "luigi";
            case MissionBucket::Joey:          return "joey";
            case MissionBucket::Toni:          return "toni";
            case MissionBucket::Frankie:       return "frankie";
            case MissionBucket::Diablo:        return "diablo";
            case MissionBucket::Asuka:         return "asuka";
            case MissionBucket::Kenji:         return "kenji";
            case MissionBucket::Ray:           return "ray";
            case MissionBucket::DonaldLove:    return "donald_love";
            case MissionBucket::Yardie:        return "yardie";
            case MissionBucket::Hood:          return "hood";
            case MissionBucket::Catalina:      return "catalina";
            case MissionBucket::AsukaSuburban: return "asuka_suburban";
            case MissionBucket::Rc:            return "rc";
            case MissionBucket::OffRoad:       return "4x4";
            case MissionBucket::MeatFactory:   return "meat_factory";
            case MissionBucket::Mayhem:        return "mayhem";
            default:                           return nullptr;
        }
    }

    // A mission's AP location has been checked (in III.Archipelago.state.json).
    inline bool IsMissionLocationChecked(int actualIndex) {
        const char* loc = MissionLocationId(actualIndex);
        return loc && ApState::IsLocationChecked(loc);
    }

    // Highest island opened by Archipelago "unlock region" items, in the same
    // 0/1/2 scale as RunState::MapState (0=Portland only, 1=+Staunton/commercial,
    // 2=+Shoreside/suburban). Used to drive the SCM bridge/gate unlock sequence.
    inline int ApRegionMapState() {
        if (ApState::HasItem("unlock_suburban"))  return 2;
        if (ApState::HasItem("unlock_commercial")) return 1;
        return 0;
    }

    using StartNewScriptFn      = CRunningScript* (__cdecl*)(std::uint32_t ip);
    using LinkScriptFn          = void(__thiscall*)(CRunningScript* script, CRunningScript** listHead);
    using ProcessOneCommandFn   = bool(__thiscall*)(void* self);
    using ClearMessagesFn       = void(__cdecl*)();

    inline StartNewScriptFn StartNewScript() {
        return reinterpret_cast<StartNewScriptFn>(
            GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
    }

    inline std::int32_t* MissionOffsets() {
        return reinterpret_cast<std::int32_t*>(
            GameAddr::Translate(GameAddr::CTheScripts_MultiScriptArray));
    }

    inline std::uint8_t* ScriptSpace() {
        return reinterpret_cast<std::uint8_t*>(
            GameAddr::Translate(GameAddr::CTheScripts_ScriptSpace));
    }

    inline std::uint16_t& NumberOfMissionScripts() {
        return *reinterpret_cast<std::uint16_t*>(
            GameAddr::Translate(GameAddr::CTheScripts_NumberOfMissionScripts));
    }

    inline std::uint32_t& OnAMissionFlagOffset() {
        return *reinterpret_cast<std::uint32_t*>(
            GameAddr::Translate(GameAddr::CTheScripts_OnAMissionFlag));
    }

    inline CRunningScript*& ActiveScripts() {
        return *reinterpret_cast<CRunningScript**>(
            GameAddr::Translate(GameAddr::CTheScripts_pActiveScripts));
    }

    inline CRunningScript*& IdleScripts() {
        return *reinterpret_cast<CRunningScript**>(
            GameAddr::Translate(GameAddr::CTheScripts_pIdleScripts));
    }

    inline bool HasMissionTable() {
        const std::uint16_t missionCount = NumberOfMissionScripts();
        if (missionCount == 0 || missionCount > kMaxMissionKeys) {
            return false;
        }

        const std::uint32_t onMissionOffset = OnAMissionFlagOffset();
        return onMissionOffset != 0 &&
               onMissionOffset + sizeof(std::int32_t) <= 0x80000;
    }

    inline LinkScriptFn RemoveFromList() {
        return reinterpret_cast<LinkScriptFn>(
            GameAddr::Translate(GameAddr::CRunningScript_RemoveFromList));
    }

    inline LinkScriptFn AddToList() {
        return reinterpret_cast<LinkScriptFn>(
            GameAddr::Translate(GameAddr::CRunningScript_AddToList));
    }

    inline bool HasLiveScriptEngine() {
        return HasMissionTable() &&
               ActiveScripts() != nullptr &&
               IdleScripts() != nullptr;
    }

    inline int MissionCount() {
        if (!HasMissionTable()) {
            return 0;
        }
        return std::min<int>(NumberOfMissionScripts(), kMaxMissionKeys);
    }

    inline std::int32_t MissionOffset(int index) {
        if (index < 0 || index >= MissionCount()) {
            return 0;
        }
        return MissionOffsets()[index];
    }

    inline int FindMissionIndexByOffset(std::uint32_t offset) {
        for (int index = 0; index < MissionCount(); ++index) {
            if (static_cast<std::uint32_t>(MissionOffset(index)) == offset) {
                return index;
            }
        }
        return -1;
    }

    inline bool CopyToken(const char* source, char (&out)[9]) {
        if (!source || source[0] == '\0') {
            return false;
        }
        std::memset(out, 0, sizeof(out));
        std::memcpy(out, source, std::min<std::size_t>(std::strlen(source), sizeof(out) - 1));
        return true;
    }

    inline std::uint32_t MissionTableSignature() {
        const int missionCount = MissionCount();
        std::uint32_t signature = 2166136261u ^ static_cast<std::uint32_t>(missionCount);
        for (int index = 0; index < missionCount; ++index) {
            signature ^= static_cast<std::uint32_t>(MissionOffset(index));
            signature *= 16777619u;
        }
        return signature;
    }

    inline bool ReadMissionThreadName(std::FILE* file,
                                      std::int32_t fileOffset,
                                      char (&out)[9]) {
        std::memset(out, 0, sizeof(out));
        if (!file || fileOffset <= 0) {
            return false;
        }
        if (_fseeki64(file, fileOffset, SEEK_SET) != 0) {
            return false;
        }

        std::uint8_t buffer[kScriptNameScanBytes] = {};
        const std::size_t bytesRead = std::fread(buffer, 1, sizeof(buffer), file);
        if (bytesRead < 2 + kThreadNameSize) {
            return false;
        }

        for (std::size_t i = 0; i + 2 + kThreadNameSize <= bytesRead; ++i) {
            if (buffer[i] != kScriptNameOpcodeLo || buffer[i + 1] != kScriptNameOpcodeHi) {
                continue;
            }
            std::memcpy(out, buffer + i + 2, kThreadNameSize);
            out[kThreadNameSize] = '\0';
            for (int c = 0; c < kThreadNameSize; ++c) {
                const unsigned char ch = static_cast<unsigned char>(out[c]);
                if (ch == 0) {
                    break;
                }
                if (ch < 32 || ch > 126) {
                    out[c] = '\0';
                    break;
                }
            }
            return out[0] != '\0';
        }
        return false;
    }

    inline void RefreshMissionThreadNameCache() {
        if (!HasMissionTable()) {
            g_CachedMissionCount = -1;
            g_CachedMissionTableSignature = 0;
            for (auto& name : g_CachedMissionThreadNames) {
                name.fill(0);
            }
            return;
        }

        const int missionCount = MissionCount();
        const std::uint32_t signature = MissionTableSignature();
        if (g_CachedMissionCount == missionCount &&
            g_CachedMissionTableSignature == signature) {
            return;
        }

        for (auto& name : g_CachedMissionThreadNames) {
            name.fill(0);
        }

        const std::string path = PluginPaths::InGameDir("data\\main.scm");
        std::FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "rb");
        if (!file) {
            Logger::Log("Mission thread cache: failed to open '%s'", path.c_str());
            g_CachedMissionCount = missionCount;
            g_CachedMissionTableSignature = signature;
            return;
        }

        for (int index = 0; index < missionCount; ++index) {
            char name[9] = {};
            if (!ReadMissionThreadName(file, MissionOffset(index), name)) {
                continue;
            }
            std::memcpy(g_CachedMissionThreadNames[index].data(), name, sizeof(name));
        }
        std::fclose(file);

        g_CachedMissionCount = missionCount;
        g_CachedMissionTableSignature = signature;
        Logger::Log("Mission thread cache rebuilt: missions=%d signature=0x%08X",
                    missionCount, signature);
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (!entry.threadName || entry.threadName[0] == '\0') {
                continue;
            }
            for (int index = 0; index < missionCount; ++index) {
                const char* cached = g_CachedMissionThreadNames[index].data();
                if (cached[0] == '\0' ||
                    _strnicmp(cached, entry.threadName, kThreadNameSize) != 0) {
                    continue;
                }
                if (index != entry.actualIndex) {
                    Logger::Log(
                        "Mission thread remap: key='%.8s' thread='%.8s' semantic=%d compiled=%d",
                        entry.displayKey ? entry.displayKey : "",
                        entry.threadName,
                        entry.actualIndex,
                        index);
                }
                break;
            }
        }
    }

    inline int FindCompiledMissionIndexByThreadName(const char* threadName) {
        if (!threadName || threadName[0] == '\0') {
            return -1;
        }
        RefreshMissionThreadNameCache();
        for (int index = 0; index < MissionCount(); ++index) {
            const char* cached = g_CachedMissionThreadNames[index].data();
            if (cached[0] != '\0' &&
                _strnicmp(cached, threadName, kThreadNameSize) == 0) {
                return index;
            }
        }
        return -1;
    }

    // The compiled main.scm mission table is a uniform shift of the semantic
    // actualIndex used in kVisibleMissions (the AP build collapses an early
    // vanilla slot, so compiled = semantic - 1 across the whole table). Derive
    // that shift empirically from the kVisibleMissions entry NEAREST to
    // actualIndex whose thread name DID resolve in the compiled table. Returns 0
    // when nothing resolves (degrades to the raw actualIndex fallback).
    inline int NearestResolvedMissionShift(int actualIndex) {
        int bestDist = 1 << 30;
        int bestShift = 0;
        for (const VisibleMissionEntry& e : kVisibleMissions) {
            const int compiled = FindCompiledMissionIndexByThreadName(e.threadName);
            if (compiled < 0) {
                continue;
            }
            const int dist = e.actualIndex > actualIndex
                ? e.actualIndex - actualIndex
                : actualIndex - e.actualIndex;
            if (dist < bestDist) {
                bestDist = dist;
                bestShift = compiled - e.actualIndex;
            }
        }
        return bestShift;
    }

    inline int ResolveCompiledMissionIndex(const VisibleMissionEntry& entry) {
        const int byThreadName = FindCompiledMissionIndexByThreadName(entry.threadName);
        if (byThreadName >= 0) {
            return byThreadName;
        }
        // Thread-name match failed (some compiled missions carry a malformed /
        // unreadable SCRIPT_NAME — the Joey block in the current main_ap.scm).
        // Falling back to the raw actualIndex lands one slot off in a shifted
        // table (JM6 -> TONI1). Apply the shift observed on the nearest mission
        // that DID resolve, so a failed match still maps to the right slot.
        const int shift = NearestResolvedMissionShift(entry.actualIndex);
        const int shifted = entry.actualIndex + shift;
        if (shift != 0 && shifted >= 0 && shifted < MissionCount() &&
            MissionOffset(shifted) > 0) {
            return shifted;
        }
        if (entry.actualIndex >= 0 &&
            entry.actualIndex < MissionCount() &&
            MissionOffset(entry.actualIndex) > 0) {
            return entry.actualIndex;
        }
        return -1;
    }

    // A mission is "compiled" when it actually exists in the loaded main.scm
    // mission table (referenced by LAUNCH_MISSION, with a valid file offset).
    inline bool IsVisibleMissionCompiled(const VisibleMissionEntry& entry) {
        const int compiledIndex = ResolveCompiledMissionIndex(entry);
        return compiledIndex >= 0 && MissionOffset(compiledIndex) > 0;
    }

    // Total unlockable characters (contacts): every MissionBucket except Intro,
    // i.e. Luigi..Mayhem. Mayhem is the last enumerator, so its value == count.
    constexpr int kCharacterCount = static_cast<int>(MissionBucket::Mayhem);

    // A character (contact) is unlocked when EITHER the Archipelago state granted
    // its "unlock_<chain>" item, OR the local sequential counter reached it (the
    // F2 debug key / non-AP fallback). Intro is the fixed bootstrap bucket: it is
    // always available from New Game and still hidden from pause-menu launch by
    // MenuPatch.
    inline bool IsCharacterUnlocked(MissionBucket bucket) {
        if (bucket == MissionBucket::Intro) {
            return true;
        }
        const char* chain = BucketChain(bucket);
        if (chain && ApState::HasItem(std::string("unlock_") + chain)) {
            return true;
        }
        if (RunState::IsBucketUnlocked(static_cast<int>(bucket))) {
            return true;
        }
        return static_cast<int>(bucket) - 1 < RunState::UnlockedCharacters();
    }

    // Every compiled mission is shown in the selector; its lock/validated state
    // is conveyed by a marker on the label (see MenuPatch), not by hiding it.
    // IsCharacterUnlocked drives the locked (✗) vs available (☐) marker.
    inline bool IsVisibleMissionAvailable(const VisibleMissionEntry& entry) {
        return IsVisibleMissionCompiled(entry);
    }

    inline bool IsMissionEntryValidated(const VisibleMissionEntry& entry);

    inline const VisibleMissionEntry* FindPreviousMissionInBucket(
        const VisibleMissionEntry& entry) {
        const VisibleMissionEntry* previous = nullptr;
        for (const VisibleMissionEntry& candidate : kVisibleMissions) {
            if (!IsVisibleMissionAvailable(candidate) || candidate.bucket != entry.bucket) {
                continue;
            }
            if (&candidate == &entry) {
                return previous;
            }
            previous = &candidate;
        }
        return nullptr;
    }

    inline bool HasAtomicMissionSeed(MissionBucket bucket) {
        if (bucket == MissionBucket::Intro) {
            return true;
        }
        if (bucket == MissionBucket::Luigi) {
            return RunState::IsSyntheticMissionValidated("APINTRO") ||
                   IsSyntheticMissionLocationChecked("APINTRO");
        }
        // Story branches can expose a second contact while the current contact
        // still has missions left. In particular LM3 (Drive Misty For Me)
        // unlocks Joey while LM4 also becomes playable, so both street markers
        // must be active concurrently rather than waiting on one global seed.
        int storyGate = -1;
        switch (bucket) {
            case MissionBucket::Joey:
            case MissionBucket::MeatFactory: storyGate = 23; break; // LM3
            case MissionBucket::Toni:        storyGate = 29; break; // JM4
            case MissionBucket::Frankie:     storyGate = 34; break; // TM3
            case MissionBucket::Asuka:       storyGate = 41; break; // FM4
            default: break;
        }
        if (storyGate >= 0 &&
            (RunState::IsMissionValidated(storyGate) ||
             IsMissionLocationChecked(storyGate))) {
            return true;
        }
        return IsCharacterUnlocked(bucket);
    }

    inline bool IsMissionEntryUnlocked(const VisibleMissionEntry& entry) {
        if (!IsVisibleMissionAvailable(entry)) {
            return false;
        }
        if (Config::RequireApMissionUnlockItems()) {
            const bool usesMissionItem = BucketUsesMissionUnlockItems(entry.bucket);
            if (!usesMissionItem) {
                return true;
            }
            if (!HasMissionUnlockItem(entry.actualIndex)) {
                return false;
            }
            const VisibleMissionEntry* previous = FindPreviousMissionInBucket(entry);
            if (!previous) {
                return entry.bucket == MissionBucket::Luigi
                    ? HasAtomicMissionSeed(entry.bucket)
                    : true;
            }
            return IsMissionEntryValidated(*previous);
        }
        if (!Config::UnlockMissionsAtomically() && !Config::UseClassicMissionMarkers()) {
            return IsCharacterUnlocked(entry.bucket);
        }
        const VisibleMissionEntry* previous = FindPreviousMissionInBucket(entry);
        if (!previous) {
            return HasAtomicMissionSeed(entry.bucket);
        }
        return IsMissionEntryValidated(*previous);
    }

    inline bool HasUnlockedMissionInBucket(MissionBucket bucket) {
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (entry.bucket == bucket && IsMissionEntryUnlocked(entry)) {
                return true;
            }
        }
        return false;
    }

    inline bool IsMissionEntryValidated(const VisibleMissionEntry& entry);

    // "Playable" = the contact's marker would accept a launch right now: the
    // LOWEST mission of the chain that is unlocked AND not yet validated
    // (kVisibleMissions is in chain order). Drives the contact radar blips
    // and the marker proximity trigger in classic-marker mode.
    inline const VisibleMissionEntry* FindPlayableMissionInBucket(MissionBucket bucket) {
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (entry.bucket == bucket && !entry.bugged &&
                IsMissionEntryUnlocked(entry) && !IsMissionEntryValidated(entry)) {
                return &entry;
            }
        }
        return nullptr;
    }

    // First mission in the contact chain that still needs validation, whether
    // it is currently unlocked or not. Used to explain a blocked street marker
    // with the exact mission/item instead of a generic "item missing" toast.
    inline const VisibleMissionEntry* FindNextUnvalidatedMissionInBucket(MissionBucket bucket) {
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (entry.bucket == bucket && !entry.bugged &&
                IsVisibleMissionAvailable(entry) && !IsMissionEntryValidated(entry)) {
                return &entry;
            }
        }
        return nullptr;
    }

    inline bool HasPlayableMissionInBucket(MissionBucket bucket) {
        return FindPlayableMissionInBucket(bucket) != nullptr;
    }

    // A bugged mission is validated by default; otherwise consult the persisted
    // validated set. Drives the (*) / (V) markers and the contact/region rollups.
    inline bool IsMissionEntryValidated(const VisibleMissionEntry& entry) {
        if (entry.bugged) {
            return true;
        }
        if (entry.syntheticValidationKey && entry.syntheticValidationKey[0] != 0) {
            if (RunState::IsSyntheticMissionValidated(entry.syntheticValidationKey)) {
                return true;
            }
            return IsSyntheticMissionLocationChecked(entry.syntheticValidationKey);
        }
        return RunState::IsMissionValidated(entry.actualIndex) ||
               IsMissionLocationChecked(entry.actualIndex);
    }

    inline bool MatchesBucket(const VisibleMissionEntry& entry, MissionBucket bucket) {
        return entry.bucket == bucket;
    }

    inline const VisibleMissionEntry* FindVisibleMissionByActualIndex(int actualIndex) {
        const VisibleMissionEntry* fallback = nullptr;
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (entry.actualIndex == actualIndex) {
                if (!fallback) {
                    fallback = &entry;
                }
                if (!entry.syntheticValidationKey || entry.syntheticValidationKey[0] == 0) {
                    return &entry;
                }
            }
        }
        return fallback;
    }

    inline const VisibleMissionEntry* FindVisibleMissionByThreadName(const char* threadName) {
        if (!threadName || threadName[0] == '\0') {
            return nullptr;
        }
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (!entry.threadName || entry.threadName[0] == '\0') {
                continue;
            }
            if (_strnicmp(entry.threadName, threadName, kThreadNameSize) == 0) {
                return &entry;
            }
        }
        return nullptr;
    }

    inline const VisibleMissionEntry* FindVisibleMissionBySyntheticValidationKey(
        const char* syntheticKey) {
        if (!syntheticKey || syntheticKey[0] == '\0') {
            return nullptr;
        }
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (!entry.syntheticValidationKey || entry.syntheticValidationKey[0] == '\0') {
                continue;
            }
            if (std::strncmp(entry.syntheticValidationKey, syntheticKey, 8) == 0) {
                return &entry;
            }
        }
        return nullptr;
    }

    inline const VisibleMissionEntry* FindVisibleMissionByDisplayKey(
        const char* displayKey,
        MissionBucket bucket) {
        if (!displayKey || displayKey[0] == '\0') {
            return nullptr;
        }
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (!IsVisibleMissionAvailable(entry) || !MatchesBucket(entry, bucket)) {
                continue;
            }
            if (std::strncmp(entry.displayKey, displayKey, 8) == 0) {
                return &entry;
            }
        }
        return nullptr;
    }

    inline const VisibleMissionEntry* FindVisibleMissionByVisibleIndex(
        int visibleIndex,
        MissionBucket bucket) {
        if (visibleIndex < 0) {
            return nullptr;
        }

        int visibleCount = 0;
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (!IsVisibleMissionAvailable(entry) || !MatchesBucket(entry, bucket)) {
                continue;
            }
            if (visibleCount == visibleIndex) {
                return &entry;
            }
            ++visibleCount;
        }
        return nullptr;
    }

    inline bool TryGetMissionDisplayKeyByActualIndex(int actualIndex, char (&out)[9]) {
        const VisibleMissionEntry* entry = FindVisibleMissionByActualIndex(actualIndex);
        if (!entry || !IsVisibleMissionAvailable(*entry)) {
            return false;
        }
        return CopyToken(entry->displayKey, out);
    }

    inline int VisibleMissionCount(MissionBucket bucket) {
        int count = 0;
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (IsVisibleMissionAvailable(entry) && MatchesBucket(entry, bucket)) {
                ++count;
            }
        }
        return count;
    }

    inline int VisibleToActualMissionIndex(
        int visibleIndex,
        MissionBucket bucket) {
        const VisibleMissionEntry* entry = FindVisibleMissionByVisibleIndex(visibleIndex, bucket);
        return entry ? entry->actualIndex : -1;
    }

    inline bool TryGetMissionDisplayKey(
        int visibleIndex,
        char (&out)[9],
        MissionBucket bucket) {
        const VisibleMissionEntry* entry = FindVisibleMissionByVisibleIndex(visibleIndex, bucket);
        if (!entry) {
            return false;
        }
        return CopyToken(entry->displayKey, out);
    }

    inline bool TryGetFirstMissionDisplayKeyForBucket(
        MissionBucket bucket,
        char (&out)[9]) {
        for (const VisibleMissionEntry& entry : kVisibleMissions) {
            if (entry.bucket == bucket && IsVisibleMissionAvailable(entry)) {
                return CopyToken(entry.displayKey, out);
            }
        }
        return false;
    }

    inline bool IsPlayerOnMission() {
        const std::uint32_t offset = OnAMissionFlagOffset();
        if (offset == 0 || offset + sizeof(std::int32_t) > 0x80000) {
            return false;
        }
        return *reinterpret_cast<std::int32_t*>(ScriptSpace() + offset) == 1;
    }

    inline void SetPlayerOnMission(bool value) {
        const std::uint32_t offset = OnAMissionFlagOffset();
        if (offset == 0 || offset + sizeof(std::int32_t) > 0x80000) {
            return;
        }
        *reinterpret_cast<std::int32_t*>(ScriptSpace() + offset) = value ? 1 : 0;
    }

    inline CRunningScript* Next(CRunningScript* script) {
        if (!script) {
            return nullptr;
        }
        return *reinterpret_cast<CRunningScript**>(
            reinterpret_cast<std::uint8_t*>(script) + kOff_Next);
    }

    inline bool IsMissionScript(CRunningScript* script) {
        if (!script) {
            return false;
        }
        return *reinterpret_cast<bool*>(
            reinterpret_cast<std::uint8_t*>(script) + kOff_IsMissionScript);
    }

    inline void MarkAsMissionScript(CRunningScript* script, bool value = true) {
        if (!script) {
            return;
        }
        *reinterpret_cast<bool*>(reinterpret_cast<std::uint8_t*>(script) + kOff_IsMissionScript) = value;
    }

    inline void SetMissionFlag(CRunningScript* script, bool value = true) {
        if (!script) {
            return;
        }
        *reinterpret_cast<bool*>(reinterpret_cast<std::uint8_t*>(script) + kOff_MissionFlag) = value;
    }

    inline bool& AlreadyRunningAMissionScript() {
        return *reinterpret_cast<bool*>(
            GameAddr::Translate(GameAddr::CTheScripts_bAlreadyRunningAMissionScript));
    }

    inline std::uint8_t& FailCurrentMission() {
        return *reinterpret_cast<std::uint8_t*>(
            GameAddr::Translate(GameAddr::CTheScripts_FailCurrentMission));
    }

    inline void AuthorizeMissionLaunch(int missionIndex) {
        g_AuthorizedMissionIndex = missionIndex;
    }

    inline void ClearAuthorizedMissionLaunch() {
        g_AuthorizedMissionIndex = -1;
    }

    inline int PeekAuthorizedMissionLaunch() {
        return g_AuthorizedMissionIndex;
    }

    inline bool ConsumeAuthorizedMissionLaunch(int missionIndex) {
        if (g_AuthorizedMissionIndex != missionIndex) {
            return false;
        }
        g_AuthorizedMissionIndex = -1;
        return true;
    }

    inline void AbortScript(CRunningScript* script) {
        if (!script) {
            return;
        }
        RemoveFromList()(script, &ActiveScripts());
        AddToList()(script, &IdleScripts());
        MarkAsMissionScript(script, false);
    }

    inline int AbortMissionScripts() {
        int aborted = 0;
        for (CRunningScript* script = ActiveScripts(); script != nullptr;) {
            CRunningScript* next = Next(script);
            if (IsMissionScript(script)) {
                AbortScript(script);
                ++aborted;
            }
            script = next;
        }
        if (aborted > 0) {
            SetPlayerOnMission(false);
        }
        return aborted;
    }

    inline ProcessOneCommandFn ProcessOneCommand() {
        return reinterpret_cast<ProcessOneCommandFn>(
            GameAddr::Translate(GameAddr::CRunningScript_ProcessOneCommand));
    }

    inline ClearMessagesFn ClearMessages() {
        return reinterpret_cast<ClearMessagesFn>(
            GameAddr::Translate(GameAddr::CMessages_ClearMessages));
    }

    // Re3 CTheScripts::SwitchToMission abort sequence: for every active
    // m_bIsMissionScript that still has deatharrest enabled, unwind its
    // stack and run its own deatharrest cleanup via ProcessOneCommand. This
    // is what releases the peds/cars/objects/blips/timers the mission
    // owns, and lets the script call TERMINATE_THIS_SCRIPT cleanly. The
    // safety counter prevents an infinite loop in case of a malformed
    // script (re3 itself doesn't bound the loop, but we want to log+bail
    // instead of hanging the game).
    inline int ProperAbortMissionScripts() {
        int aborted = 0;
        for (CRunningScript* script = ActiveScripts(); script != nullptr;) {
            CRunningScript* nextScript = Next(script);

            auto base = reinterpret_cast<std::uint8_t*>(script);
            const bool isMission =
                *reinterpret_cast<bool*>(base + kOff_IsMissionScript);
            const bool deatharrestEnabled =
                *reinterpret_cast<bool*>(base + kOff_DeatharrestEnabled);
            if (!isMission || !deatharrestEnabled) {
                script = nextScript;
                continue;
            }

            const char* name = reinterpret_cast<const char*>(base + kOff_ScriptName);
            auto stackPointer =
                reinterpret_cast<std::uint16_t*>(base + kOff_StackPointer);
            auto stack =
                reinterpret_cast<std::uint32_t*>(base + kOff_Stack);
            auto ip = reinterpret_cast<std::uint32_t*>(base + kOff_Ip);
            const std::uint16_t spBefore = *stackPointer;
            const std::uint32_t ipBefore = *ip;
            const std::uint32_t stack0    = stack[0];

            Logger::Log("  abort %.8s: ip_before=%u sp=%u stack[0]=%u",
                        name, ipBefore, spBefore, stack0);

            // Unwind stack to depth 0, set IP to stack[0] (re-entry into
            // the script's top-level handler that checks deatharrest).
            *stackPointer = 0;
            *ip = stack[0];

            SetPlayerOnMission(false);

            *reinterpret_cast<std::uint32_t*>(base + kOff_WakeTime) = 0;
            *reinterpret_cast<bool*>(base + kOff_DeatharrestExecuted) = true;

            auto processOneCommand = ProcessOneCommand();
            int steps = 0;
            bool terminated = false;
            bool excepted = false;
            constexpr int kMaxSteps = 50000;
            __try {
                while (steps < kMaxSteps) {
                    ++steps;
                    if (processOneCommand(script)) {
                        terminated = true;
                        break;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                excepted = true;
            }

            Logger::Log("  abort %.8s: steps=%d terminated=%d excepted=%d",
                        name, steps, terminated ? 1 : 0, excepted ? 1 : 0);

            __try {
                ClearMessages()();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("  abort %.8s: ClearMessages raised exception", name);
            }

            ++aborted;
            script = nextScript;
        }
        return aborted;
    }
}
