#pragma once

#include "MissionManager.h"

namespace MissionTable {
    // Maps a main.scm mission index (GTA III 1.0) to its Archipelago location.
    struct Entry {
        MissionManager::MissionId missionId;
        const char* location;
    };

    // Stable logical IDs inherited from the old mod. The vanilla SCM has two
    // fewer slots; translate only at the native opcode boundary.
    inline constexpr int NativeIndex(int logical) {
        return logical == 1000 ? 19 : (logical >= 2 && logical <= 81 ? logical - 2 : -1);
    }
    inline constexpr int LogicalIndex(int native) {
        return native >= 0 && native < 80 ? native + 2 : -1;
    }
    inline constexpr Entry kEntries[] = {
        {1000, "mission_give_me_liberty"}, {21, "mission_8ball"},
        {5, "mission_rc1"},       {6, "mission_rc2"},       {7, "mission_rc3"},
        {8, "mission_rc4"},       {9, "mission_4x4_1"},     {10, "mission_4x4_2"},
        {11, "mission_4x4_3"},    {12, "mission_mayhem1"},  {17, "mission_meat1"},
        {18, "mission_meat2"},    {19, "mission_meat3"},    {20, "mission_meat4"},
        {22, "mission_luigi2"},   {23, "mission_luigi3"},   {24, "mission_luigi4"},
        {25, "mission_luigi5"},   {26, "mission_joey1"},    {27, "mission_joey2"},
        {28, "mission_joey3"},    {29, "mission_joey4"},    {30, "mission_joey5"},
        {31, "mission_joey6"},    {32, "mission_toni1"},    {33, "mission_toni2"},
        {34, "mission_toni3"},    {35, "mission_toni4"},    {36, "mission_toni5"},
        {37, "mission_frank1"},   {38, "mission_frank2"},   {39, "mission_frank2_1"},
        {40, "mission_frank3"},   {41, "mission_frank4"},   {42, "mission_diablo1"},
        {43, "mission_diablo2"},  {44, "mission_diablo3"},  {45, "mission_diablo4"},
        {46, "mission_asuka1"},   {47, "mission_asuka2"},   {48, "mission_asuka3"},
        {49, "mission_asuka4"},   {50, "mission_asuka5"},   {51, "mission_kenji1"},
        {52, "mission_kenji2"},   {53, "mission_kenji3"},   {54, "mission_kenji4"},
        {55, "mission_kenji5"},   {56, "mission_ray1"},     {57, "mission_ray2"},
        {58, "mission_ray3"},     {59, "mission_ray4"},     {60, "mission_ray5"},
        {61, "mission_ray6"},     {62, "mission_love1"},    {63, "mission_love2"},
        {64, "mission_love3"},    {65, "mission_yard1"},    {66, "mission_yard2"},
        {67, "mission_yard3"},    {68, "mission_yard4"},    {69, "mission_love4"},
        {70, "mission_love5"},    {71, "mission_love6"},    {72, "mission_love7"},
        {73, "mission_asusb1"},   {74, "mission_asusb2"},   {75, "mission_asusb3"},
        {76, "mission_hood1"},    {77, "mission_hood2"},    {78, "mission_hood3"},
        {79, "mission_hood4"},    {80, "mission_hood5"},    {81, "mission_cat1"},
    };

    // Returns the Archipelago location of a mission, or nullptr if it has none.
    inline const char* LocationFor(MissionManager::MissionId missionId) {
        for (const Entry& entry : kEntries) {
            if (entry.missionId == missionId) {
                return entry.location;
            }
        }
        return nullptr;
    }
}
