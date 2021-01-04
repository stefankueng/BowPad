// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2021 - Stefan Kueng
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
#include <comip.h>
#include <comdefsp.h>

#include <UIRibbon.h>
#include <ShlObj.h>
#include <Shobjidl.h>
#include <activscp.h>
#include <UIAutomation.h>
#include <spellcheck.h>

_COM_SMARTPTR_TYPEDEF(IUICollection, __uuidof(IUICollection));
_COM_SMARTPTR_TYPEDEF(IUISimplePropertySet, __uuidof(IUISimplePropertySet));
_COM_SMARTPTR_TYPEDEF(IUIImage, __uuidof(IUIImage));
_COM_SMARTPTR_TYPEDEF(IUIImageFromBitmap, __uuidof(IUIImageFromBitmap));
_COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
_COM_SMARTPTR_TYPEDEF(IShellItemArray, __uuidof(IShellItemArray));
_COM_SMARTPTR_TYPEDEF(IFileOperation, __uuidof(IFileOperation));
_COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));
_COM_SMARTPTR_TYPEDEF(IFileSaveDialog, __uuidof(IFileSaveDialog));
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));
_COM_SMARTPTR_TYPEDEF(IUnknown, __uuidof(IUnknown));
// Bowpad uses this interface but not the smart pointer for it yet.
//_COM_SMARTPTR_TYPEDEF(IShellFolder, __uuidof(IShellFolder));
_COM_SMARTPTR_TYPEDEF(IPropertyStore, __uuidof(IPropertyStore));
_COM_SMARTPTR_TYPEDEF(ITaskbarList3, __uuidof(ITaskbarList3));
_COM_SMARTPTR_TYPEDEF(ITaskbarList4, __uuidof(ITaskbarList4));
_COM_SMARTPTR_TYPEDEF(IDispatch, __uuidof(IDispatch));
_COM_SMARTPTR_TYPEDEF(IActiveScript, __uuidof(IActiveScript));
_COM_SMARTPTR_TYPEDEF(IActiveScriptParse, __uuidof(IActiveScriptParse));

// UIAutomation
_COM_SMARTPTR_TYPEDEF(IUIAutomation, __uuidof(IUIAutomation));
_COM_SMARTPTR_TYPEDEF(IUIAutomationElement, __uuidof(IUIAutomationElement));
_COM_SMARTPTR_TYPEDEF(IUIAutomationCondition, __uuidof(IUIAutomationCondition));
_COM_SMARTPTR_TYPEDEF(IUIAutomationInvokePattern, __uuidof(IUIAutomationInvokePattern));

// Spellchecker
_COM_SMARTPTR_TYPEDEF(ISpellCheckerFactory, __uuidof(ISpellCheckerFactory));
_COM_SMARTPTR_TYPEDEF(ISpellChecker, __uuidof(ISpellChecker));
_COM_SMARTPTR_TYPEDEF(IEnumSpellingError, __uuidof(IEnumSpellingError));
_COM_SMARTPTR_TYPEDEF(ISpellingError, __uuidof(ISpellingError));

// These smart pointers are not yet used, but BowPad uses these COM interfaces
// in at the FileTree at least.
_COM_SMARTPTR_TYPEDEF(IContextMenu, __uuidof(IContextMenu));
//_COM_SMARTPTR_TYPEDEF(IContextMenu2, __uuidof(IContextMenu2));
//_COM_SMARTPTR_TYPEDEF(IContextMenu3, __uuidof(IContextMenu3));
_COM_SMARTPTR_TYPEDEF(IUIRibbon, __uuidof(IUIRibbon));
_COM_SMARTPTR_TYPEDEF(IUIContextualUI, __uuidof(IUIContextualUI));
