// This file is part of BowPad.
//
// Copyright (C) 2014-2017, 2020 Stefan Kueng
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
#include "CmdPlugins.h"
#include "PropertySet.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "AppUtils.h"
#include "ResString.h"
#include "CommandHandler.h"
#include "CmdScripts.h"


HRESULT CCmdPlugins::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;

    if (key == UI_PKEY_Categories)
        return S_FALSE;

    if (key == UI_PKEY_SelectedItem)
    {
        return UIInitPropertyFromUInt32(UI_PKEY_SelectedItem, (UINT)UI_COLLECTION_INVALIDINDEX, ppropvarNewValue);
    }
    if (key == UI_PKEY_Enabled)
    {
        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, true, ppropvarNewValue);
    }

    if (key == UI_PKEY_ItemsSource)
    {
        IUICollectionPtr pCollection;
        hr = ppropvarCurrentValue->punkVal->QueryInterface(IID_PPV_ARGS(&pCollection));
        if (FAILED(hr))
            return hr;

        pCollection->Clear();

        // Create an IUIImage from a resource id.
        IUIImageFromBitmapPtr pifbFactory;
        hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
        if (FAILED(hr))
            return hr;

        // Load the bitmap from the resource file.
        IUIImagePtr pImg;
        HBITMAP hbm = (HBITMAP)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_EMPTY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
            }
        }
        IUIImagePtr pImgChecked;
        hbm = (HBITMAP)LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_EMPTYCHECKED), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImgChecked);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
            }
        }

        if (FAILED(hr))
            return hr;

        const auto& plugins = CCommandHandler::Instance().GetPluginMap();
        if (plugins.empty())
        {
            // show text that no plugins are available
            ResString sNoPlugins(hRes, IDS_NO_PLUGINS);
            CAppUtils::AddStringItem(pCollection, sNoPlugins);
        }

        // populate the dropdown with the plugins
        for (const auto& p : plugins)
        {
            auto plugin = dynamic_cast<CCmdScript*>(CCommandHandler::Instance().GetCommand(p.first));

            if (plugin)
            {
                CAppUtils::AddStringItem(pCollection, p.second.c_str(), -1, plugin->IsChecked() ? pImgChecked : pImg);
            }
        }
        hr = S_OK;
    }
    return hr;
}

HRESULT CCmdPlugins::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* ppropvarValue, IUISimplePropertySet* /*pCommandExecutionProperties*/)
{
    HRESULT hr = E_FAIL;

    if (verb == UI_EXECUTIONVERB_EXECUTE)
    {
        if (key && *key == UI_PKEY_SelectedItem)
        {
            UINT selected;
            hr                  = UIPropertyToUInt32(*key, *ppropvarValue, &selected);
            UINT        count   = 0;
            const auto& plugins = CCommandHandler::Instance().GetPluginMap();
            for (const auto& p : plugins)
            {
                if (count == selected)
                {
                    SendMessage(GetHwnd(), WM_COMMAND, MAKEWPARAM(p.first, 1), 0);
                    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
                    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_ItemsSource);
                    break;
                }
                ++count;
            }
            hr = S_OK;
        }
    }
    return hr;
}
