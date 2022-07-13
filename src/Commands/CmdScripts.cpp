// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2018, 2020-2022 - Stefan Kueng
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
#include "CmdScripts.h"
#include "PathUtils.h"
#include "AppUtils.h"
#include "UnicodeUtils.h"
#include "OnOutOfScope.h"
#include "BowPadUI.h"

#include "scripting/BasicScriptObject.h"
#include "scripting/BasicScriptHost.h"

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)

CCmdScript::CCmdScript(void* obj, const std::wstring& path)
    : ICommand(obj)
    , m_version(0)
    , m_appObject(new BasicScriptObject(obj, path))
    , m_host(nullptr)
    , m_cmdID(0)
    , m_image(nullptr)
{
}

CCmdScript::~CCmdScript()
{
    if (m_host)
    {
        m_host->Terminate();
        delete m_host;
    }
    delete m_appObject;
}

bool CCmdScript::Create(const std::wstring& path)
{
    try
    {
        //  Initialize the application object
        GUID         engineID;
        std::wstring sEngine = L"JavaScript";
        if (_wcsicmp(CPathUtils::GetFileExtension(path).c_str(), L"bpj") == 0)
            sEngine = L"JavaScript";
        else if (_wcsicmp(CPathUtils::GetFileExtension(path).c_str(), L"bpv") == 0)
            sEngine = L"VBScript";
        else
            return false;

        HRESULT hr = CLSIDFromProgID(sEngine.c_str(), &engineID);
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        IDispatchPtr appObjectDispatch;
        hr = m_appObject->QueryInterface(IID_PPV_ARGS(&appObjectDispatch));
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        //  Create the script site
        m_host = new BasicScriptHost(engineID, L"BowPad", appObjectDispatch);

        hr = m_host->Initialize();
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        m_host->m_path = path;
        m_host->m_hWnd = GetHwnd();
        std::wstring source; // stores file contents
        FILE*        f = nullptr;
        _wfopen_s(&f, path.c_str(), L"rtS, ccs=UTF-8");

        // Failed to open file
        if (f != nullptr)
        {
            OnOutOfScope(
                fclose(f);
                f = nullptr;);
            struct _stat fileInfo;
            _wstat(path.c_str(), &fileInfo);

            // Read entire file contents in to memory
            if (fileInfo.st_size > 0)
            {
                source.resize(fileInfo.st_size);
                size_t wCharsRead = fread(&(source.front()), sizeof(source[0]), fileInfo.st_size, f);
                source.resize(wCharsRead);
                source.shrink_to_fit();
            }
        }

        hr = m_host->Parse(source);
        if (CAppUtils::FailedShowMessage(hr))
            return false;

        hr = m_host->Run();
        if (CAppUtils::FailedShowMessage(hr))
            return false;
        // get the version
        DISPPARAMS dispParams = {nullptr};
        try
        {
            auto ret = m_host->CallFunction(L"Version", dispParams);
            if (SUCCEEDED(VariantChangeType(&ret, &ret, VARIANT_ALPHABOOL, VT_INT)))
                m_version = ret.intVal;
        }
        catch (const std::exception&)
        {
        }

        m_name        = CPathUtils::GetFileNameWithoutExtension(path);
        auto iconPath = CPathUtils::GetParentDirectory(path) + L"\\" + m_name + L".png";
        if (PathFileExists(iconPath.c_str()))
        {
            auto bmp  = Gdiplus::Bitmap(iconPath.c_str(), TRUE);
            auto pBmp = &bmp;
            if (bmp.GetLastStatus() == Gdiplus::Ok)
            {
                Gdiplus::Bitmap resized(16, 16);
                if (!IsWindows8OrGreater())
                {
                    auto g = Gdiplus::Graphics::FromImage(&resized);
                    g->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                    g->Clear(Gdiplus::Color::Transparent);
                    g->DrawImage(&bmp, Gdiplus::Rect(0, 0, 16, 16));
                    pBmp = &resized;
                }
                IUIImageFromBitmapPtr pifbFactory;
                hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
                if (SUCCEEDED(hr))
                {
                    HBITMAP hbm = nullptr;
                    pBmp->GetHBITMAP(Gdiplus::Color(0xFFFFFFFF), &hbm);
                    hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &m_image);
                    if (FAILED(hr))
                        ::DeleteObject(hbm);
                }
            }
        }
        else
        {
            // use the plugin default icon
            if (IsWindows8OrGreater())
                hr = CAppUtils::CreateImage(MAKEINTRESOURCE(cmdPlugins_LargeImages_RESID), m_image);
            else
                hr = CAppUtils::CreateImage(MAKEINTRESOURCE(cmdPlugins_LargeImages_RESID), m_image, 16, 16);
        }
    }
    catch (const std::exception& e)
    {
        if (CIniSettings::Instance().GetInt64(L"Debug", L"usemessagebox", 0))
        {
            MessageBox(GetHwnd(), L"BowPad", CUnicodeUtils::StdGetUnicode(e.what()).c_str(), MB_ICONERROR);
        }
        else
        {
            CTraceToOutputDebugString::Instance()(L"BowPad : ");
            CTraceToOutputDebugString::Instance()(e.what());
            CTraceToOutputDebugString::Instance()(L"\n");
        }
        return false;
    }
    return true;
}

