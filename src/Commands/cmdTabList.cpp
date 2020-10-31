// This file is part of BowPad.
//
// Copyright (C) 2014-2017, 2020 - Stefan Kueng
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
#include "stdafx.h"
#include "CmdTabList.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "ResString.h"
#include "AppUtils.h"
#include "PathUtils.h"

/*

OVERVIEW

Clicking the tab list button causes BP to display a menu of tabs.

Ideally the tab list would be built on demand to ensure it remains
in sync. But we don't know how to make the ribbon query for a new list
each time the menu is clicked, it prefers to cache it.
That means we have to build the tab list when the ribbon asks for it
and track events which indicate the tab list is out of sync and
then invalidate the tab list then so BP re-queries for a new list.
So ideally we'd just build it on demand and trash it when the user
dismisses/cancels the list without making a selection.

But there appears to be no InvalidateOnCancel type ribbon event that
I am aware of to allow for that.
Windows 8 has supports IUIEventLogger and UI_EVENTTYPE that would seem
to be what we want but this isn't documented as working on Windows 7.

A workaround might have been to invalidate our list once the user
begins working in the editor again, but this isn't totally
reliable and leaves the user looking at an empty menu sometimes
so that approach hasn't been taken.

When a future solution becomes available it is likely
we want to call InvalidateTabList on the appropriate event type.

A better solution as described above may be possible, at least in the
future, but the current solution of tracking events and maintaining the list
seems to work pretty effectively for now. It's at least in fashion
with how other commands work. There is a chance that bugs and/or failing
to trap all the right events might make this solution not 100% bullet proof
but it seems to be working ok and all commands currently operate under these
circumstances anyway.

*/

HRESULT CCmdTabList::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    // Each if is self contained and should not fall through
    if (key == UI_PKEY_Categories)
    {
        return S_FALSE;
    }
    else if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr collection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&collection));
        if (CAppUtils::FailedShowMessage(hr))
            return hr;

        // The list will retain whatever from last time so clear it.
        hr = collection->Clear();
        // We will rebuild this information so clear it.
        m_menuInfo.clear();

        // We need to know have an active document to continue.
        auto docId = GetDocIdOfCurrentTab();
        if (!docId.IsValid())
            return E_FAIL;
        if (!HasActiveDocument())
            return E_FAIL;

        PopulateMenu(collection);

        return hr;
    }
    else if (key == UI_PKEY_SelectedItem)
    {
        auto docId = GetDocIdOfCurrentTab();

        UINT index = UI_COLLECTION_INVALIDINDEX;
        for (const auto& d : m_menuInfo)
        {
            ++index;
            if (d.docId == docId)
                break;
        }

        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)index, ppropvarNewValue);
    }
    else if (key == UI_PKEY_Enabled)
    {
        bool enabled = IsServiceAvailable();

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, enabled, ppropvarNewValue);
    }
    else
    {
        return E_NOTIMPL;
    }

    assert(false); // No fall through expected.
    return E_NOTIMPL;
}

bool CCmdTabList::IsServiceAvailable() const
{
    bool available = false;
    if (HasActiveDocument())
        available = true;
    return available;
}

// Populate the dropdown with the details. Returns false if
// any options couldn't be added but for some reason.
// Not a good enough reason not to show the menu though.
bool CCmdTabList::PopulateMenu(IUICollectionPtr& collection)
{
    IUIImagePtr pImg;
    HRESULT     hr = CAppUtils::CreateImage(MAKEINTRESOURCE(IDB_EMPTY), pImg);
    // If image creation fails we can't do much about it other than report it.
    // Images aren't essential, so try to continue without them if we need to.
    CAppUtils::FailedShowMessage(hr);

    int tabCount = GetTabCount();
    for (int i = 0; i < tabCount; ++i)
    {
        // Don't store tab ids since they won't match after a tab drag.
        auto docId = GetDocIDFromTabIndex(i);
        auto path  = GetDocumentFromID(docId).m_path;
        m_menuInfo.push_back(TabInfo(docId, GetTitleForTabIndex(i), path));
    }

    std::sort(std::begin(m_menuInfo), std::end(m_menuInfo),
              [&](const TabInfo& lhs, const TabInfo& rhs) -> bool {
                  return _wcsicmp(lhs.title.c_str(), rhs.title.c_str()) < 0;
              });

    for (const auto& tabInfo : m_menuInfo)
    {
        auto text  = tabInfo.title;
        auto count = std::count_if(m_menuInfo.begin(), m_menuInfo.end(), [&](const TabInfo& item) -> bool {
            return item.title == tabInfo.title;
        });
        if (count > 1)
        {
            wchar_t pathBuf[30] = {0};
            PathCompactPathEx(pathBuf, CPathUtils::GetParentDirectory(tabInfo.path).c_str(), _countof(pathBuf), 0);
            text = CStringUtils::Format(L"%s (%s)",
                                        tabInfo.title.c_str(),
                                        pathBuf);
        }
        hr = CAppUtils::AddStringItem(collection, text.c_str(), -1, pImg);
        // If we can't add one, assume we can't add any more so quit
        // not to avoid spamming the user with a sequence of errors.
        if (FAILED(hr))
            break;
    }

    // Technically if no tabs are found, we could populate the menu with
    // a list item that says that. But the current implementation doesn't
    // allow for no tabs to be open so we don't have that possibility
    // so haven't bothered to handle it as we'd have to add the resource info
    // for that accordingly. In it happens the user would see an empty list
    // which is what they should expect if there were really no tabs open,
    // so it isn't a big deal to worry about now. Also if there were really
    // no tab we'd likely disable the tab list button too.
    return true;
}

