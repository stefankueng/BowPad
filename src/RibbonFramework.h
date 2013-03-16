#pragma once

#include <UIRibbon.h>
extern IUIFramework *g_pFramework;  // Reference to the Ribbon framework.
extern IUIApplication *g_pApplication;  // Reference to the Application object.

// Methods to facilitate the initialization and destruction of the Ribbon framework.
bool InitializeFramework(HWND hWnd);
void DestroyFramework();
