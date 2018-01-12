// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2018 - Stefan Kueng
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

#include "scripting/BasicScriptObject.h"
#include "scripting/BasicScriptHost.h"

CCmdScript::CCmdScript(void * obj)
    : ICommand(obj)
    , m_appObject(new BasicScriptObject(obj))
    , m_host(nullptr)
    , m_cmdID(0)
    , m_version(0)
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
        GUID engineID;
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
        std::wstring source;  // stores file contents
        FILE * f = nullptr;
        _wfopen_s(&f, path.c_str(), L"rtS, ccs=UTF-8");

        // Failed to open file
        if (f != nullptr)
        {
            OnOutOfScope(
                fclose(f);
                f = nullptr;
            );
            struct _stat fileinfo;
            _wstat(path.c_str(), &fileinfo);

            // Read entire file contents in to memory
            if (fileinfo.st_size > 0)
            {
                source.resize(fileinfo.st_size);
                size_t wchars_read = fread(&(source.front()), sizeof(source[0]), fileinfo.st_size, f);
                source.resize(wchars_read);
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
        DISPPARAMS dispparams = { 0 };
        try
        {
            auto ret = m_host->CallFunction(L"Version", dispparams);
            if (SUCCEEDED(VariantChangeType(&ret, &ret, VARIANT_ALPHABOOL, VT_INT)))
                m_version = ret.intVal;
        }
        catch (const std::exception&) {}

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

bool CCmdScript::Execute()
{
    DISPPARAMS dispparams = { 0 };
    try { m_host->CallFunction(L"Execute", dispparams); }
    catch (const std::exception&) {}
    return true;
}

UINT CCmdScript::GetCmdId()
{
    return m_cmdID;
}

void CCmdScript::ScintillaNotify(SCNotification * pScn)
{
    _bstr_t btext;
    if (pScn->text && pScn->length)
    {
        std::string text(pScn->text, pScn->length);
        btext = text.c_str();
    }
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 21;
    VARIANT v[21];
    v[20].uintVal = pScn->nmhdr.code;
    v[20].vt = VT_UINT;
    v[19].intVal = (int)pScn->position;
    v[19].vt = VT_INT;
    v[18].intVal = pScn->ch;
    v[18].vt = VT_INT;
    v[17].intVal = pScn->modifiers;
    v[17].vt = VT_INT;
    v[16].intVal = pScn->modificationType;
    v[16].vt = VT_INT;
    v[15].bstrVal = btext;
    v[15].vt = VT_BSTR;
    v[14].intVal = (int)pScn->length;
    v[14].vt = VT_INT;
    v[13].intVal = (int)pScn->linesAdded;
    v[13].vt = VT_INT;
    v[12].intVal = pScn->message;
    v[12].vt = VT_INT;
    v[11].ullVal = pScn->wParam;
    v[11].vt = VT_UI8;
    v[10].ullVal = pScn->lParam;
    v[10].vt = VT_UI8;
    v[ 9].intVal = (int)pScn->line;
    v[ 9].vt = VT_INT;
    v[ 8].intVal = pScn->foldLevelNow;
    v[ 8].vt = VT_INT;
    v[ 7].intVal = pScn->foldLevelPrev;
    v[ 7].vt = VT_INT;
    v[ 6].intVal = pScn->margin;
    v[ 6].vt = VT_INT;
    v[ 5].intVal = pScn->listType;
    v[ 5].vt = VT_INT;
    v[ 4].intVal = pScn->x;
    v[ 4].vt = VT_INT;
    v[ 3].intVal = pScn->y;
    v[ 3].vt = VT_INT;
    v[ 2].intVal = pScn->token;
    v[ 2].vt = VT_INT;
    v[ 1].intVal = (int)pScn->annotationLinesAdded;
    v[ 1].vt = VT_INT;
    v[ 0].intVal = pScn->updated;
    v[ 0].vt = VT_INT;

    dispparams.rgvarg = v;
    try { m_host->CallFunction(L"ScintillaNotify", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::TabNotify(TBHDR * ptbhdr)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 2;
    VARIANT v[2];
    v[0].uintVal = ptbhdr->hdr.code;
    v[0].vt = VT_UINT;
    v[1].intVal = ptbhdr->tabOrigin;
    v[1].vt = VT_INT;
    dispparams.rgvarg = v;
    try { m_host->CallFunction(L"TabNotify", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnClose()
{
    DISPPARAMS dispparams = { 0 };
    try { m_host->CallFunction(L"OnClose", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::AfterInit()
{
    DISPPARAMS dispparams = { 0 };
    try { m_host->CallFunction(L"AfterInit", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnDocumentClose(DocID id)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 1;
    VARIANT vI;
    vI.intVal = id.GetValue();
    vI.vt = VT_INT;
    dispparams.rgvarg = &vI;
    try { m_host->CallFunction(L"OnDocumentClose", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnDocumentOpen(DocID id)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 1;
    VARIANT vI;
    vI.intVal = id.GetValue();
    vI.vt = VT_INT;
    dispparams.rgvarg = &vI;
    try { m_host->CallFunction(L"OnDocumentOpen", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnDocumentSave(DocID id, bool bSaveAs)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 2;
    VARIANT v[2];
    v[0].intVal = id.GetValue();
    v[0].vt = VT_INT;
    v[1].boolVal = bSaveAs ? VARIANT_TRUE : VARIANT_FALSE;
    v[1].vt = VT_BOOL;
    dispparams.rgvarg = v;
    try { m_host->CallFunction(L"OnDocumentSave", dispparams); }
    catch (const std::exception&) {}
}

HRESULT CCmdScript::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*ppropvarCurrentValue*/, PROPVARIANT* ppropvarNewValue)
{
    if (UI_PKEY_Enabled == key)
    {
        DISPPARAMS dispparams = { 0 };
        try
        {
            _variant_t ret = m_host->CallFunction(L"IsEnabled", dispparams);
            if (ret.vt == VT_BOOL)
                return UIInitPropertyFromBoolean(UI_PKEY_Enabled, ret.boolVal, ppropvarNewValue);
        }
        catch (const std::exception&) {}

        return UIInitPropertyFromBoolean(UI_PKEY_Enabled, true, ppropvarNewValue);
    }
    if (UI_PKEY_BooleanValue == key)
    {
        DISPPARAMS dispparams = { 0 };
        try
        {
            _variant_t ret = m_host->CallFunction(L"IsChecked", dispparams);
            if (ret.vt == VT_BOOL)
                return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, ret.boolVal, ppropvarNewValue);
        }
        catch (const std::exception&) {}
        return UIInitPropertyFromBoolean(UI_PKEY_BooleanValue, false, ppropvarNewValue);
    }
    return E_NOTIMPL;
}

void CCmdScript::OnTimer(UINT id)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 1;
    VARIANT vI;
    vI.uintVal = id;
    vI.vt = VT_UINT;
    dispparams.rgvarg = &vI;
    try { m_host->CallFunction(L"OnTimer", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnThemeChanged(bool bDark)
{
    DISPPARAMS dispparams = { 0 };
    dispparams.cArgs = 1;
    VARIANT vI;
    vI.boolVal = bDark ? VARIANT_TRUE : VARIANT_FALSE;
    vI.vt = VT_BOOL;
    dispparams.rgvarg = &vI;
    try { m_host->CallFunction(L"OnThemeChanged", dispparams); }
    catch (const std::exception&) {}
}

void CCmdScript::OnLangChanged()
{
    DISPPARAMS dispparams = { 0 };
    try { m_host->CallFunction(L"OnLexerChanged", dispparams); }
    catch (const std::exception&) {}
}


