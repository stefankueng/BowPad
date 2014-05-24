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
#include "stdafx.h"
#include "ICommand.h"
#include <string>
#include <comdef.h>

class BasicScriptObject : public IDispatch, ICommand
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
    virtual bool Execute() { return true; }
    virtual UINT GetCmdId() { return 0; }


private:

    static const std::wstring functionName;
    ICommand *                  m_Obj;

    ULONG                       m_refCount;
};
