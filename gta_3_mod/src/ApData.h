#pragma once

#include <string>

// Mirrors ap_bridge/gta3/data.py so the native bridge can run without Python.
namespace ApData {
    struct IdNamePair {
        int         id;
        const char* name;
    };

    struct NameIdPair {
        const char* name;
        int         id;
    };

    inline constexpr IdNamePair kItemIdToName[] = {
        {108269568, "unlock_asuka"},
        {108269569, "unlock_asuka_suburban"},
        {108269570, "unlock_catalina"},
        {108269571, "unlock_diablo"},
        {108269572, "unlock_donald_love"},
        {108269573, "unlock_frankie"},
        {108269574, "unlock_hood"},
        {108269575, "unlock_joey"},
        {108269576, "unlock_kenji"},
        // Legacy-only: old seeds may still send this, but new seeds no longer
        // place Luigi as an Archipelago item because the chain starts unlocked.
        {108269577, "unlock_luigi"},
        {108269578, "unlock_ray"},
        {108269579, "unlock_toni"},
        {108269580, "unlock_yardie"},
        {108269581, "unlock_commercial"},
        {108269582, "unlock_suburban"},
        {108269583, "cash"},
        // Weapon/armor items (useful tier): applied by the plugin via
        // CPed::GiveWeapon / the armor float. Mirrors ap_bridge/gta3/data.py.
        {108269584, "weapon_bat"},
        {108269585, "weapon_pistol"},
        {108269586, "weapon_uzi"},
        {108269587, "weapon_shotgun"},
        {108269588, "weapon_ak47"},
        {108269589, "weapon_m16"},
        {108269590, "weapon_sniper"},
        {108269591, "weapon_rocket"},
        {108269592, "weapon_flamethrower"},
        {108269593, "weapon_molotov"},
        {108269594, "weapon_grenade"},
        {108269595, "armor"},
        // Mission-by-mission AP unlock items. Mirrors ap_bridge/gta3/data.py.
        {108269596, "unlock_mission_asuka1"},
        {108269597, "unlock_mission_asuka2"},
        {108269598, "unlock_mission_asuka3"},
        {108269599, "unlock_mission_asuka4"},
        {108269600, "unlock_mission_asuka5"},
        {108269601, "unlock_mission_love1"},
        {108269602, "unlock_mission_love2"},
        {108269603, "unlock_mission_love3"},
        {108269604, "unlock_mission_kenji1"},
        {108269605, "unlock_mission_kenji2"},
        {108269606, "unlock_mission_kenji3"},
        {108269607, "unlock_mission_kenji4"},
        {108269608, "unlock_mission_kenji5"},
        {108269609, "unlock_mission_ray1"},
        {108269610, "unlock_mission_ray2"},
        {108269611, "unlock_mission_ray3"},
        {108269612, "unlock_mission_ray4"},
        {108269613, "unlock_mission_ray5"},
        {108269614, "unlock_mission_yard1"},
        {108269615, "unlock_mission_yard2"},
        {108269616, "unlock_mission_yard3"},
        {108269617, "unlock_mission_yard4"},
        {108269618, "unlock_mission_diablo1"},
        {108269619, "unlock_mission_diablo2"},
        {108269620, "unlock_mission_diablo3"},
        {108269621, "unlock_mission_diablo4"},
        {108269622, "unlock_mission_frank1"},
        {108269623, "unlock_mission_frank2"},
        {108269624, "unlock_mission_frank2_1"},
        {108269625, "unlock_mission_frank3"},
        {108269626, "unlock_mission_frank4"},
        {108269627, "unlock_mission_joey1"},
        {108269628, "unlock_mission_joey2"},
        {108269629, "unlock_mission_joey3"},
        {108269630, "unlock_mission_joey4"},
        {108269631, "unlock_mission_joey5"},
        {108269632, "unlock_mission_joey6"},
        {108269633, "unlock_mission_luigi2"},
        {108269634, "unlock_mission_luigi3"},
        {108269635, "unlock_mission_luigi4"},
        {108269636, "unlock_mission_luigi5"},
        {108269637, "unlock_mission_toni1"},
        {108269638, "unlock_mission_toni2"},
        {108269639, "unlock_mission_toni3"},
        {108269640, "unlock_mission_toni4"},
        {108269641, "unlock_mission_toni5"},
        {108269642, "unlock_mission_asusb1"},
        {108269643, "unlock_mission_asusb2"},
        {108269644, "unlock_mission_asusb3"},
        {108269645, "unlock_mission_cat1"},
        {108269646, "unlock_mission_love4"},
        {108269647, "unlock_mission_love5"},
        {108269648, "unlock_mission_love6"},
        {108269649, "unlock_mission_love7"},
        {108269650, "unlock_mission_hood1"},
        {108269651, "unlock_mission_hood2"},
        {108269652, "unlock_mission_hood3"},
        {108269653, "unlock_mission_hood4"},
        {108269654, "unlock_mission_hood5"},
        {108269655, "unlock_mission_ray6"},
    };

