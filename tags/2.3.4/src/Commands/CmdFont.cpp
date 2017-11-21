// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017 - Stefan Kueng
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
#include "CmdFont.h"
#include "IniSettings.h"
#include "AppUtils.h"
#include "UnicodeUtils.h"

CCmdFont::CCmdFont(void * obj) : ICommand(obj)
    , m_bBold(false)
    , m_bItalic(false)
    , m_size(11)
    , m_FontName(L"Consolas")
{
}

HRESULT CCmdFont::IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* ppropvarCurrentValue, PROPVARIANT* ppropvarNewValue)
{
    HRESULT hr = E_FAIL;
    if (key == UI_PKEY_FontProperties)
    {
        hr = E_POINTER;
        if (ppropvarCurrentValue != nullptr)
        {
            // Get the font values for the selected text in the font control.
            IPropertyStorePtr pValues;
            hr = UIPropertyToInterface(UI_PKEY_FontProperties, *ppropvarCurrentValue, &pValues);
            if (CAppUtils::FailedShowMessage(hr))
                return hr;
            PROPVARIANT propvar;
            PropVariantInit(&propvar);

            m_bBold = !!CIniSettings::Instance().GetInt64(L"View", L"FontBold", false);
            // Set the bold value to UI_FONTPROPERTIES_SET or UI_FONTPROPERTIES_NOTSET.
            UIInitPropertyFromUInt32(UI_PKEY_FontProperties_Bold, m_bBold ? UI_FONTPROPERTIES_SET : UI_FONTPROPERTIES_NOTSET, &propvar);
            // Set UI_PKEY_FontProperties_Bold value in property store.
            pValues->SetValue(UI_PKEY_FontProperties_Bold, propvar);
            PropVariantClear(&propvar);

            m_bItalic = !!CIniSettings::Instance().GetInt64(L"View", L"FontItalic", false);
            // Set the italic value to UI_FONTPROPERTIES_SET or UI_FONTPROPERTIES_NOTSET.
            UIInitPropertyFromUInt32(UI_PKEY_FontProperties_Italic, m_bItalic ? UI_FONTPROPERTIES_SET : UI_FONTPROPERTIES_NOTSET, &propvar);
            // Set UI_PKEY_FontProperties_Italic value in property store.
            pValues->SetValue(UI_PKEY_FontProperties_Italic, propvar);
            PropVariantClear(&propvar);

            m_FontName = CIniSettings::Instance().GetString(L"View", L"FontName", L"Consolas");
            // Set the font family value to the font name.
            UIInitPropertyFromString(UI_PKEY_FontProperties_Family, m_FontName.c_str(), &propvar);
            // Set UI_PKEY_FontProperties_Family value in property store.
            pValues->SetValue(UI_PKEY_FontProperties_Family, propvar);
            PropVariantClear(&propvar);

            m_size = (int)CIniSettings::Instance().GetInt64(L"View", L"FontSize", 11);
            DECIMAL decSize;
            // Font size value is available so get the font size.
            VarDecFromR8((DOUBLE)m_size, &decSize);
            // Set UI_PKEY_FontProperties_Size value in property store.
            UIInitPropertyFromDecimal(UI_PKEY_FontProperties_Size, decSize, &propvar);
            pValues->SetValue(UI_PKEY_FontProperties_Size, propvar);
            PropVariantClear(&propvar);

            // Provide the new values to the font control.
            hr = UIInitPropertyFromInterface(UI_PKEY_FontProperties, pValues, ppropvarNewValue);
        }
    }
    return hr;
}

