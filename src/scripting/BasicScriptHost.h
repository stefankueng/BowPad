// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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

#include <string>
#include <vector>
#include <activscp.h>
#include <comdef.h>


class BasicScriptObject;


class BasicScriptHost : public IActiveScriptSite
{
public:

    typedef IActiveScriptSite Interface;

    explicit BasicScriptHost(const GUID& languageId);

    BasicScriptHost(const GUID& languageId, const std::wstring& objectName, IDispatchPtr object);


    virtual ~BasicScriptHost();

    // Implementation of IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** object);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // Implementation of IActiveScriptSite
    virtual HRESULT STDMETHODCALLTYPE GetLCID(DWORD *lcid);
    virtual HRESULT STDMETHODCALLTYPE GetDocVersionString(BSTR* ver);
    virtual HRESULT STDMETHODCALLTYPE OnScriptTerminate(const VARIANT *, const EXCEPINFO *);
    virtual HRESULT STDMETHODCALLTYPE OnStateChange(SCRIPTSTATE state);
    virtual HRESULT STDMETHODCALLTYPE OnEnterScript();
    virtual HRESULT STDMETHODCALLTYPE OnLeaveScript();
    virtual HRESULT STDMETHODCALLTYPE GetItemInfo(const WCHAR * name, DWORD req, IUnknown ** obj, ITypeInfo ** type);
    virtual HRESULT STDMETHODCALLTYPE OnScriptError(IActiveScriptError *err);

    // Our implementation
    virtual HRESULT Initialize();
    virtual HRESULT Parse(const std::wstring& script);
    virtual HRESULT Run();
    virtual HRESULT Terminate();

    virtual _variant_t CallFunction(const std::wstring& strFunc, const std::vector<std::wstring>& paramArray);
    virtual _variant_t CallFunction(const std::wstring& strFunc, DISPPARAMS& params);

    std::wstring            m_path;
    HWND                    m_hWnd;
private:

    ULONG                   m_refCount;

protected:

    IActiveScriptPtr        m_scriptEngine;
    IDispatchPtr            m_application;
};
