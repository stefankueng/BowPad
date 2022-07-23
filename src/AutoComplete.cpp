// This file is part of BowPad.
//
// Copyright (C) 2021-2022 - Stefan Kueng
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
#include "AutoComplete.h"
#include "AppUtils.h"
#include "BowPad.h"
#include "DarkModeHelper.h"
#include "DirFileEnum.h"
#include "LexStyles.h"
#include "MainWindow.h"
#include "OnOutOfScope.h"
#include "SciLexer.h"
#include "ScintillaWnd.h"
#include "SmartHandle.h"
#include "StringUtils.h"
#include "Theme.h"
#include "UnicodeUtils.h"
#include "SciTextReader.h"
#include "../ext/tinyexpr/tinyexpr.h"

#include <chrono>

constexpr int fullSnippetPosId = 9999;

static HICON  LoadIconEx(HINSTANCE hInstance, LPCWSTR lpIconName, int iconWidth, int iconHeight)
{
    // the docs for LoadIconWithScaleDown don't mention that a size of 0 will
    // use the default system icon size like for e.g. LoadImage or LoadIcon.
    // So we don't assume that this works but do it ourselves.
    if (iconWidth == 0)
        iconWidth = GetSystemMetrics(SM_CXSMICON);
    if (iconHeight == 0)
        iconHeight = GetSystemMetrics(SM_CYSMICON);
    HICON hIcon = nullptr;
    if (FAILED(LoadIconWithScaleDown(hInstance, lpIconName, iconWidth, iconHeight, &hIcon)))
    {
        // fallback, just in case
        hIcon = static_cast<HICON>(LoadImage(hInstance, lpIconName, IMAGE_ICON, iconWidth, iconHeight, LR_DEFAULTCOLOR));
    }
    return hIcon;
}

static std::unique_ptr<UINT[]> Icon2Image(HICON hIcon)
{
    if (hIcon == nullptr)
        return nullptr;

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo))
        return nullptr;

    BITMAP bm;
    if (!GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bm))
        return nullptr;

    int        width            = bm.bmWidth;
    int        height           = bm.bmHeight;
    int        bytesPerScanLine = (width * 3 + 3) & 0xFFFFFFFC;
    int        size             = bytesPerScanLine * height;
    BITMAPINFO infoHeader;
    infoHeader.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    infoHeader.bmiHeader.biWidth       = width;
    infoHeader.bmiHeader.biHeight      = height;
    infoHeader.bmiHeader.biPlanes      = 1;
    infoHeader.bmiHeader.biBitCount    = 24;
    infoHeader.bmiHeader.biCompression = BI_RGB;
    infoHeader.bmiHeader.biSizeImage   = size;

    auto   ptrb                        = std::make_unique<BYTE[]>(2LL * size + 4LL * height * width);
    LPBYTE pixelsIconRGB               = ptrb.get();
    LPBYTE alphaPixels                 = pixelsIconRGB + size;
    HDC    hDC                         = CreateCompatibleDC(nullptr);
    OnOutOfScope(DeleteDC(hDC));
    HBITMAP hBmpOld = static_cast<HBITMAP>(SelectObject(hDC, static_cast<HGDIOBJ>(iconInfo.hbmColor)));
    if (!GetDIBits(hDC, iconInfo.hbmColor, 0, height, static_cast<LPVOID>(pixelsIconRGB), &infoHeader, DIB_RGB_COLORS))
        return nullptr;

    SelectObject(hDC, hBmpOld);
    if (!GetDIBits(hDC, iconInfo.hbmMask, 0, height, static_cast<LPVOID>(alphaPixels), &infoHeader, DIB_RGB_COLORS))
        return nullptr;

    auto imagePixels = std::make_unique<UINT[]>(static_cast<size_t>(height) * width);
    int  lsSrc       = width * 3;
    int  vsDest      = height - 1;
    for (int y = 0; y < height; y++)
    {
        int linePosSrc  = (vsDest - y) * lsSrc;
        int linePosDest = y * width;
        for (int x = 0; x < width; x++)
        {
            int currentDestPos          = linePosDest + x;
            int currentSrcPos           = linePosSrc + x * 3;
            imagePixels[currentDestPos] = (static_cast<UINT>((
                                                                 ((pixelsIconRGB[currentSrcPos + 2] /*Red*/) | (pixelsIconRGB[currentSrcPos + 1] << 8 /*Green*/)) | pixelsIconRGB[currentSrcPos] << 16 /*Blue*/
                                                                 ) |
                                                             ((alphaPixels[currentSrcPos] ? 0 : 0xff) << 24)) &
                                           0xffffffff);
        }
    }
    return imagePixels;
}

static bool isAllowedBeforeDriveLetter(wchar_t c)
{
    return c == '\'' || c == '/' || c == '"' || c == '(' || c == '{' || c == '[' || std::isspace(c);
}

static bool getRawPath(const std::wstring& input, std::wstring& rawPathOut)
{
    // Try to find a path in the given input.
    // Algorithm: look for a colon. The colon must be preceded by an alphabetic character.
    // The alphabetic character must, in turn, be preceded by nothing, or by whitespace, or by
    // a quotation mark.
    size_t lastOccurrence = input.rfind(':');
    if (lastOccurrence == std::string::npos) // No match.
        return false;
    else if (lastOccurrence == 0)
        return false;
    else if (!std::isalpha(input[lastOccurrence - 1]))
        return false;
    else if (lastOccurrence >= 2 && !isAllowedBeforeDriveLetter(input[lastOccurrence - 2]))
        return false;

    rawPathOut = input.substr(lastOccurrence - 1);
    return true;
}

static bool getPathsForPathCompletion(const std::wstring& input, std::wstring& pathRaw, std::wstring& pathToMatchOut)
{
    std::wstring rawPath;
    if (!getRawPath(input, rawPath))
    {
        return false;
    }
    else if (PathIsDirectory(rawPath.c_str()))
    {
        pathToMatchOut = rawPath;
        pathRaw        = rawPath;
        return true;
    }
    else
    {
        auto lastOccurrence = rawPath.find_last_of(L"\\/");
        if (lastOccurrence == std::string::npos) // No match.
            return false;
        else
        {
            pathToMatchOut = rawPath.substr(0, lastOccurrence);
            pathRaw        = rawPath;
            return true;
        }
    }
}