void CCmdScript::SetCmdId(UINT cmdId)
{
    m_cmdID = cmdId;
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SelectedItem);
    InvalidateUICommand(UI_INVALIDATIONS_VALUE, &UI_PKEY_SelectedItem);
    InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_SmallImage);
}

bool CCmdScript::Execute()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"Execute", dispParams, false);
    return true;
}

bool CCmdScript::IsEnabled() const
{
    DISPPARAMS dispParams = {nullptr};
    _variant_t ret        = {0};
    ret                   = m_host->CallFunction(L"IsEnabled", dispParams, false);

    if (ret.vt == VT_BOOL)
        return ret.boolVal != VARIANT_FALSE;
    return false;
}

bool CCmdScript::IsChecked() const
{
    DISPPARAMS dispParams = {nullptr};
    _variant_t ret        = {0};
    ret                   = m_host->CallFunction(L"IsChecked", dispParams, false);
    if (ret.vt == VT_BOOL)
        return ret.boolVal != VARIANT_FALSE;
    return false;
}

IUIImagePtr CCmdScript::getIcon() const
{
    return m_image;
}

UINT CCmdScript::GetCmdId()
{
    return m_cmdID;
}

void CCmdScript::ScintillaNotify(SCNotification* pScn)
{
    _bstr_t bText;
    if (pScn->text && pScn->length)
    {
        std::string text(pScn->text, pScn->length);
        bText = text.c_str();
    }
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 21;
    VARIANT v[21];
    v[20].uintVal = pScn->nmhdr.code;
    v[20].vt      = VT_UINT;
    v[19].intVal  = static_cast<int>(pScn->position);
    v[19].vt      = VT_INT;
    v[18].intVal  = pScn->ch;
    v[18].vt      = VT_INT;
    v[17].intVal  = pScn->modifiers;
    v[17].vt      = VT_INT;
    v[16].intVal  = pScn->modificationType;
    v[16].vt      = VT_INT;
    v[15].bstrVal = bText;
    v[15].vt      = VT_BSTR;
    v[14].intVal  = static_cast<int>(pScn->length);
    v[14].vt      = VT_INT;
    v[13].intVal  = static_cast<int>(pScn->linesAdded);
    v[13].vt      = VT_INT;
    v[12].intVal  = pScn->message;
    v[12].vt      = VT_INT;
    v[11].ullVal  = pScn->wParam;
    v[11].vt      = VT_UI8;
    v[10].ullVal  = pScn->lParam;
    v[10].vt      = VT_UI8;
    v[9].intVal   = static_cast<int>(pScn->line);
    v[9].vt       = VT_INT;
    v[8].intVal   = pScn->foldLevelNow;
    v[8].vt       = VT_INT;
    v[7].intVal   = pScn->foldLevelPrev;
    v[7].vt       = VT_INT;
    v[6].intVal   = pScn->margin;
    v[6].vt       = VT_INT;
    v[5].intVal   = pScn->listType;
    v[5].vt       = VT_INT;
    v[4].intVal   = pScn->x;
    v[4].vt       = VT_INT;
    v[3].intVal   = pScn->y;
    v[3].vt       = VT_INT;
    v[2].intVal   = pScn->token;
    v[2].vt       = VT_INT;
    v[1].intVal   = static_cast<int>(pScn->annotationLinesAdded);
    v[1].vt       = VT_INT;
    v[0].intVal   = pScn->updated;
    v[0].vt       = VT_INT;

    dispParams.rgvarg = v;
    m_host->CallFunction(L"ScintillaNotify", dispParams, false);
}

void CCmdScript::TabNotify(TBHDR* ptbHdr)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 2;
    VARIANT v[2];
    v[1].intVal       = static_cast<int>(ptbHdr->hdr.code);
    v[1].vt           = VT_INT;
    v[0].intVal       = ptbHdr->tabOrigin;
    v[0].vt           = VT_INT;
    dispParams.rgvarg = v;
    m_host->CallFunction(L"TabNotify", dispParams, false);
}

void CCmdScript::OnClose()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"OnClose", dispParams, false);
}

