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
#pragma once
#include "ICommand.h"
#include <string>
#include <comdef.h>
#include <vector>

struct ScintillaCmd
{
    const wchar_t*const functionname;
    UINT            cmd;
    UINT            cmdget;
    VARTYPE         retval;
    VARTYPE         p1;
    VARTYPE         p2;
};

// BasicScriptObject is not a command but inherits from ICommand only so
// it gets access to all the scintilla and BP commands ICommand provides.
// That's why the inheritance is private.

class BasicScriptObject : public IDispatch, /* private */ ICommand
{
public:

    BasicScriptObject(void * obj);

    virtual ~BasicScriptObject();

    // IUnknown implementation
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** object);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IDispatch implementation
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *count);
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo** typeInfo);
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR * nameList, UINT nameCount, LCID lcid, DISPID * idList);
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS * args, VARIANT * ret, EXCEPINFO * excp, UINT * err);

    // ICommand implementation
    bool Execute() override { return true; }
    UINT GetCmdId() override { return 0; }

private:
    bool ScintillaCommandsDispId(wchar_t * name, DISPID& id);
    HRESULT ScintillaCommandInvoke(DISPID id, WORD flags, DISPPARAMS* args, VARIANT* ret);
    ULONG                       m_refCount;
};
