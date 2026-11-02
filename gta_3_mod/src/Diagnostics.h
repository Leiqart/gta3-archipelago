#pragma once

namespace Diagnostics {
    // Logs first-chance faults of every thread, rate-limited, to explain silent game exits.
    void Install();

    // Removes the fault logger before the module can be unloaded.
    void Uninstall();

    // Logs the call chain of the thread ending the process; call only from DLL_PROCESS_DETACH.
    void LogProcessExit();
}
