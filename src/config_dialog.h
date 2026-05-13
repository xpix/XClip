#pragma once
#include <windows.h>
#include "settings.h"

// Shows the configuration dialog (modal)
// Returns true if settings were changed
bool ShowConfigDialog(HWND hwndParent, Settings& settings);