HRESULT CCmdTabList::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            // Happens when a highlighted item is selected from the drop down
            // and clicked.
            UINT uSelected;
            hr = UIPropertyToUInt32(*key, *ppropvarValue, &uSelected);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            size_t selected = static_cast<size_t>(uSelected);

            // The user selected a file to open, we don't want that file
            // to remain selected because the user is supposed to
            // reselect a new one each time,so clear the selection status.
            hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;

            return HandleSelectedMenuItem(selected) ? S_OK : E_FAIL;
        }
    }
    return E_NOTIMPL;
}

bool CCmdTabList::IsValidMenuItem(size_t item) const
{
    return item < m_menuInfo.size();
}

bool CCmdTabList::HandleSelectedMenuItem(size_t selected)
{
    if (!HasActiveDocument())
    {
        APPVERIFY(false); // Shouldn't happen.
        return false;
    }
    if (!IsValidMenuItem(selected))
    {
        APPVERIFY(false); // Shouldn't happen.
        return false;
    }

    const auto& item = m_menuInfo[selected];
    int         tab  = GetTabIndexFromDocID(item.docId);
    if (tab >= 0)
    {
        if (tab != GetActiveTabIndex())
            TabActivateAt(tab);
        return true;
    }
    // In the current implementation, we don't build the tab list each time
    // the menu is clicked, because we don't know how to determine
    // when the menu is opened but then canceled without the user making
    // a selection. There is no InvalidateOnCancel type ribbon event that
    // I am aware of.
    // Not being built on demand means our tab list could get out of sync
    // with the real tab list, though we don't know how yet.
    // It might happen if we failed to trap a close event somehow for example.
    // If we reach here the likely reason is our list is out of sync however
    // it happened so call IvalidTabList here.
    // Note reaching here may or may not mean a bug in this module, it could
    // be a bug elsewhere failing to notify us of events. I have fixed one
    // two problems like this so there may be more.
    // This assert is just to draw attention to the fact that a problem exists,
    // but it might be in this module or elsewhere that is causing this.
    APPVERIFY(false);
    InvalidateTabList();

    return false;
}

void CCmdTabList::InvalidateTabList()
{
    HRESULT hr = InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
    CAppUtils::FailedShowMessage(hr);
    m_menuInfo.clear();
}

void CCmdTabList::ScintillaNotify(SCNotification* /*pScn*/)
{
    // ideally we want to invalidate this list when the user cancels
    // the drop down list without making a selection, but there isn't an
    // appropriate fool proof way of doing it that yet, see OVERVIEW.
    // When a solution appears for that problem we likely want some
    // code here to invalidate the tab list on the appropriate event.
    //if (pScn->nmhdr.code == SCN_FOCUSIN)
    //InvalidateTabList();
}

void CCmdTabList::OnDocumentOpen(DocID /*id*/)
{
    // Tab List will be stale now.
    InvalidateTabList();
}

void CCmdTabList::OnDocumentClose(DocID /*id*/)
{
    // Tab List will be stale now.
    InvalidateTabList();
}

void CCmdTabList::TabNotify(TBHDR* /*ptbhdr*/)
{
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
}

void CCmdTabList::OnDocumentSave(DocID /*id*/, bool /*bSaveAs*/)
{
    // Tab List will be stale now. Tab may have changed name etc.
    InvalidateTabList();
}

bool CCmdTabList::Execute()
{
    ResString ctrlName(hRes, cmdTabList_LabelTitle_RESID);
    return CAppUtils::ShowDropDownList(GetHwnd(), ctrlName);
}
