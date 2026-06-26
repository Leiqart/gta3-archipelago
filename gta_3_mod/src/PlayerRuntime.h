#pragma once

#include "BuildConfig.h"   // AP_ENABLE_DEBUG_KEYS (cheats: Debug=in, Release=out)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "ApBridge.h"
#include "ApBridgeProcess.h"
#include "Config.h"
#include "DebugKeys.h"
#include "GameAddresses.h"
#include "Hooks.h"
#include "GameStructs.h"
#include "Logger.h"
#include "PluginPaths.h"
#include "RunState.h"
#include "PackagePoints.h"
#include "ScriptRuntime.h"
#include "TextOverrides.h"

namespace MenuPatch {
    bool QueueIntroGmlFallbackMissionLaunch();
    bool QueueGiveMeLibertyRetryMissionLaunch();
}

// ==========================================================================
// PlayerRuntime was a 4000-line monolith. Its body is now carved into the
// PlayerRuntime/*.h sub-headers below, all in 'namespace PlayerRuntime' and
// #included here IN ORIGINAL ORDER, so every cross-reference / forward decl
// resolves exactly as before (pure text reorg, zero behaviour change).
// ==========================================================================
#include "PlayerRuntime/Core.h"
#include "PlayerRuntime/ScmUtils.h"
#include "PlayerRuntime/IntroSequence.h"
#include "PlayerRuntime/MissionLifecycle.h"
#include "PlayerRuntime/RunStartMoney.h"
#include "PlayerRuntime/StubsValidation.h"
#include "PlayerRuntime/BridgeOutfit.h"
#include "PlayerRuntime/BlipsVehiclePrep.h"
#include "PlayerRuntime/SpawnDeathPersistence.h"
#include "PlayerRuntime/FrameWatches.h"


// Debug hotkey handlers (teleports, spawn, traps, kill, recorder, unlocks,
// death) live in their own file but the same namespace; included last so every
// shared helper/stub/data they use is already defined above.
#include "debug/DebugHotkeys.h"
#include "debug/DebugManager.h"