CAutoComplete::CAutoComplete(CMainWindow* main, CScintillaWnd* scintilla)
    : m_editor(scintilla)
    , m_main(main)
    , m_insertingSnippet(false)
    , m_currentSnippetPos(-1)
{
}

CAutoComplete::~CAutoComplete()
{
}

void CAutoComplete::Init()
{
    {
        std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
        m_langWordList.clear();
        m_langSnippetList.clear();
    }
    int iconWidth  = GetSystemMetrics(SM_CXSMICON);
    int iconHeight = GetSystemMetrics(SM_CYSMICON);
    m_editor->Scintilla().RGBAImageSetWidth(iconWidth);
    m_editor->Scintilla().RGBAImageSetHeight(iconHeight);
    m_editor->Scintilla().AutoCSetIgnoreCase(TRUE);
    m_editor->Scintilla().AutoCStops("([.");
    int i = 0;
    for (auto icon : {IDI_SCI_CODE, IDI_SCI_FILE, IDI_SCI_SNIPPET, IDI_WORD})
    {
        CAutoIcon hIcon = LoadIconEx(g_hInst, MAKEINTRESOURCE(icon), iconWidth, iconHeight);
        auto      bytes = Icon2Image(hIcon);
        m_editor->Scintilla().RegisterRGBAImage(i, reinterpret_cast<const char*>(bytes.get()));
        ++i;
    }

    std::vector<std::unique_ptr<CSimpleIni>> iniFiles;

    DWORD                                    resLen  = 0;
    const char*                              resData = CAppUtils::GetResourceData(L"config", IDR_AUTOCOMPLETE, resLen);
    if (resData != nullptr && resLen)
    {
        iniFiles.push_back(std::make_unique<CSimpleIni>());
        iniFiles.back()->SetUnicode();
        iniFiles.back()->LoadFile(resData, resLen);
    }
    auto iniPath = CAppUtils::GetDataPath() + L"\\autocomplete.ini";
    iniFiles.push_back(std::make_unique<CSimpleIni>());
    iniFiles.back()->SetUnicode();
    iniFiles.back()->LoadFile(iniPath.c_str());
    for (const auto& ini : iniFiles)
    {
        std::list<const wchar_t*> sections;
        ini->GetAllSections(sections);
        for (const auto& section : sections)
        {
            std::list<const wchar_t*> keys;
            ini->GetAllKeys(section, keys);
            auto csMap = m_langSnippetList[CUnicodeUtils::StdGetUTF8(section)];

            for (const auto& key : keys)
            {
                if (_wcsnicmp(key, L"snippet_", 8) == 0)
                {
                    const std::wstring snippetVal = ini->GetValue(section, key, L"");
                    auto               snipp      = CUnicodeUtils::StdGetUTF8(key);
                    if (!snippetVal.empty())
                    {
                        auto sSnippetVal = CUnicodeUtils::StdGetUTF8(snippetVal);
                        SearchReplace(sSnippetVal, "\\r\\n", "\n");
                        SearchReplace(sSnippetVal, "\\n", "\n");
                        SearchReplace(sSnippetVal, "\\t", "\t");
                        SearchReplace(sSnippetVal, "\\b", "\b");
                        csMap[snipp.substr(8)] = sSnippetVal;
                    }
                    else
                        csMap.erase(snipp.substr(8));
                }
            }

            std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
            m_langSnippetList[CUnicodeUtils::StdGetUTF8(section)] = std::move(csMap);
        }
    }

    auto autocompleteWords = CLexStyles::Instance().GetAutoCompleteWords();
    for (const auto& [lang, words] : autocompleteWords)
    {
        std::map<std::string, AutoCompleteType, ci_less> acMap;
        std::vector<std::string>                         values;
        stringtok(values, words, true, " ");
        for (const auto& v : values)
            acMap[v] = AutoCompleteType::Code;
        std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
        m_langWordList[lang] = std::move(acMap);
    }
    const auto& langDataMap = CLexStyles::Instance().GetLanguageDataMap();
    for (const auto& [lang, data] : langDataMap)
    {
        if (!data.keywordList.empty())
        {
            std::map<std::string, AutoCompleteType, ci_less> acMap;
            for (const auto& [id, keywordString] : data.keywordList)
            {
                std::vector<std::string> values;
                stringtok(values, keywordString, true, " ");
                for (const auto& v : values)
                {
                    if (v.size() > 4 && std::isalpha(v[0]))
                        acMap[v] = AutoCompleteType::Code;
                }
            }
            if (!acMap.empty())
                m_langWordList[lang].insert(acMap.begin(), acMap.end());
        }
    }
}