void CCmdScript::BeforeLoad()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"BeforeLoad", dispParams, false);
}

void CCmdScript::AfterInit()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"AfterInit", dispParams, false);
}

void CCmdScript::OnDocumentClose(DocID id)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 1;
    VARIANT vI;
    vI.intVal         = id.GetValue();
    vI.vt             = VT_INT;
    dispParams.rgvarg = &vI;
    m_host->CallFunction(L"OnDocumentClose", dispParams, false);
}

void CCmdScript::OnDocumentOpen(DocID id)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 1;
    VARIANT vI;
    vI.intVal         = id.GetValue();
    vI.vt             = VT_INT;
    dispParams.rgvarg = &vI;
    m_host->CallFunction(L"OnDocumentOpen", dispParams, false);
}

void CCmdScript::OnBeforeDocumentSave(DocID id)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 1;
    VARIANT v[1];
    v[0].intVal       = id.GetValue();
    v[0].vt           = VT_INT;
    dispParams.rgvarg = v;
    m_host->CallFunction(L"OnBeforeDocumentSave", dispParams, false);
}

void CCmdScript::OnDocumentSave(DocID id, bool bSaveAs)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 2;
    VARIANT v[2];
    v[1].intVal       = id.GetValue();
    v[1].vt           = VT_INT;
    v[0].boolVal      = bSaveAs ? VARIANT_TRUE : VARIANT_FALSE;
    v[0].vt           = VT_BOOL;
    dispParams.rgvarg = v;
    m_host->CallFunction(L"OnDocumentSave", dispParams, false);
}

HRESULT CCmdScript::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        DISPPARAMS dispParams = {nullptr};
        _variant_t ret        = m_host->CallFunction(L"IsEnabled", dispParams, false);
        if (ret.vt == VT_BOOL)
            return UIInitPropertyFromBoolean(UI_PKEY_Enabled, ret.boolVal == VARIANT_TRUE, pPropVarNewValue);

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, true, pPropVarNewValue);
    }
    else if (UI_PKEY_BooleanValue == key)
    {
        DISPPARAMS dispParams = {nullptr};
        _variant_t ret        = m_host->CallFunction(L"IsChecked", dispParams, false);
        if (ret.vt == VT_BOOL)
            return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ret.boolVal == VARIANT_TRUE, pPropVarNewValue);

        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, false, pPropVarNewValue);
    }
    else if (UI_PKEY_Label == key)
    {
        return UIInitPropertyFromString(UI_PKEY_Label, m_name.c_str(), pPropVarNewValue);
    }
    else if (UI_PKEY_LabelDescription == key)
    {
        return UIInitPropertyFromString(UI_PKEY_LabelDescription, m_name.c_str(), pPropVarNewValue);
    }
    else if (UI_PKEY_TooltipDescription == key)
    {
        return UIInitPropertyFromString(UI_PKEY_TooltipDescription, m_description.c_str(), pPropVarNewValue);
    }
    else if (UI_PKEY_LargeImage == key)
    {
        return UIInitPropertyFromImage(UI_PKEY_LargeImage, m_image, pPropVarNewValue);
    }
    else if (UI_PKEY_SmallImage == key)
    {
        return UIInitPropertyFromImage(UI_PKEY_SmallImage, m_image, pPropVarNewValue);
    }
    else if (UI_PKEY_LargeHighContrastImage == key)
    {
        return UIInitPropertyFromImage(UI_PKEY_LargeHighContrastImage, m_image, pPropVarNewValue);
    }
    else if (UI_PKEY_SmallHighContrastImage == key)
    {
        return UIInitPropertyFromImage(UI_PKEY_SmallHighContrastImage, m_image, pPropVarNewValue);
    }

    return E_NOTIMPL;
}

void CCmdScript::OnTimer(UINT id)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 1;
    VARIANT vI;
    vI.uintVal        = id;
    vI.vt             = VT_UINT;
    dispParams.rgvarg = &vI;
    m_host->CallFunction(L"OnTimer", dispParams, false);
}

void CCmdScript::OnThemeChanged(bool bDark)
{
    DISPPARAMS dispParams = {nullptr};
    dispParams.cArgs      = 1;
    VARIANT vI;
    vI.boolVal        = bDark ? VARIANT_TRUE : VARIANT_FALSE;
    vI.vt             = VT_BOOL;
    dispParams.rgvarg = &vI;
    m_host->CallFunction(L"OnThemeChanged", dispParams, false);
}

void CCmdScript::OnLangChanged()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"OnLexerChanged", dispParams, false);
}

void CCmdScript::OnStylesSet()
{
    DISPPARAMS dispParams = {nullptr};
    m_host->CallFunction(L"OnStylesSet", dispParams, false);
}
