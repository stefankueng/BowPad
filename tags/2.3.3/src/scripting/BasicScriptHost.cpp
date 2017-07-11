// This file is part of BowPad.
//
// Copyright (C) 2014, 2016 - Stefan Kueng
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
#include "BasicScriptHost.h"
#include "BasicScriptObject.h"
#include "StringUtils.h"
#include <stdexcept>

BasicScriptHost::BasicScriptHost(const GUID& languageId, const std::wstring& objectName, IDispatchPtr object)
    : m_refCount(1)
    , m_hWnd(nullptr)
{
    IActiveScriptPtr engine;

    HRESULT hr = CoCreateInstance(languageId, 0, CLSCTX_INPROC_SERVER, IID_IActiveScript, reinterpret_cast<void**>(&engine));
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to create active script object");
    }

    hr = engine->SetScriptSite(this);
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to set scripting site");
    }

    hr = engine->AddNamedItem(objectName.c_str(), SCRIPTITEM_ISVISIBLE | SCRIPTITEM_NOCODE);
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to set application object");
    }

    //  Done, set the application object and engine
    m_application = object;
    m_scriptEngine = engine;
}


BasicScriptHost::BasicScriptHost(const GUID& languageId)
    : m_refCount(1)
    , m_hWnd(nullptr)
{
    IActiveScriptPtr engine;

    HRESULT hr = CoCreateInstance(languageId, 0, CLSCTX_INPROC_SERVER, IID_IActiveScript, reinterpret_cast<void**>(&engine));
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to create active script object");
    }

    hr = engine->SetScriptSite(this);
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to set scripting site");
    }

    //  Done, set the engine
    m_scriptEngine = engine.Detach();
}


BasicScriptHost::~BasicScriptHost()
{}


HRESULT BasicScriptHost::QueryInterface(REFIID riid, void ** object)
{
    if (riid == IID_IActiveScriptSite)
    {
        *object = this;
    }
    if (riid == IID_IDispatch)
    {
        *object = reinterpret_cast<IDispatch*>(this);
    }
    else
    {
        *object = nullptr;
    }

    if (*object != nullptr)
    {
        AddRef();

        return S_OK;
    }

    return E_NOINTERFACE;
}


ULONG BasicScriptHost::AddRef()
{
    return ::InterlockedIncrement(&m_refCount);
}


ULONG BasicScriptHost::Release()
{
    ULONG oldCount = m_refCount;

    ULONG newCount = ::InterlockedDecrement(&m_refCount);
    if (0 == newCount)
    {
        delete this;
    }

    return oldCount;
}


HRESULT BasicScriptHost::GetLCID(DWORD *lcid)
{
    *lcid = LOCALE_USER_DEFAULT;
    return S_OK;
}


HRESULT BasicScriptHost::GetDocVersionString(BSTR* ver)
{
    *ver = nullptr;
    return S_OK;
}


HRESULT BasicScriptHost::OnScriptTerminate(const VARIANT *, const EXCEPINFO *)
{
    return S_OK;
}


HRESULT BasicScriptHost::OnStateChange(SCRIPTSTATE /*state*/)
{
    return S_OK;
}


HRESULT BasicScriptHost::OnEnterScript()
{
    return S_OK;
}


HRESULT BasicScriptHost::OnLeaveScript()
{
    return S_OK;
}


HRESULT BasicScriptHost::GetItemInfo(const WCHAR*    /*name*/,
                                     DWORD           req,
                                     IUnknown**      obj,
                                     ITypeInfo**     type)
{
    if (req & SCRIPTINFO_IUNKNOWN && obj != nullptr)
    {
        *obj = m_application;
        if (*obj != nullptr)
        {
            (*obj)->AddRef();
        }
    }

    if (req & SCRIPTINFO_ITYPEINFO && type != nullptr)
    {
        *type = nullptr;
    }

    return S_OK;
}


HRESULT BasicScriptHost::Initialize()
{
    IActiveScriptParsePtr parse;

    HRESULT hr = m_scriptEngine->QueryInterface(IID_IActiveScriptParse, reinterpret_cast<void **>(&parse));
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to get pointer to script parsing interface");
    }

    // Sets state to SCRIPTSTATE_INITIALIZED
    hr = parse->InitNew();

    return hr;
}