void CAutoComplete::HandleScintillaEvents(const SCNotification* scn)
{
    switch (scn->nmhdr.code)
    {
        case SCN_CHARADDED:
            HandleAutoComplete(scn);
            break;
        case SCN_AUTOCSELECTION:
        {
            m_insertingSnippet = true;
            OnOutOfScope(m_insertingSnippet = false);
            if (scn && scn->text)
            {
                std::string sText = scn->text;
                if (!sText.empty())
                {
                    auto colonPos = sText.find(':');
                    if (colonPos != std::string::npos)
                    {
                        auto        sKey = sText.substr(0, colonPos);
                        std::string sSnippet;
                        {
                            std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
                            auto                                  docID      = m_main->m_tabBar.GetCurrentTabId();
                            auto                                  lang       = m_main->m_docManager.GetDocumentFromID(docID).GetLanguage();
                            const auto&                           snippetMap = m_langSnippetList[lang];
                            auto                                  foundIt    = snippetMap.find(sKey);
                            if (foundIt != snippetMap.end())
                            {
                                sSnippet = foundIt->second;
                            }
                        }
                        if (!sSnippet.empty())
                        {
                            m_editor->Scintilla().AutoCCancel();
                            auto autoBrace = CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1);
                            CIniSettings::Instance().SetInt64(L"View", L"autobrace", 0);
                            OnOutOfScope(CIniSettings::Instance().SetInt64(L"View", L"autobrace", autoBrace));
                            m_main->m_bBlockAutoIndent = true;
                            OnOutOfScope(m_main->m_bBlockAutoIndent = false);
                            auto pos           = m_editor->Scintilla().CurrentPos();
                            auto startIndent   = m_editor->Scintilla().LineIndentation(m_editor->GetCurrentLineNumber());
                            auto tabWidth      = m_editor->Scintilla().TabWidth();
                            auto indentToStart = [&]() {
                                auto tabCount = startIndent / tabWidth;
                                for (int i = 0; i < tabCount; ++i)
                                    m_editor->Scintilla().Tab();
                                auto spaceCount = startIndent % tabWidth;
                                for (int i = 0; i < spaceCount; ++i)
                                    m_editor->Scintilla().ReplaceSel(" ");
                            };

                            m_editor->Scintilla().BeginUndoAction();
                            m_editor->Scintilla().SetSelection(scn->position, pos);
                            m_editor->Scintilla().DeleteRange(scn->position, pos - scn->position);

                            sptr_t cursorPos  = -1;
                            char   lastC      = 0;
                            char   last2C     = 0;
                            int    hotSpotNum = -1;
                            ExitSnippetMode();
                            m_snippetPositions[fullSnippetPosId].push_back(m_editor->Scintilla().CurrentPos());
                            bool lastWasNewLine = false;
                            for (const auto& c : sSnippet)
                            {
                                if (c == '^' && lastC != '\\')
                                {
                                    cursorPos = m_editor->Scintilla().CurrentPos();
                                    if (hotSpotNum >= 0)
                                        m_snippetPositions[hotSpotNum].push_back(cursorPos);
                                    hotSpotNum = -1;
                                }
                                else if (lastC == '^' && last2C != '\\' && isdigit(c) && hotSpotNum < 0)
                                {
                                    hotSpotNum = c - '0';
                                    m_snippetPositions[c - '0'].push_back(cursorPos);
                                }
                                else
                                {
                                    if (lastC == '\\')
                                    {
                                        if (lastWasNewLine)
                                            indentToStart();
                                        lastWasNewLine = false;

                                        char text[]    = {c, 0};
                                        m_editor->Scintilla().ReplaceSel(text);
                                    }
                                    else if (c == '\t')
                                    {
                                        if (lastWasNewLine)
                                            indentToStart();
                                        lastWasNewLine = false;

                                        m_editor->Scintilla().Tab();
                                    }
                                    else if (c == '\n')
                                    {
                                        m_editor->Scintilla().NewLine();
                                        lastWasNewLine = true;
                                    }
                                    else if (c == '\b')
                                    {
                                        m_editor->Scintilla().DeleteBack();
                                    }
                                    else if (c != '\\')
                                    {
                                        if (lastWasNewLine)
                                            indentToStart();
                                        lastWasNewLine = false;

                                        char text[]    = {c, 0};
                                        m_editor->Scintilla().ReplaceSel(text);
                                    }
                                }
                                last2C = lastC;
                                lastC  = c;
                            }
                            m_snippetPositions[fullSnippetPosId].push_back(m_editor->Scintilla().CurrentPos());
                            if (!m_snippetPositions.contains(0))
                            {
                                auto pos0 = m_editor->Scintilla().CurrentPos();
                                m_snippetPositions[0].push_back(pos0);
                                m_snippetPositions[0].push_back(pos0);
                            }
                            if (m_snippetPositions.size() > 2)
                            {
                                const auto& posVec = m_snippetPositions[1];
                                bool        first  = true;
                                for (auto it = posVec.cbegin(); it != posVec.cend(); ++it)
                                {
                                    auto a = *it;
                                    ++it;
                                    auto b = *it;
                                    if (first)
                                    {
                                        m_editor->Scintilla().SetSelection(a, b);
                                        first = false;
                                    }
                                    else
                                    {
                                        m_editor->Scintilla().AddSelection(a, b);
                                    }
                                }
                                m_currentSnippetPos = 1;
                            }
                            else if (m_snippetPositions.size() == 2)
                            {
                                if (m_snippetPositions.contains(0))
                                    m_editor->Scintilla().SetSelection(*m_snippetPositions[0].cbegin(), *m_snippetPositions[0].cbegin());
                                ExitSnippetMode();
                            }
                            m_editor->Scintilla().EndUndoAction();
                            MarkSnippetPositions(false);
                        }
                        else if (!PathFileExists(CUnicodeUtils::StdGetUnicode(sText).c_str()))
                        {
                            m_editor->Scintilla().AutoCCancel();
                        }
                    }
                }
            }
        }
        break;
        case SCN_AUTOCSELECTIONCHANGE:
        {
            if (scn->text == nullptr || scn->text[0] == 0)
            {
                if (!m_stringToSelect.empty())
                {
                    m_editor->Scintilla().AutoCSelect(m_stringToSelect.c_str());
                }
            }
        }
        break;
        case SCN_AUTOCCANCELLED:
        case SCN_AUTOCCOMPLETED:
            m_stringToSelect.clear();
            break;
        case SCN_MODIFIED:
        {
            if (scn->modificationType & (SC_MOD_DELETETEXT | SC_MOD_INSERTTEXT))
            {
                if (m_currentSnippetPos >= 0 && !m_snippetPositions.empty())
                {
                    if (scn->linesAdded != 0)
                    {
                        ExitSnippetMode();
                    }
                    else
                    {
                        Sci_Position lastRootPos               = -1;
                        bool         rootPositionsAreDifferent = false;
                        for (auto& [id, snippetPositions] : m_snippetPositions)
                        {
                            bool         odd                   = true;
                            Sci_Position lastPos               = -1;
                            bool         positionsAreDifferent = false;
                            for (auto& pos : snippetPositions)
                            {
                                if (pos < 0)
                                    continue;
                                if (scn->modificationType & SC_MOD_INSERTTEXT)
                                {
                                    if (odd)
                                    {
                                        if (pos > scn->position)
                                            pos += scn->length;
                                    }
                                    else
                                    {
                                        if (pos >= scn->position)
                                            pos += scn->length;
                                    }
                                }
                                else if (scn->modificationType & SC_MOD_DELETETEXT)
                                {
                                    if (pos > scn->position)
                                        pos -= scn->length;
                                }
                                odd = !odd;
                                if (lastPos >= 0 && lastPos != pos)
                                    positionsAreDifferent = true;
                                lastPos = pos;
                            }
                            if (positionsAreDifferent || (lastRootPos >= 0 && lastRootPos != lastPos))
                                rootPositionsAreDifferent = true;
                            lastRootPos = lastPos;
                        }
                        if (rootPositionsAreDifferent)
                            MarkSnippetPositions(false);
                        else
                            ExitSnippetMode();
                    }
                }
            }
        }
        break;
        default:
            break;
    }
}

