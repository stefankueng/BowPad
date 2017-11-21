// This file is part of BowPad.
//
// Copyright (C) 2013-2016 - Stefan Kueng
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See <http://www.gnu.org/licenses/> for a copy of the full license text
//
#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>

// C RunTime Header Files
#include <stdlib.h>
#include <memory.h>
#include <tchar.h>
#include <comip.h>
#include <comdef.h>

#include "COMPtrs.h"

#define DEBUGOUTPUTREGPATH L"Software\\BowPad\\DebugOutputString"
#include "DebugOutput.h"

#include "Language.h"
#include "IniSettings.h"

#include <shellapi.h>
#include <Commctrl.h>
#include <Shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// SDKs prior to Win10 don't have the IsWindows10OrGreater API in the versionhelpers header, so
// we define it here just in case:
#include <VersionHelpers.h>
#ifndef _WIN32_WINNT_WIN10
#define _WIN32_WINNT_WIN10 0x0A00
#define _WIN32_WINNT_WINTHRESHOLD 0x0A00
#define  IsWindows10OrGreater() (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0))
#endif

#define APP_ID L"TortoiseSVN.Tools.BowPad.1"
#define APP_ID_ELEVATED L"TortoiseSVN.Tools.BowPad_elevated.1"

#ifdef _WIN64
#define LANGPLAT L"x64"
#else
#define LANGPLAT L"x86"
#endif

// custom id for the WM_COPYDATA message
#define CD_COMMAND_LINE 101
#define CD_COMMAND_MOVETAB 102

#define WM_UPDATEAVAILABLE  (WM_APP + 10)
#define WM_AFTERINIT        (WM_APP + 11)
#define WM_STATUSBAR_MSG    (WM_APP + 12)
#define WM_THREADRESULTREADY    (WM_APP + 13)
#define WM_CANHIDECURSOR    (WM_APP + 14)
