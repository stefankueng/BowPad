// This file is part of BowPad.
//
// Copyright (C) 2014, 2016, 2021 - Stefan Kueng
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
#include "ICommand.h"

struct ScintillaCmd
{
    const wchar_t* const functionName;
    UINT                 cmd;
    UINT                 cmdGet;
    VARTYPE              retVal;
    VARTYPE              p1;
    VARTYPE              p2;
};

// BasicScriptObject is not a command but inherits from ICommand only so
// it gets access to all the scintilla and BP commands ICommand provides.
// That's why the inheritance is private.

class BasicScriptObject : public IDispatch
    , /* private */ ICommand
{
public:
    BasicScriptObject(void* obj);

    virtual ~BasicScriptObject();

    // IUnknown implementation
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
    ULONG STDMETHODCALLTYPE   AddRef() override;
    ULONG STDMETHODCALLTYPE   Release() override;

    // IDispatch implementation
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override;
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo** typeInfo) override;
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* nameList, UINT nameCount, LCID lcid, DISPID* idList) override;
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS* args, VARIANT* ret, EXCEPINFO* excp, UINT* err) override;

    // ICommand implementation
    bool Execute() override { return true; }
    UINT GetCmdId() override { return 0; }

private:
    static bool ScintillaCommandsDispId(wchar_t* name, DISPID& id);
    HRESULT     ScintillaCommandInvoke(DISPID id, WORD flags, DISPPARAMS* args, VARIANT* ret) const;
    ULONG       m_refCount;
};