bool CAutoComplete::HandleChar(WPARAM wParam, LPARAM /*lParam*/)
{
    if (m_snippetPositions.empty())
        return true;
    if (wParam == VK_TAB || wParam == VK_RETURN)
    {
        auto lastSnippetPos = m_currentSnippetPos;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            --m_currentSnippetPos;
        else
            ++m_currentSnippetPos;
        if (m_currentSnippetPos <= 0)
        {
            m_currentSnippetPos = static_cast<int>(m_snippetPositions.size() - 1);
        }
        if (m_currentSnippetPos >= static_cast<int>(m_snippetPositions.size() - 1))
        {
            if (wParam == VK_TAB && lastSnippetPos != 1)
                m_currentSnippetPos = 1;
            else
            {
                if (m_snippetPositions.contains(0))
                {
                    if (*m_snippetPositions[0].cbegin() >= 0)
                        m_editor->Scintilla().SetSelection(*m_snippetPositions[0].cbegin(), *m_snippetPositions[0].cbegin());
                }
                ExitSnippetMode();
                return false;
            }
        }
        if (m_currentSnippetPos < static_cast<int>(m_snippetPositions.size()))
        {
            const auto& posVec = m_snippetPositions[m_currentSnippetPos];
            bool        first  = true;
            for (auto it = posVec.cbegin(); it != posVec.cend(); ++it)
            {
                auto a = *it;
                ++it;
                auto b = *it;
                if (first)
                {
                    m_editor->Scintilla().SetSelection(a, b);
                    first = false;
                }
                else
                {
                    m_editor->Scintilla().AddSelection(a, b);
                }
            }
            return false;
        }
    }
    if (wParam == VK_ESCAPE)
    {
        ExitSnippetMode();
    }
    return true;
}

