#include <string>
#include "plugin.h"
#include "CCutsceneMgr.h"
#include "CMenuManager.h"
#include "CPlayerInfo.h"
#include "CPlayerPed.h"
#include "CTimer.h"
#include "CWorld.h"
#include "common.h"
#include "ClientLink.h"
#include "Deaths.h"
#include "Log.h"
#include "Message.h"

namespace {
    // A death the player dies within this many milliseconds of a kill from the link is that kill's, and is not sent back.
    constexpr unsigned int kKillAnswerTime = 10000;

    // The player's state on the last frame, to see the moment he is wasted or busted.
    ePlayerState g_lastState = PLAYERSTATE_PLAYING;
    // Who died in another game, until the player can be killed; empty when no kill waits.
    std::string g_pendingKill;
    // Whether this mod has killed the player for another player's death, and when, until that death is seen.
    bool g_killed = false;
    unsigned int g_killedAt = 0;

    CPlayerInfo& Player() { return CWorld::Players[CWorld::PlayerInFocus]; }

    // True while the player plays: a ped with its model, alive and free, no cutscene or menu showing.
    bool Playing() {
        const CPlayerPed* ped = FindPlayerPed();
        return ped && ped->m_pRwObject && Player().m_nPlayerState == PLAYERSTATE_PLAYING && !CCutsceneMgr::ms_running && !FrontEndMenuManager.m_bMenuActive;
    }

    // Tells the client the player was just wasted or busted, unless a kill from the link caused it.
    void ReportDeath(ePlayerState state) {
        const std::string how = state == PLAYERSTATE_HASDIED ? "wasted" : "busted";
        const bool answer = g_killed && CTimer::m_snTimeInMilliseconds - g_killedAt < kKillAnswerTime;
        g_killed = false;
        if (answer) {
            Log::Write("DeathLink: the player is " + how + " by another player's death, not sent back");
            return;
        }
        const bool sent = ClientLink::Send("death " + how);
        Log::Write("DeathLink: the player is " + how + (sent ? ", sent to the client" : ", not sent: the client is not running"));
    }

    // Kills the player with damage: in a car only drowning reaches him (a ped in a vehicle has no collision, and that is how a sinking car kills), on foot an explosion; false while the game keeps him from harm.
    bool Kill() {
        CPlayerPed* ped = FindPlayerPed();
        const eWeaponType damage = ped->m_bInVehicle ? WEAPONTYPE_DROWNING : WEAPONTYPE_EXPLOSION;
        if (!ped->InflictDamage(nullptr, damage, 1000.0f, PEDPIECE_TORSO, 0)) return false;
        Message::Show(g_pendingKill + " died, so do you");
        Log::Write("DeathLink: " + g_pendingKill + " died, the player is killed");
        g_killed = true;
        g_killedAt = CTimer::m_snTimeInMilliseconds;
        return true;
    }

    // Runs once per frame: reports a new death, and tries a waiting kill while the player plays, until the game lets it through.
    void Process() {
        const ePlayerState state = Player().m_nPlayerState;
        if (state != g_lastState && (state == PLAYERSTATE_HASDIED || state == PLAYERSTATE_HASBEENARRESTED)) ReportDeath(state);
        g_lastState = state;
        if (!g_pendingKill.empty() && Playing() && Kill()) g_pendingKill.clear();
    }

    // One "kill <name>" line: another player died; a player already dead or busted, or not in a game, is spared.
    void HandleKill(const std::string& line) {
        const std::string name = line.size() > 5 ? line.substr(5) : "Someone";
        if (!FindPlayerPed() || Player().m_nPlayerState != PLAYERSTATE_PLAYING) {
            Log::Write("DeathLink: " + name + " died, the player is not playing: spared");
            return;
        }
        g_pendingKill = name;
        Log::Write("DeathLink: " + name + " died, the player dies as soon as the game lets it");
    }
}

void Deaths::Install() {
    ClientLink::On("kill", HandleKill);
    plugin::Events::gameProcessEvent += Process;
}
