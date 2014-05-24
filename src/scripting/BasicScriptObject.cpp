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
#include "stdafx.h"
#include "BasicScriptObject.h"
#include "UnicodeUtils.h"

BasicScriptObject::BasicScriptObject(void * obj)
    : ICommand(obj)
    , m_refCount(1)
{}


BasicScriptObject::~BasicScriptObject()
{}


HRESULT BasicScriptObject::QueryInterface(REFIID riid, void ** object)
{
    if (riid == IID_IDispatch)
    {
        *object = static_cast<IDispatch*>(this);
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


ULONG BasicScriptObject::AddRef()
{
    return ::InterlockedIncrement(&m_refCount);
}


ULONG BasicScriptObject::Release()
{
    ULONG oldCount = m_refCount;

    ULONG newCount = ::InterlockedDecrement(&m_refCount);
    if (0 == newCount)
    {
        delete this;
    }

    return oldCount;
}


HRESULT BasicScriptObject::GetTypeInfoCount(UINT *count)
{
    *count = 0;

    return S_OK;
}


HRESULT BasicScriptObject::GetTypeInfo(UINT, LCID, ITypeInfo** typeInfo)
{
    *typeInfo = nullptr;

    return S_OK;
}

// This is where we register procs (or vars)
HRESULT BasicScriptObject::GetIDsOfNames(REFIID      /*riid*/,
                                         LPOLESTR*   nameList,
                                         UINT        nameCount,
                                         LCID        /*lcid*/,
                                         DISPID*     idList)
{
    // TODO: find a better way to handle the commands than with this ugly
    // if-else-if monster
    for (UINT i = 0; i < nameCount; i++)
    {
        if (_wcsicmp(nameList[i], L"alert") == 0)
            idList[i] = 1;
        else if (_wcsicmp(nameList[i], L"debugprint") == 0)
            idList[i] = 2;
        else if (_wcsicmp(nameList[i], L"SetInsertionIndex") == 0)
            idList[i] = 100;
        else if (_wcsicmp(nameList[i], L"TabActivateAt") == 0)
            idList[i] = 101;
        else if (_wcsicmp(nameList[i], L"UpdateTab") == 0)
            idList[i] = 102;
        else if (_wcsicmp(nameList[i], L"GetDocIdOfCurrentTab") == 0)
            idList[i] = 103;
        else if (_wcsicmp(nameList[i], L"GetActiveTabIndex") == 0)
            idList[i] = 104;
        else if (_wcsicmp(nameList[i], L"GetTabCount") == 0)
            idList[i] = 105;
        else if (_wcsicmp(nameList[i], L"GetCurrentTitle") == 0)
            idList[i] = 106;
        else if (_wcsicmp(nameList[i], L"GetTitleForTabIndex") == 0)
            idList[i] = 107;
        else if (_wcsicmp(nameList[i], L"GetTitleForDocID") == 0)
            idList[i] = 108;
        else if (_wcsicmp(nameList[i], L"SetCurrentTitle") == 0)
            idList[i] = 109;
        else if (_wcsicmp(nameList[i], L"CloseTab") == 0)
            idList[i] = 110;
        else if (_wcsicmp(nameList[i], L"GetDocIDFromTabIndex") == 0)
            idList[i] = 111;
        else if (_wcsicmp(nameList[i], L"GetDocIDFromPath") == 0)
            idList[i] = 112;
        else if (_wcsicmp(nameList[i], L"GetTabIndexFromDocID") == 0)
            idList[i] = 113;
        else if (_wcsicmp(nameList[i], L"DocumentCount") == 0)
            idList[i] = 114;
        else if (_wcsicmp(nameList[i], L"HasActiveDocument") == 0)
            idList[i] = 115;
        else if (_wcsicmp(nameList[i], L"HasDocumentID") == 0)
            idList[i] = 116;
        else if (_wcsicmp(nameList[i], L"UpdateStatusBar") == 0)
            idList[i] = 117;
        else if (_wcsicmp(nameList[i], L"SetupLexerForLang") == 0)
            idList[i] = 118;
        else if (_wcsicmp(nameList[i], L"DocScrollClear") == 0)
            idList[i] = 119;
        else if (_wcsicmp(nameList[i], L"DocScrollAddLineColor") == 0)
            idList[i] = 120;
        else if (_wcsicmp(nameList[i], L"DocScrollUpdate") == 0)
            idList[i] = 121;
        else if (_wcsicmp(nameList[i], L"DocScrollRemoveLine") == 0)
            idList[i] = 122;
        else if (_wcsicmp(nameList[i], L"GotoLine") == 0)
            idList[i] = 123;
        else if (_wcsicmp(nameList[i], L"Center") == 0)
            idList[i] = 124;
        else if (_wcsicmp(nameList[i], L"GotoBrace") == 0)
            idList[i] = 125;
        else if (_wcsicmp(nameList[i], L"GetTimerID") == 0)
            idList[i] = 126;
        else if (_wcsicmp(nameList[i], L"SetTimer") == 0)
            idList[i] = 127;
        else if (_wcsicmp(nameList[i], L"OpenFile") == 0)
            idList[i] = 128;
        else if (_wcsicmp(nameList[i], L"ReloadTab") == 0)
            idList[i] = 129;
        else if (_wcsicmp(nameList[i], L"SaveCurrentTab") == 0)
            idList[i] = 130;
        else if (_wcsicmp(nameList[i], L"InvalidateState") == 0)
            idList[i] = 131;
        // TODO: we need a lot of functions to deal with
        // the scintilla control - using ScintillaCall() is not
        // really an option since that deals with pointers
        else
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

// And this is where they are called from script
HRESULT BasicScriptObject::Invoke(DISPID      id,
                                  REFIID      /*riid*/,
                                  LCID        /*lcid*/,
                                  WORD        flags,
                                  DISPPARAMS* args,
                                  VARIANT*    ret,
                                  EXCEPINFO*  /*excp*/,
                                  UINT*       /*err*/)
{
    _variant_t dummyRet;
    if (ret)
        ret->vt = VT_EMPTY;
    else
        ret = &dummyRet;

    _variant_t p1;
    _variant_t p2;
    _variant_t p3;
    switch (id)
    {
        case 1: // alert
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            MessageBox(NULL, p1.bstrVal, L"Script Alert", MB_OK);
            break;
        case 2: // debugprint
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            CTraceToOutputDebugString::Instance()(L"BowPad : ");
            CTraceToOutputDebugString::Instance()(p1.bstrVal);
            CTraceToOutputDebugString::Instance()(L"\n");
            break;
        case 100: // SetInsertionIndex
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            SetInsertionIndex(p1.intVal);
            break;
        case 101: // TabActivateAt
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            TabActivateAt(p1.intVal);
            break;
        case 102: // UpdateTab
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            UpdateTab(p1.intVal);
            break;
        case 103: // GetDocIdOfCurrentTab
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            ret->vt = VT_INT;
            ret->intVal = GetDocIdOfCurrentTab();
            break;
        case 104: // GetActiveTabIndex
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            ret->vt = VT_INT;
            ret->intVal = GetActiveTabIndex();
            break;
        case 105: // GetTabCount
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            ret->vt = VT_INT;
            ret->intVal = GetTabCount();
            break;
        case 106: // GetCurrentTitle
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            ret->vt = VT_BSTR;
            ret->bstrVal = _bstr_t(GetCurrentTitle().c_str()).Detach();
            break;
        case 107: // GetTitleForTabIndex
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BSTR;
            ret->bstrVal = _bstr_t(GetTitleForTabIndex(p1.intVal).c_str()).Detach();
            break;
        case 108: // GetTitleForDocID
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BSTR;
            ret->bstrVal = _bstr_t(GetTitleForDocID(p1.intVal).c_str()).Detach();
            break;
        case 109: // SetCurrentTitle
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            SetCurrentTitle(p1.bstrVal);
            break;
        case 110: // CloseTab
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BOOL)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BOOL;
            ret->boolVal = CloseTab(p1.intVal, !!p2.boolVal);
            break;
        case 111: // GetDocIDFromTabIndex
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_INT;
            ret->intVal = GetDocIDFromTabIndex(p1.intVal);
            break;
        case 112: // GetDocIDFromPath
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_INT;
            ret->intVal = GetDocIDFromPath(p1.bstrVal);
            break;
        case 113: // GetTabIndexFromDocID
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_INT;
            ret->intVal = GetTabIndexFromDocID(p1.intVal);
            break;
        case 114: // DocumentCount
            if (flags == DISPATCH_PROPERTYGET)
            {
                ret->lVal = GetDocumentCount();
                ret->vt = VT_INT;
                return S_OK;
            }
            return DISP_E_TYPEMISMATCH;
            break;
        case 115: // HasActiveDocument
            if (flags == DISPATCH_PROPERTYGET)
            {
                ret->boolVal = HasActiveDocument();
                ret->vt = VT_BOOL;
                return S_OK;
            }
            return DISP_E_TYPEMISMATCH;
            break;
        case 116: // HasDocumentID
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BOOL;
            ret->boolVal = HasDocumentID(p1.intVal);
            break;
        case 117: // UpdateStatusBar
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BOOL)))
                return DISP_E_TYPEMISMATCH;
            UpdateStatusBar(!!p1.boolVal);
            break;
        case 118: // SetupLexerForLang
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            SetupLexerForLang(p1.bstrVal);
            break;
        case 119: // DocScrollClear
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            DocScrollClear(p1.intVal);
            break;
        case 120: // DocScrollAddLineColor
            if (args->cArgs != 3)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[2], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p3, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_UI4)))
                return DISP_E_TYPEMISMATCH;
            DocScrollAddLineColor(p1.intVal, p2.intVal, p3.ulVal);
            break;
        case 121: // DocScrollUpdate
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            DocScrollUpdate();
            break;
        case 122: // DocScrollRemoveLine
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            DocScrollRemoveLine(p1.intVal, p2.intVal);
            break;
        case 123: // GotoLine
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            GotoLine(p1.intVal);
            break;
        case 124: // Center
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            Center(p1.intVal, p2.intVal);
            break;
        case 125: // GotoBrace
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            GotoBrace();
            break;
        case 126: // GetTimerID
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            ret->vt = VT_INT;
            ret->intVal = GetTimerID();
            break;
        case 127: // SetTimer
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            SetTimer(GetHwnd(), p1.intVal, p2.intVal, NULL);
            break;
        case 128: // OpenFile
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_BSTR)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BOOL)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BOOL;
            ret->boolVal = OpenFile(p1.bstrVal, !!p2.boolVal);
            break;
        case 129: // ReloadTab
            if (args->cArgs != 2)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[1], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            if (FAILED(VariantChangeType(&p2, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_INT)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BOOL;
            ret->boolVal = ReloadTab(p1.intVal, p2.intVal);
            break;
        case 130: // SaveCurrentTab
            if (args->cArgs != 1)
                return DISP_E_BADPARAMCOUNT;
            if (FAILED(VariantChangeType(&p1, &args->rgvarg[0], VARIANT_ALPHABOOL, VT_BOOL)))
                return DISP_E_TYPEMISMATCH;
            ret->vt = VT_BOOL;
            ret->boolVal = SaveCurrentTab(!!p1.boolVal);
            break;
        case 131: // InvalidateState
            if (args->cArgs != 0)
                return DISP_E_BADPARAMCOUNT;
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
            InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Enabled);
            InvalidateUICommand(UI_INVALIDATIONS_STATE, NULL);
            break;

        default:
            return DISP_E_MEMBERNOTFOUND;
            break;
    }

    return S_OK;
}