    inline constexpr NameIdPair kLocationNameToId[] = {
        {"mission_asuka1", 108273664},
        {"mission_asuka2", 108273665},
        {"mission_asuka3", 108273666},
        {"mission_asuka4", 108273667},
        {"mission_asuka5", 108273668},
        {"mission_love1", 108273669},
        {"mission_love2", 108273670},
        {"mission_love3", 108273671},
        {"mission_kenji1", 108273672},
        {"mission_kenji2", 108273673},
        {"mission_kenji3", 108273674},
        {"mission_kenji4", 108273675},
        {"mission_kenji5", 108273676},
        {"mission_ray1", 108273677},
        {"mission_ray2", 108273678},
        {"mission_ray3", 108273679},
        {"mission_ray4", 108273680},
        {"mission_ray5", 108273681},
        {"mission_yard1", 108273682},
        {"mission_yard2", 108273683},
        {"mission_yard3", 108273684},
        {"mission_yard4", 108273685},
        {"mission_diablo1", 108273686},
        {"mission_diablo2", 108273687},
        {"mission_diablo3", 108273688},
        {"mission_diablo4", 108273689},
        {"mission_frank1", 108273690},
        {"mission_frank2", 108273691},
        {"mission_frank2_1", 108273692},
        {"mission_frank3", 108273693},
        {"mission_frank4", 108273694},
        {"mission_joey1", 108273695},
        {"mission_joey2", 108273696},
        {"mission_joey3", 108273697},
        {"mission_joey4", 108273698},
        {"mission_joey5", 108273699},
        {"mission_joey6", 108273700},
        {"mission_8ball", 108273701},
        {"mission_luigi2", 108273702},
        {"mission_luigi3", 108273703},
        {"mission_luigi4", 108273704},
        {"mission_luigi5", 108273705},
        {"mission_toni1", 108273706},
        {"mission_toni2", 108273707},
        {"mission_toni3", 108273708},
        {"mission_toni4", 108273709},
        {"mission_toni5", 108273710},
        {"mission_asusb1", 108273711},
        {"mission_asusb2", 108273712},
        {"mission_asusb3", 108273713},
        {"mission_cat1", 108273714},
        {"mission_love4", 108273715},
        {"mission_love5", 108273716},
        {"mission_love6", 108273717},
        {"mission_love7", 108273718},
        {"mission_hood1", 108273719},
        {"mission_hood2", 108273720},
        {"mission_hood3", 108273721},
        {"mission_hood4", 108273722},
        {"mission_hood5", 108273723},
        {"mission_ray6", 108273724},
        {"hidden_packages_10", 108273725},
        {"hidden_packages_20", 108273726},
        {"hidden_packages_30", 108273727},
        {"hidden_packages_40", 108273728},
        {"hidden_packages_50", 108273729},
        {"hidden_packages_60", 108273730},
        {"hidden_packages_70", 108273731},
        {"hidden_packages_80", 108273732},
        {"hidden_packages_90", 108273733},
        {"hidden_packages_100", 108273734},
        {"mission_give_me_liberty", 108273735},
        {"mission_rc1", 108273736},
        {"mission_rc2", 108273737},
        {"mission_rc3", 108273738},
        {"mission_rc4", 108273739},
        {"mission_4x4_1", 108273740},
        {"mission_4x4_2", 108273741},
        {"mission_4x4_3", 108273742},
        {"mission_mayhem1", 108273743},
        {"mission_meat1", 108273744},
        {"mission_meat2", 108273745},
        {"mission_meat3", 108273746},
        {"mission_meat4", 108273747},
    };

    inline const char* LookupItemName(int id) {
        for (const auto& entry : kItemIdToName) {
            if (entry.id == id) {
                return entry.name;
            }
        }
        return nullptr;
    }

    inline bool LookupLocationId(const std::string& name, int* id) {
        for (const auto& entry : kLocationNameToId) {
            if (name == entry.name) {
                if (id) {
                    *id = entry.id;
                }
                return true;
            }
        }
        return false;
    }

    inline const char* LookupLocationName(int id) {
        for (const auto& entry : kLocationNameToId) {
            if (entry.id == id) {
                return entry.name;
            }
        }
        return nullptr;
    }
}
