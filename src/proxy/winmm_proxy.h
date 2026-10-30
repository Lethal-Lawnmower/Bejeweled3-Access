#pragma once

// Loads the system winmm.dll and fills the proxy jump table.
// Called from DllMain before anything else.
bool LoadRealWinmm();