HRESULT CCmdFont::IUICommandHandlerExecute(UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* /*ppropvarValue*/, IUISimplePropertySet* pCommandExecutionProperties)
{
    HRESULT hr = E_NOTIMPL;
    if (verb == UI_EXECUTIONVERB_CANCELPREVIEW)
    {
        // restore from saved values
        std::string sFontName = CUnicodeUtils::StdGetUTF8(m_FontName);
        CIniSettings::Instance().SetString(L"View", L"FontName", m_FontName.c_str());
        ScintillaCall(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)sFontName.c_str());

        CIniSettings::Instance().SetInt64(L"View", L"FontBold", m_bBold);
        CIniSettings::Instance().SetInt64(L"View", L"FontItalic", m_bItalic);
        CIniSettings::Instance().SetInt64(L"View", L"FontSize", m_size);

        ScintillaCall(SCI_STYLESETBOLD, STYLE_DEFAULT, m_bBold);
        ScintillaCall(SCI_STYLESETITALIC, STYLE_DEFAULT, m_bItalic);
        ScintillaCall(SCI_STYLESETSIZE, STYLE_DEFAULT, m_size);

        // refresh lexer
        const auto& doc = GetActiveDocument();
        SetupLexerForLang(doc.GetLanguage());
        return S_OK;
    }
    if (key && *key == UI_PKEY_FontProperties)
    {
        // Font properties have changed.
        PROPVARIANT varChanges;
        if (pCommandExecutionProperties == nullptr)
            return E_INVALIDARG;
        hr = pCommandExecutionProperties->GetValue(UI_PKEY_FontProperties_ChangedProperties, &varChanges);
        if (SUCCEEDED(hr))
        {
            IPropertyStorePtr pChanges;
            hr = UIPropertyToInterface(UI_PKEY_FontProperties, varChanges, &pChanges);
            if (SUCCEEDED(hr))
            {
                // Using the changed properties, set the new font on the selection on RichEdit control.
                PROPVARIANT propvar;
                PropVariantInit(&propvar);
                UINT uValue;

                // Get the bold value from the property store.
                if (SUCCEEDED(pChanges->GetValue(UI_PKEY_FontProperties_Bold, &propvar)))
                {
                    UIPropertyToUInt32(UI_PKEY_FontProperties_Bold, propvar, &uValue);
                    if ((UI_FONTPROPERTIES) uValue != UI_FONTPROPERTIES_NOTAVAILABLE)
                    {
                        if ((verb == UI_EXECUTIONVERB_EXECUTE) || (verb == UI_EXECUTIONVERB_PREVIEW))
                            CIniSettings::Instance().SetInt64(L"View", L"FontBold", uValue == UI_FONTPROPERTIES_SET);
                        if (verb == UI_EXECUTIONVERB_EXECUTE)
                            m_bBold = uValue == UI_FONTPROPERTIES_SET;
                    }
                }
                PropVariantClear(&propvar);

                // Get the italic value from the property store.
                if (SUCCEEDED(pChanges->GetValue(UI_PKEY_FontProperties_Italic, &propvar)))
                {
                    UIPropertyToUInt32(UI_PKEY_FontProperties_Italic, propvar, &uValue);
                    if ((UI_FONTPROPERTIES) uValue != UI_FONTPROPERTIES_NOTAVAILABLE)
                    {
                        if ((verb == UI_EXECUTIONVERB_EXECUTE) || (verb == UI_EXECUTIONVERB_PREVIEW))
                            CIniSettings::Instance().SetInt64(L"View", L"FontItalic", uValue == UI_FONTPROPERTIES_SET);
                        if (verb == UI_EXECUTIONVERB_EXECUTE)
                            m_bItalic = uValue == UI_FONTPROPERTIES_SET;
                    }
                }
                PropVariantClear(&propvar);

                // Get the font family value from the property store.
                if (SUCCEEDED(pChanges->GetValue(UI_PKEY_FontProperties_Family, &propvar)))
                {
                    // Get the string for the font family.
                    PWSTR pszFamily = nullptr;
                    UIPropertyToStringAlloc(UI_PKEY_FontProperties_Family, propvar, &pszFamily);
                    // Blank string is used as "Not Available" value.
                    if (lstrcmp(pszFamily, L""))
                    {
                        if ((verb == UI_EXECUTIONVERB_EXECUTE) || (verb == UI_EXECUTIONVERB_PREVIEW))
                            CIniSettings::Instance().SetString(L"View", L"FontName", pszFamily);
                        if (verb == UI_EXECUTIONVERB_EXECUTE)
                            m_FontName = pszFamily;
                    }
                    // Free the allocated string.
                    CoTaskMemFree(pszFamily);
                }
                PropVariantClear(&propvar);

                // Get the font size value from the property store.
                if (SUCCEEDED(pChanges->GetValue(UI_PKEY_FontProperties_Size, &propvar)))
                {
                    // Get the decimal font size value.
                    DECIMAL decSize;
                    UIPropertyToDecimal(UI_PKEY_FontProperties_Size, propvar, &decSize);
                    DOUBLE dSize;
                    VarR8FromDec(&decSize, &dSize);
                    // Zero is used as "Not Available" value.
                    if (dSize > 0)
                    {
                        if ((verb == UI_EXECUTIONVERB_EXECUTE) || (verb == UI_EXECUTIONVERB_PREVIEW))
                            CIniSettings::Instance().SetInt64(L"View", L"FontSize", (LONG)(dSize));
                        if (verb == UI_EXECUTIONVERB_EXECUTE)
                            m_size = (int)dSize;
                    }
                }
                PropVariantClear(&propvar);
            }
            PropVariantClear(&varChanges);
        }
        if ((verb == UI_EXECUTIONVERB_EXECUTE) || (verb == UI_EXECUTIONVERB_PREVIEW))
        {
            const auto& doc = GetActiveDocument();
            SetupLexerForLang(doc.GetLanguage());
        }
    }
    return hr;
}
