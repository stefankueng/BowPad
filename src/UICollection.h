// This file is part of BowPad.
//
// Copyright (C) 2021 Stefan Kueng
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
#include <vector>
#include <UIRibbon.h>

class CUICollection : public IUICollection
{
public:
    CUICollection();
    virtual ~CUICollection();

    // Inherited via IUnknown
    virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override;
    virtual ULONG __stdcall AddRef() override;
    virtual ULONG __stdcall Release() override;
    // Inherited via IUICollection
    virtual HRESULT __stdcall GetCount(UINT32* count) override;
    virtual HRESULT __stdcall GetItem(UINT32 index, IUnknown** item) override;
    virtual HRESULT __stdcall Add(IUnknown* item) override;
    virtual HRESULT __stdcall Insert(UINT32 index, IUnknown* item) override;
    virtual HRESULT __stdcall RemoveAt(UINT32 index) override;
    virtual HRESULT __stdcall Replace(UINT32 indexReplaced, IUnknown* itemReplaceWith) override;
    virtual HRESULT __stdcall Clear() override;

private:
    LONG                     m_cRef;
    std::vector<IUnknownPtr> m_data;
};