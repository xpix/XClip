#pragma once
#include <windows.h>
#include "history.h"

// Shows the search dialog (modal)
// Returns the index of the selected entry, or -1 if cancelled
int ShowSearchDialog(HWND hwndParent, const History& history);