HRESULT BasicScriptHost::Run()
{
    // Sets state to SCRIPTSTATE_CONNECTED
    HRESULT hr = m_scriptEngine->SetScriptState(SCRIPTSTATE_CONNECTED);

    return hr;
}


HRESULT BasicScriptHost::Terminate()
{
    HRESULT hr = m_scriptEngine->SetScriptState(SCRIPTSTATE_DISCONNECTED);
    if (SUCCEEDED(hr))
    {
        hr = m_scriptEngine->Close();
    }

    return hr;
}


HRESULT BasicScriptHost::Parse(const std::wstring& source)
{
    IActiveScriptParsePtr parser;

    HRESULT hr = m_scriptEngine->QueryInterface(IID_IActiveScriptParse, reinterpret_cast<void **>(&parser));
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to get pointer to script parsing interface");
    }

    hr = parser->ParseScriptText(source.c_str(),
                                 nullptr,
                                 nullptr,
                                 nullptr,
                                 0,
                                 0,
                                 0,
                                 nullptr,
                                 nullptr);

    return hr;
}



HRESULT BasicScriptHost::OnScriptError(IActiveScriptError *err)
{
    EXCEPINFO e;

    err->GetExceptionInfo(&e);
    DWORD cookie;
    ULONG line;
    LONG charpos;
    err->GetSourcePosition(&cookie, &line, &charpos);
    std::wstring sMsg = CStringUtils::Format(L"Script error in file '%s':\nline %lu, pos %ld\n%s\nscode = %xld\nwcode = %xd", m_path.c_str(), line + 1, charpos + 1, (e.bstrDescription == nullptr ? L"unknown" : e.bstrDescription), e.scode, e.wCode);

    MessageBox(m_hWnd, sMsg.c_str(), L"BowPad Script error", MB_ICONERROR);

    return S_OK;
}


_variant_t BasicScriptHost::CallFunction(const std::wstring& strFunc,
                                         const std::vector<std::wstring>& paramArray)
{
    // Putting parameters
    DISPPARAMS dispparams = { 0 };
    const int arraySize = (int)paramArray.size();

    auto varmem = std::make_unique<VARIANT[]>(dispparams.cArgs);
    dispparams.cArgs = arraySize;
    dispparams.rgvarg = varmem.get();
    dispparams.cNamedArgs = 0;

    for (int i = 0; i < arraySize; i++)
    {
        _bstr_t bstr = paramArray[arraySize - 1 - i].c_str(); // back reading
        dispparams.rgvarg[i].bstrVal = bstr.Detach();
        dispparams.rgvarg[i].vt = VT_BSTR;
    }

    _variant_t vaResult = CallFunction(strFunc, dispparams);

    for (int i = 0; i < arraySize; i++)
    {
        SysFreeString(dispparams.rgvarg[i].bstrVal);
    }

    return vaResult;
}

_variant_t BasicScriptHost::CallFunction(const std::wstring& strFunc, DISPPARAMS& params)
{
    IDispatchPtr scriptDispatch;
    m_scriptEngine->GetScriptDispatch(nullptr, &scriptDispatch);
    // Find dispid for given function in the object
    _bstr_t bstrMember(strFunc.c_str());
    DISPID dispid = 0;

    HRESULT hr = scriptDispatch->GetIDsOfNames(IID_NULL, &bstrMember.GetBSTR(), 1, LOCALE_SYSTEM_DEFAULT, &dispid);
    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to get id of function");
    }
    // Call JavaScript function
    EXCEPINFO excepInfo = { 0 };
    _variant_t vaResult;
    UINT nArgErr = (UINT)-1;  // initialize to invalid arg

    hr = scriptDispatch->Invoke(dispid,
                                IID_NULL,
                                0,
                                DISPATCH_METHOD,
                                &params,
                                &vaResult,
                                &excepInfo,
                                &nArgErr);

    if (FAILED(hr))
    {
        throw std::runtime_error("Unable to get invoke function");
    }

    return vaResult;
}