void CAutoComplete::AddWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
    m_langWordList[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
    m_langWordList[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
    m_docWordList[docID].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
    m_docWordList[docID].insert(words.begin(), words.end());
}

void CAutoComplete::HandleAutoComplete(const SCNotification* scn)
{
    static constexpr auto wordSeparator = '\n';
    static constexpr auto typeSeparator = '?';

    if (m_insertingSnippet)
        return;
    auto pos = m_editor->Scintilla().CurrentPos();
    // if we're typing outside an active snippet, cancel
    // the snippet editing mode
    if (m_currentSnippetPos >= 0 && !m_snippetPositions.empty())
    {
        bool isInside = false;
        for (auto& [id, snippetPositions] : m_snippetPositions)
        {
            bool         odd     = true;
            Sci_Position lastPos = -1;
            for (auto& sPos : snippetPositions)
            {
                if (sPos < 0)
                    continue;
                if (id != 0 && id != fullSnippetPosId && !odd)
                {
                    if (pos >= lastPos && pos <= sPos)
                        isInside = true;
                }
                odd     = !odd;
                lastPos = sPos;
            }
        }
        if (!isInside)
            ExitSnippetMode();
        else
            return;
    }

    if (pos != m_editor->Scintilla().WordEndPosition(pos, TRUE))
        return; // don't auto complete if we're not at the end of a word
    auto word        = m_editor->GetCurrentWord();

    // path completion: get the current line
    auto currentLine = CUnicodeUtils::StdGetUnicode(m_editor->GetCurrentLine());
    CStringUtils::rtrim(currentLine, L"\r\n");
    std::wstring pathToMatch, rawPath;
    if (getPathsForPathCompletion(currentLine, rawPath, pathToMatch))
    {
        if (m_editor->Scintilla().AutoCActive())
            return;

        auto lineStartPos = m_editor->Scintilla().PositionFromLine(m_editor->Scintilla().LineFromPosition(pos));
        if (currentLine.find(rawPath) == (pos - lineStartPos - CUnicodeUtils::StdGetUTF8(rawPath).size()))
        {
            std::string pathComplete;
            if (pathToMatch.ends_with(':'))
                pathToMatch += '\\';
            SearchReplace(pathToMatch, L"/", L"\\");
            CDirFileEnum   fileFinder(pathToMatch);
            bool           bIsDirectory;
            std::wstring   filename;

            auto           startTime   = std::chrono::steady_clock::now();
            constexpr auto maxPathTime = std::chrono::milliseconds(400);
            auto           rootDir     = rawPath.substr(0, rawPath.find_last_of(L"\\/") + 1);
            while (fileFinder.NextFile(filename, &bIsDirectory, false))
            {
                if (rootDir.size() < filename.size())
                {
                    filename = rootDir + filename.substr(rootDir.size());
                    pathComplete += (CUnicodeUtils::StdGetUTF8(filename) + typeSeparator + std::to_string(static_cast<int>(AutoCompleteType::Path)) + wordSeparator);
                }
                auto elapsedPeriod = std::chrono::steady_clock::now() - startTime;
                if (elapsedPeriod > maxPathTime)
                    break;
            }
            if (!pathComplete.empty())
            {
                m_editor->Scintilla().AutoCSetAutoHide(TRUE);
                m_editor->Scintilla().AutoCSetSeparator(static_cast<uptr_t>(wordSeparator));
                m_editor->Scintilla().AutoCSetTypeSeparator(static_cast<uptr_t>(typeSeparator));
                if (CTheme::Instance().IsDarkTheme())
                    m_editor->Scintilla().AutoCSetOptions(Scintilla::AutoCompleteOption::FixedSize);
                m_editor->Scintilla().AutoCShow(CUnicodeUtils::StdGetUTF8(rawPath).size(), pathComplete.c_str());
                SetWindowStylesForAutocompletionPopup();
                return;
            }
        }
    }

    if (scn->ch == '=')
    {
        auto curLine = m_editor->GetCurrentLine();
        curLine      = curLine.substr(0, curLine.size() - 1);
        if (!curLine.empty())
        {
            int  err       = 0;
            auto exprValue = te_interp(curLine.c_str(), &err);
            if (err == 0 && exprValue)
            {
                long long ulongVal       = static_cast<long long>(exprValue);
                auto      sValueComplete = CStringUtils::Format("%f\n%lld\n0x%llX\n%#llo",
                                                                exprValue, ulongVal, ulongVal, ulongVal);

                m_editor->Scintilla().AutoCSetAutoHide(TRUE);
                m_editor->Scintilla().AutoCSetSeparator(static_cast<uptr_t>(wordSeparator));
                m_editor->Scintilla().AutoCSetTypeSeparator(static_cast<uptr_t>(typeSeparator));
                if (CTheme::Instance().IsDarkTheme())
                    m_editor->Scintilla().AutoCSetOptions(Scintilla::AutoCompleteOption::FixedSize);
                m_editor->Scintilla().AutoCShow(CUnicodeUtils::StdGetUTF8(rawPath).size(), sValueComplete.c_str());
                SetWindowStylesForAutocompletionPopup();
                return;
            }
            curLine = curLine.substr(1);
        }
    }

    if (scn->ch == '/')
    {
        auto lexer = m_editor->Scintilla().Lexer();
        switch (lexer)
        {
            // add the closing tag only for xml and html lexers
            case SCLEX_XML:
            case SCLEX_HTML:
            {
                auto pos2  = pos - 2;
                auto inner = 1;
                if (pos2 > 2)
                {
                    if (m_editor->Scintilla().CharAt(pos2) == '<')
                    {
                        auto docLen = m_editor->Scintilla().Length();
                        do
                        {
                            auto savedPos2 = pos2;
                            pos2           = m_editor->FindText("<", pos2 - 2, 0);
                            if (pos2)
                            {
                                auto nextChar = m_editor->Scintilla().CharAt(pos2 + 1);
                                if (nextChar == '!')
                                    continue;
                                auto closeBracketPos = m_editor->FindText("/>", pos2, docLen);
                                if (closeBracketPos > savedPos2 || closeBracketPos < 0)
                                {
                                    if (nextChar == '/')
                                        ++inner;
                                    else
                                        --inner;
                                }
                            }
                        } while (inner && pos2 > 2);
                        std::string tagName;
                        auto        position = pos2 + 1;
                        int         nextChar = static_cast<int>(m_editor->Scintilla().CharAt(position));
                        while (position < pos && !m_editor->IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                        {
                            tagName.push_back(static_cast<char>(nextChar));
                            ++position;
                            nextChar = static_cast<int>(m_editor->Scintilla().CharAt(position));
                        }
                        if (!tagName.empty())
                        {
                            if (tagName.starts_with("?"))
                                tagName = tagName.substr(1);
                            tagName = tagName + ">" + typeSeparator + "-1";
                            m_editor->Scintilla().AutoCSetAutoHide(TRUE);
                            m_editor->Scintilla().AutoCSetSeparator(static_cast<uptr_t>(wordSeparator));
                            m_editor->Scintilla().AutoCSetTypeSeparator(static_cast<uptr_t>(typeSeparator));
                            if (CTheme::Instance().IsDarkTheme())
                                m_editor->Scintilla().AutoCSetOptions(Scintilla::AutoCompleteOption::FixedSize);
                            m_editor->Scintilla().AutoCShow(CUnicodeUtils::StdGetUTF8(rawPath).size(), tagName.c_str());
                            SetWindowStylesForAutocompletionPopup();
                            return;
                        }
                    }
                }
            }
            break;
            default:
                break;
        }
    }
    if (word.empty() && pos > 2)
    {
        if ((m_editor->Scintilla().CharAt(pos - 1) == ' ') && (m_editor->Scintilla().CharAt(pos - 2) != ' '))
        {
            auto startPos = m_editor->Scintilla().WordStartPosition(pos - 2, true);
            auto endPos   = m_editor->Scintilla().WordEndPosition(pos - 2, true);
            word          = m_editor->GetTextRange(startPos, endPos);

            if (!word.empty())
            {
                auto trimWord = word;
                CStringUtils::trim(trimWord);
                if (trimWord == word)
                {
                    std::string sAutoCompleteString;
                    {
                        std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
                        auto                                  docID      = m_main->m_tabBar.GetCurrentTabId();
                        auto                                  lang       = m_main->m_docManager.GetDocumentFromID(docID).GetLanguage();
                        const auto&                           snippetMap = m_langSnippetList[lang];
                        auto                                  foundIt    = snippetMap.find(word);
                        if (foundIt != snippetMap.end())
                        {
                            auto sVal           = SanitizeSnippetText(foundIt->second);
                            m_stringToSelect    = foundIt->first;
                            sAutoCompleteString = CStringUtils::Format("%s: %s", foundIt->first.c_str(), sVal.c_str());
                        }
                    }
                    if (!sAutoCompleteString.empty())
                    {
                        m_editor->Scintilla().AutoCSetAutoHide(FALSE);
                        m_editor->Scintilla().AutoCSetSeparator(static_cast<uptr_t>(wordSeparator));
                        m_editor->Scintilla().AutoCSetTypeSeparator(static_cast<uptr_t>(typeSeparator));
                        if (CTheme::Instance().IsDarkTheme())
                            m_editor->Scintilla().AutoCSetOptions(Scintilla::AutoCompleteOption::FixedSize);
                        m_editor->Scintilla().AutoCShow(word.size() + 1, sAutoCompleteString.c_str());
                        SetWindowStylesForAutocompletionPopup();
                    }
                    else
                        m_editor->Scintilla().AutoCCancel();
                }
                else
                    m_editor->Scintilla().AutoCCancel();
            }
        }
        else
            m_editor->Scintilla().AutoCCancel();
    }
    else if (CIniSettings::Instance().GetInt64(L"View", L"autocomplete", 1) && word.size() > 1)
    {
        std::map<std::string, AutoCompleteType> wordSet;
        {
            std::lock_guard<std::recursive_mutex> lockGuard(m_mutex);
            auto                                  docID        = m_main->m_tabBar.GetCurrentTabId();
            auto                                  docAutoList  = m_docWordList[docID];
            auto                                  lang         = m_main->m_docManager.GetDocumentFromID(docID).GetLanguage();
            auto                                  langAutoList = m_langWordList[lang];
            const auto&                           snippetMap   = m_langSnippetList[lang];
            if (docAutoList.empty() && langAutoList.empty() && snippetMap.empty() && (scn->ch != ' '))
                return;

            PrepareWordList(wordSet);

            for (const auto& list : {docAutoList, langAutoList})
            {
                for (auto lowerIt = list.lower_bound(word); lowerIt != list.end(); ++lowerIt)
                {
                    int compare = _strnicmp(word.c_str(), lowerIt->first.c_str(), word.size());
                    if (compare == 0)
                    {
                        wordSet.emplace(lowerIt->first, lowerIt->second);
                    }
                    else
                    {
                        break;
                    }
                }
            }

            for (const auto& [name, text] : snippetMap)
            {
                int compare = _strnicmp(word.c_str(), name.c_str(), word.size());
                if (compare == 0)
                {
                    auto sVal                = SanitizeSnippetText(text);
                    auto sAutoCompleteString = CStringUtils::Format("%s: %s", name.c_str(), sVal.c_str());
                    wordSet.emplace(sAutoCompleteString, AutoCompleteType::Snippet);
                    wordSet.erase(name);
                }
            }
        }
        if (wordSet.empty())
            m_editor->Scintilla().AutoCCancel();
        else
        {
            std::string sAutoCompleteList;
            for (const auto& [word2, autoCompleteType] : wordSet)
                sAutoCompleteList += CStringUtils::Format("%s%c%d%c", word2.c_str(), typeSeparator, static_cast<int>(autoCompleteType), wordSeparator);
            if (sAutoCompleteList.empty())
                return;
            if (sAutoCompleteList.size() > 1)
                sAutoCompleteList[sAutoCompleteList.size() - 1] = 0;

            m_editor->Scintilla().AutoCSetAutoHide(TRUE);
            m_editor->Scintilla().AutoCSetSeparator(static_cast<uptr_t>(wordSeparator));
            m_editor->Scintilla().AutoCSetTypeSeparator(static_cast<uptr_t>(typeSeparator));
            if (CTheme::Instance().IsDarkTheme())
                m_editor->Scintilla().AutoCSetOptions(Scintilla::AutoCompleteOption::FixedSize);
            m_editor->Scintilla().AutoCShow(word.size(), sAutoCompleteList.c_str());
            SetWindowStylesForAutocompletionPopup();
        }
    }
}

void CAutoComplete::ExitSnippetMode()
{
    MarkSnippetPositions(true);
    m_snippetPositions.clear();
    m_currentSnippetPos = -1;
    m_editor->MarkSelectedWord(true, false);
}

void CAutoComplete::MarkSnippetPositions(bool clearOnly)
{
    if (m_snippetPositions.empty())
        return;
    m_editor->Scintilla().SetIndicatorCurrent(INDIC_SNIPPETPOS);

    Sci_Position firstPos = static_cast<Sci_Position>(INT64_MAX);
    Sci_Position lastPos  = 0;

    for (const auto& [id, vec] : m_snippetPositions)
    {
        for (const auto& pos : vec)
        {
            if (pos < 0)
                continue;
            firstPos = min(pos, firstPos);
            lastPos  = max(pos, lastPos);
        }
    }
    auto docLen        = m_editor->Scintilla().Length();
    auto startClearPos = max(0, firstPos - 100);
    auto endClearPos   = min(lastPos - firstPos + 200, docLen - startClearPos);
    m_editor->Scintilla().IndicatorClearRange(startClearPos, endClearPos);
    if (clearOnly)
        return;
    for (const auto& [id, vec] : m_snippetPositions)
    {
        if (id == fullSnippetPosId)
            continue;
        if (id == 0)
            continue;
        for (auto it = vec.cbegin(); it != vec.cend(); ++it)
        {
            auto a = *it;
            ++it;
            auto b = *it;
            m_editor->Scintilla().IndicatorFillRange(a, b - a);
        }
    }
}

void CAutoComplete::PrepareWordList(std::map<std::string, AutoCompleteType>& wordList) const
{
    const bool          autoCompleteIgnoreCase = CIniSettings::Instance().GetInt64(L"View", L"autocompleteIgnorecase", 0) != 0;
    const auto          maxSearchTime          = std::chrono::milliseconds(CIniSettings::Instance().GetInt64(L"View", L"autocompleteMaxSearchTime", 2000));

    auto                lineLen                = m_editor->Scintilla().GetCurLine(0, nullptr);
    const auto          line                   = m_editor->Scintilla().GetCurLine(lineLen);
    const auto          caret                  = m_editor->Scintilla().CurrentPos();
    const auto          ln                     = m_editor->Scintilla().LineFromPosition(caret);
    const auto          lineStart              = m_editor->Scintilla().LineStart(ln);
    const auto          current                = caret - lineStart;

    Scintilla::Position startword              = current;
    // Autocompletion of pure numbers is mostly an annoyance
    bool                allNumber              = true;
    while (startword > 0 && IsWordChar(line[startword - 1]))
    {
        startword--;
        if (line[startword] < '0' || line[startword] > '9')
        {
            allNumber = false;
        }
    }
    if (startword == current || allNumber)
        return;

    const auto root           = line.substr(startword, current - startword);
    const auto rootLength     = static_cast<Scintilla::Position>(root.length());
    const auto doclen         = m_editor->Scintilla().Length();
    const auto flags          = Scintilla::FindOption::WordStart | (autoCompleteIgnoreCase ? Scintilla::FindOption::None : Scintilla::FindOption::MatchCase);
    const auto posCurrentWord = m_editor->Scintilla().CurrentPos() - rootLength;

    m_editor->Scintilla().SetTarget(Scintilla::Span(0, doclen));
    m_editor->Scintilla().SetSearchFlags(flags);
    auto          posFind = m_editor->Scintilla().SearchInTarget(root);
    SciTextReader acc(m_editor->Scintilla());
    auto          startTime = std::chrono::steady_clock::now();
    // search the whole document
    while (posFind >= 0 && posFind < doclen)
    {
        auto elapsedPeriod = std::chrono::steady_clock::now() - startTime;
        if (elapsedPeriod > maxSearchTime)
            break; // don't search for too long, some documents can be huge!
        Scintilla::Position wordEnd = posFind + rootLength;
        if (posFind != posCurrentWord)
        {
            while (IsWordChar(acc.SafeGetCharAt(wordEnd)))
                wordEnd++;
            const auto wordLength = wordEnd - posFind;
            if (wordLength > rootLength)
            {
                const auto word = m_editor->Scintilla().StringOfSpan(Scintilla::Span(posFind, wordEnd));
                wordList.emplace(word, AutoCompleteType::Word);
            }
        }
        m_editor->Scintilla().SetTarget(Scintilla::Span(wordEnd, doclen));
        posFind = m_editor->Scintilla().SearchInTarget(root);
    }
}

std::string CAutoComplete::SanitizeSnippetText(const std::string& text) const
{
    auto sVal = text;
    SearchReplace(sVal, "\n", " ");
    SearchReplace(sVal, "\b", " ");
    SearchReplace(sVal, "^0", "");
    SearchReplace(sVal, "^1", "");
    SearchReplace(sVal, "^2", "");
    SearchReplace(sVal, "^3", "");
    SearchReplace(sVal, "^4", "");
    SearchReplace(sVal, "^5", "");
    SearchReplace(sVal, "^6", "");
    SearchReplace(sVal, "^7", "");
    SearchReplace(sVal, "^8", "");
    SearchReplace(sVal, "^9", "");
    SearchReplace(sVal, "^", "");

    constexpr size_t maxLen = 40;
    if (sVal.size() >= maxLen)
        sVal = sVal.substr(0, maxLen - 3) + "...";

    return sVal;
}

bool CAutoComplete::IsWordChar(int ch)
{
    if (isalnum(ch))
        return true;
    if (ch == '_')
        return true;
    return false;
}

void CAutoComplete::SetWindowStylesForAutocompletionPopup()
{
    if (CTheme::Instance().IsDarkTheme())
    {
        EnumThreadWindows(GetCurrentThreadId(), AdjustThemeProc, 0);
    }
}

BOOL CAutoComplete::AdjustThemeProc(HWND hwnd, LPARAM /*lParam*/)
{
    wchar_t szWndClassName[MAX_PATH] = {0};
    GetClassName(hwnd, szWndClassName, _countof(szWndClassName));
    if ((wcscmp(szWndClassName, L"ListBoxX") == 0) ||
        (wcscmp(szWndClassName, WC_LISTBOX) == 0))
    {
        DarkModeHelper::Instance().AllowDarkModeForWindow(hwnd, TRUE);
        SetWindowTheme(hwnd, L"Explorer", nullptr);
        EnumChildWindows(hwnd, AdjustThemeProc, 0);
    }

    return TRUE;
}

CAutoCompleteConfigDlg::CAutoCompleteConfigDlg(CMainWindow* main)
    : m_main(main)
    , m_scintilla(g_hInst)
{
}

LRESULT CAutoCompleteConfigDlg::DlgFunc(HWND /*hwndDlg*/, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_NCDESTROY:
            break;
        case WM_INITDIALOG:
        {
            InitDialog(*this, IDI_BOWPAD);
            m_resizer.Init(*this);
            m_scintilla.Init(g_hRes, *this, GetDlgItem(*this, IDC_SCINTILLA));
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(*this, IDC_LABEL1, RESIZER_TOPLEFT);
            m_resizer.AddControl(*this, IDC_LANGCOMBO, RESIZER_TOPLEFT);
            m_resizer.AddControl(*this, IDC_SNIPPETGROUP, RESIZER_TOPLEFTBOTTOMLEFT);
            m_resizer.AddControl(*this, IDC_SNIPPETLIST, RESIZER_TOPLEFTBOTTOMLEFT);
            m_resizer.AddControl(*this, IDC_DELETE, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(*this, IDC_SCINTILLA, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(*this, IDC_LABEL2, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(*this, IDC_SNIPPETNAME, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(*this, IDC_LABEL3, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(*this, IDC_SAVE, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(*this, IDCANCEL, RESIZER_BOTTOMRIGHT);
            m_resizer.UseSizeGrip(true);

            m_scintilla.SetupLexerForLang("Snippets");
            m_scintilla.Scintilla().SetEOLMode(Scintilla::EndOfLine::Lf);
            m_scintilla.Scintilla().SetUseTabs(true);

            DialogEnableWindow(IDC_DELETE, false);

            auto languages  = CLexStyles::Instance().GetLanguages();
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            for (const auto& langName : languages)
                ComboBox_AddString(hLangCombo, langName.c_str());

            // Select the current language.
            auto id = m_main->m_tabBar.GetCurrentTabId();
            if (m_main->m_docManager.HasDocumentID(id))
            {
                const auto& doc = m_main->m_docManager.GetDocumentFromID(m_main->m_tabBar.GetCurrentTabId());
                ComboBox_SelectString(hLangCombo, -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
            }

            DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);
        }
            return FALSE;

        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_SIZE:
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_NOTIFY:
        {
            LPNMHDR pnmHdr = reinterpret_cast<LPNMHDR>(lParam);

            if (pnmHdr->idFrom == reinterpret_cast<UINT_PTR>(&m_scintilla) || pnmHdr->hwndFrom == m_scintilla)
            {
                if (pnmHdr->code == NM_COOLSB_CUSTOMDRAW)
                    return m_scintilla.HandleScrollbarCustomDraw(wParam, reinterpret_cast<NMCSBCUSTOMDRAW*>(lParam));
            }
        }
        break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CAutoCompleteConfigDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
        case IDC_SAVE:
        {
            auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
            int  langSel    = ComboBox_GetCurSel(hLangCombo);
            auto languages  = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                auto snippetName = GetDlgItemText(IDC_SNIPPETNAME);
                if (snippetName && snippetName[0])
                {
                    auto        lineCount = m_scintilla.Scintilla().LineCount();
                    std::string snippet;
                    for (sptr_t lineNumber = 0; lineNumber < lineCount; ++lineNumber)
                    {
                        auto line = m_scintilla.GetLine(lineNumber);
                        CStringUtils::trim(line, "\r\n");
                        if (!snippet.empty())
                            snippet += "\\n";

                        for (const auto& c : line)
                        {
                            if (c == '\t')
                                snippet += "\\t";
                            else if (c != '\r' && c != '\n')
                                snippet += c;
                        }
                    }
                    auto       iniPath = CAppUtils::GetDataPath() + L"\\autocomplete.ini";
                    CSimpleIni iniFile;
                    iniFile.SetUnicode();
                    iniFile.LoadFile(iniPath.c_str());
                    auto sKey = std::wstring(L"snippet_");
                    sKey += snippetName.get();
                    iniFile.SetValue(CUnicodeUtils::StdGetUnicode(currentLang).c_str(), sKey.c_str(), CUnicodeUtils::StdGetUnicode(snippet).c_str());
                    FILE* pFile = nullptr;
                    _wfopen_s(&pFile, iniPath.c_str(), L"wb");
                    if (pFile)
                    {
                        iniFile.SaveFile(pFile);
                        fclose(pFile);
                    }
                    m_main->m_autoCompleter.Init();
                    DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);
                    auto hSnippetList = GetDlgItem(*this, IDC_SNIPPETLIST);
                    ListBox_SelectString(hSnippetList, 0, snippetName.get());
                }
            }
        }
        break;
        case IDC_DELETE:
        {
            auto hLangCombo   = GetDlgItem(*this, IDC_LANGCOMBO);
            auto hSnippetList = GetDlgItem(*this, IDC_SNIPPETLIST);
            int  langSel      = ComboBox_GetCurSel(hLangCombo);
            auto languages    = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                auto snippetSel  = ListBox_GetCurSel(hSnippetList);
                if (snippetSel != CB_ERR)
                {
                    auto nameLen = ListBox_GetTextLen(hSnippetList, snippetSel);
                    auto nameBuf = std::make_unique<wchar_t[]>(nameLen + 1LL);
                    ListBox_GetText(hSnippetList, snippetSel, nameBuf.get());

                    if (nameBuf[0])
                    {
                        auto       iniPath = CAppUtils::GetDataPath() + L"\\autocomplete.ini";
                        CSimpleIni iniFile;
                        iniFile.SetUnicode();
                        iniFile.LoadFile(iniPath.c_str());
                        auto sKey = std::wstring(L"snippet_");
                        sKey += nameBuf.get();
                        std::wstring existingValue = iniFile.GetValue(CUnicodeUtils::StdGetUnicode(currentLang).c_str(), sKey.c_str(), L"");
                        if (!existingValue.empty())
                            iniFile.Delete(CUnicodeUtils::StdGetUnicode(currentLang).c_str(), sKey.c_str(), true);
                        else
                            iniFile.SetValue(CUnicodeUtils::StdGetUnicode(currentLang).c_str(), sKey.c_str(), L"");
                        FILE* pFile = nullptr;
                        _wfopen_s(&pFile, iniPath.c_str(), L"wb");
                        if (pFile)
                        {
                            iniFile.SaveFile(pFile);
                            fclose(pFile);
                        }
                        m_main->m_autoCompleter.Init();
                        DoCommand(IDC_LANGCOMBO, CBN_SELCHANGE);
                    }
                }
            }
        }
        break;
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        case IDC_LANGCOMBO:
        {
            if (msg == CBN_SELCHANGE)
            {
                auto hLangCombo   = GetDlgItem(*this, IDC_LANGCOMBO);
                int  langSel      = ComboBox_GetCurSel(hLangCombo);
                auto hSnippetList = GetDlgItem(*this, IDC_SNIPPETLIST);
                ListBox_ResetContent(hSnippetList);
                auto languages = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                {
                    auto currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                    auto snippets    = m_main->m_autoCompleter.m_langSnippetList[currentLang];
                    for (const auto& [name, snippet] : snippets)
                    {
                        ListBox_AddString(hSnippetList, CUnicodeUtils::StdGetUnicode(name).c_str());
                    }
                }
            }
        }
        break;
        case IDC_SNIPPETLIST:
        {
            if (msg == LBN_SELCHANGE)
            {
                SetDlgItemText(*this, IDC_SNIPPETNAME, L"");
                m_scintilla.Scintilla().ClearAll();

                auto hSnippetList = GetDlgItem(*this, IDC_SNIPPETLIST);
                auto curSel       = ListBox_GetCurSel(hSnippetList);
                if (curSel != LB_ERR)
                {
                    DialogEnableWindow(IDC_DELETE, true);
                    auto nameLen = ListBox_GetTextLen(hSnippetList, curSel);
                    auto nameBuf = std::make_unique<wchar_t[]>(nameLen + 1LL);
                    ListBox_GetText(hSnippetList, curSel, nameBuf.get());
                    if (nameBuf[0])
                    {
                        auto autoBrace = CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1);
                        CIniSettings::Instance().SetInt64(L"View", L"autobrace", 0);
                        OnOutOfScope(CIniSettings::Instance().SetInt64(L"View", L"autobrace", autoBrace));

                        auto hLangCombo = GetDlgItem(*this, IDC_LANGCOMBO);
                        int  langSel    = ComboBox_GetCurSel(hLangCombo);
                        auto languages  = CLexStyles::Instance().GetLanguages();
                        if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                        {
                            auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                            auto        snippets    = m_main->m_autoCompleter.m_langSnippetList[currentLang];
                            const auto& snippet     = snippets[CUnicodeUtils::StdGetUTF8(nameBuf.get())];
                            SetDlgItemText(*this, IDC_SNIPPETNAME, nameBuf.get());
                            for (const auto& c : snippet)
                            {
                                if (c == '\t')
                                {
                                    m_scintilla.Scintilla().Tab();
                                }
                                else if (c == '\n')
                                {
                                    m_scintilla.Scintilla().NewLine();
                                }
                                else if (c == '\b')
                                {
                                    m_scintilla.Scintilla().DeleteBack();
                                }
                                else
                                {
                                    char text[] = {c, 0};
                                    m_scintilla.Scintilla().ReplaceSel(text);
                                }
                            }
                        }
                    }
                }
                else
                    DialogEnableWindow(IDC_DELETE, false);
            }
        }
        break;
        default:
            break;
    }
    return 1;
}
