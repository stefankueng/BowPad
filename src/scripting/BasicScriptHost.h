// This file is part of BowPad.
//
// Copyright (C) 2014, 2021 - Stefan Kueng
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
    using Interface = IActiveScriptSite;

    explicit BasicScriptHost(const GUID& languageId);

    BasicScriptHost(const GUID& languageId, const std::wstring& objectName, IDispatchPtr object);

    virtual ~BasicScriptHost();

    // Implementation of IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
    ULONG STDMETHODCALLTYPE   AddRef() override;
    ULONG STDMETHODCALLTYPE   Release() override;

    // Implementation of IActiveScriptSite
    HRESULT STDMETHODCALLTYPE GetLCID(DWORD* lcid) override;
    HRESULT STDMETHODCALLTYPE GetDocVersionString(BSTR* ver) override;
    HRESULT STDMETHODCALLTYPE OnScriptTerminate(const VARIANT*, const EXCEPINFO*) override;
    HRESULT STDMETHODCALLTYPE OnStateChange(SCRIPTSTATE state) override;
    HRESULT STDMETHODCALLTYPE OnEnterScript() override;
    HRESULT STDMETHODCALLTYPE OnLeaveScript() override;
    HRESULT STDMETHODCALLTYPE GetItemInfo(const WCHAR* name, DWORD req, IUnknown** obj, ITypeInfo** type) override;
    HRESULT STDMETHODCALLTYPE OnScriptError(IActiveScriptError* err) override;

    // Our implementation
    virtual HRESULT Initialize();
    virtual HRESULT Parse(const std::wstring& script);
    virtual HRESULT Run();
    virtual HRESULT Terminate();

    virtual _variant_t CallFunction(const std::wstring& strFunc, const std::vector<std::wstring>& paramArray, bool doThrow = true);
    virtual _variant_t CallFunction(const std::wstring& strFunc, DISPPARAMS& params, bool doThrow = true);

    std::wstring m_path;
    HWND         m_hWnd;

private:
    ULONG m_refCount;

protected:
    IActiveScriptPtr m_scriptEngine;
    IDispatchPtr     m_application;
};
