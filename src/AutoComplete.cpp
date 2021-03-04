// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "BowPad.h"
#include "AppUtils.h"
#include "MainWindow.h"
#include "ScintillaWnd.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "DirFileEnum.h"
#include "SmartHandle.h"
#include "OnOutOfScope.h"
#include "SciLexer.h"

#include <chrono>

static HICON LoadIconEx(HINSTANCE hInstance, LPCWSTR lpIconName, int iconWidth, int iconHeight)
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

    auto   ptrb          = std::make_unique<BYTE[]>(size * 2 + height * width * 4);
    LPBYTE pixelsIconRGB = ptrb.get();
    LPBYTE alphaPixels   = pixelsIconRGB + size;
    HDC    hDC           = CreateCompatibleDC(nullptr);
    OnOutOfScope(DeleteDC(hDC));
    HBITMAP hBmpOld = static_cast<HBITMAP>(SelectObject(hDC, static_cast<HGDIOBJ>(iconInfo.hbmColor)));
    if (!GetDIBits(hDC, iconInfo.hbmColor, 0, height, static_cast<LPVOID>(pixelsIconRGB), &infoHeader, DIB_RGB_COLORS))
        return nullptr;

    SelectObject(hDC, hBmpOld);
    if (!GetDIBits(hDC, iconInfo.hbmMask, 0, height, static_cast<LPVOID>(alphaPixels), &infoHeader, DIB_RGB_COLORS))
        return nullptr;

    auto imagePixels = std::make_unique<UINT[]>(height * width);
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
    int iconWidth  = GetSystemMetrics(SM_CXSMICON);
    int iconHeight = GetSystemMetrics(SM_CYSMICON);
    m_editor->Call(SCI_RGBAIMAGESETWIDTH, iconWidth);
    m_editor->Call(SCI_RGBAIMAGESETHEIGHT, iconHeight);
    m_editor->Call(SCI_AUTOCSETIGNORECASE, TRUE);
    m_editor->Call(SCI_AUTOCSETFILLUPS, 0, reinterpret_cast<sptr_t>("\t(["));
    int i = 0;
    for (auto icon : {IDI_SCI_CODE, IDI_SCI_FILE})
    {
        CAutoIcon hIcon = LoadIconEx(g_hInst, MAKEINTRESOURCE(icon), iconWidth, iconHeight);
        auto      bytes = Icon2Image(hIcon);
        m_editor->Call(SCI_REGISTERRGBAIMAGE, i, reinterpret_cast<sptr_t>(bytes.get()));
        ++i;
    }

    std::vector<std::unique_ptr<CSimpleIni>> iniFiles;

    auto iniPath = CAppUtils::GetDataPath() + L"\\autocomplete.ini";
    iniFiles.push_back(std::make_unique<CSimpleIni>());
    iniFiles.back()->LoadFile(iniPath.c_str());
    DWORD       resLen  = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_AUTOCOMPLETE, resLen);
    if (resData != nullptr && resLen)
    {
        iniFiles.push_back(std::make_unique<CSimpleIni>());
        iniFiles.back()->LoadFile(resData, resLen);
    }
    for (const auto& ini : iniFiles)
    {
        std::list<const wchar_t*> sections;
        ini->GetAllSections(sections);
        for (const auto& section : sections)
        {
            {
                std::map<std::string, AutoCompleteType, ci_less> acMap;
                const auto*                                      codeVal = ini->GetValue(section, L"code");
                if (codeVal)
                {
                    std::vector<std::wstring> values;
                    stringtok(values, codeVal, true, L" ");
                    for (const auto& v : values)
                        acMap[CUnicodeUtils::StdGetUTF8(v)] = AutoCompleteType::Code;
                }
                std::lock_guard<std::mutex> lockGuard(m_mutex);
                m_langWordList[CUnicodeUtils::StdGetUTF8(section)] = std::move(acMap);
            }

            {
                std::list<const wchar_t*> keys;
                ini->GetAllKeys(section, keys);
                auto csMap = m_langSnippetList[CUnicodeUtils::StdGetUTF8(section)];

                for (const auto& key : keys)
                {
                    if (_wcsnicmp(key, L"snippet_", 8) == 0)
                    {
                        const auto* snippetVal = ini->GetValue(section, key);
                        if (snippetVal)
                        {
                            auto snipp       = CUnicodeUtils::StdGetUTF8(key);
                            auto sSnippetVal = CUnicodeUtils::StdGetUTF8(snippetVal);
                            SearchReplace(sSnippetVal, "\\r\\n", "\n");
                            SearchReplace(sSnippetVal, "\\n", "\n");
                            SearchReplace(sSnippetVal, "\\t", "\t");
                            SearchReplace(sSnippetVal, "\\b", "\b");
                            csMap[snipp.substr(8)] = sSnippetVal;
                        }
                    }
                }

                std::lock_guard<std::mutex> lockGuard(m_mutex);
                m_langSnippetList[CUnicodeUtils::StdGetUTF8(section)] = std::move(csMap);
            }
        }
    }
}

void CAutoComplete::HandleScintillaEvents(const SCNotification* scn)
{
    switch (scn->nmhdr.code)
    {
        case SCN_UPDATEUI:
            break;
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
                        m_editor->Call(SCI_AUTOCCANCEL);
                        auto        sKey = sText.substr(0, colonPos);
                        std::string sSnippet;
                        {
                            std::lock_guard<std::mutex> lockGuard(m_mutex);
                            auto                        docID      = m_main->m_tabBar.GetCurrentTabId();
                            auto                        lang       = m_main->m_docManager.GetDocumentFromID(docID).GetLanguage();
                            const auto&                 snippetMap = m_langSnippetList[lang];
                            auto                        foundIt    = snippetMap.find(sKey);
                            if (foundIt != snippetMap.end())
                            {
                                sSnippet = foundIt->second;
                            }
                        }
                        if (!sSnippet.empty())
                        {
                            auto autoBrace = CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1);
                            CIniSettings::Instance().SetInt64(L"View", L"autobrace", 0);
                            OnOutOfScope(CIniSettings::Instance().SetInt64(L"View", L"autobrace", autoBrace));
                            auto pos = m_editor->Call(SCI_GETCURRENTPOS);
                            m_editor->Call(SCI_BEGINUNDOACTION);
                            m_editor->Call(SCI_SETSELECTION, scn->position, pos);
                            m_editor->Call(SCI_DELETERANGE, scn->position, pos - scn->position);

                            sptr_t cursorPos  = -1;
                            char   lastC      = 0;
                            char   last2C     = 0;
                            int    hotSpotNum = -1;
                            ExitSnippetMode();
                            for (const auto& c : sSnippet)
                            {
                                if (c == '^' && lastC != '\\')
                                {
                                    cursorPos = m_editor->Call(SCI_GETCURRENTPOS);
                                    if (hotSpotNum >= 0)
                                        m_snippetPositions[hotSpotNum].push_back(cursorPos);
                                    hotSpotNum = -1;
                                }
                                else if (lastC == '^' && last2C != '\\' && isdigit(c))
                                {
                                    hotSpotNum = c - '0';
                                    m_snippetPositions[c - '0'].push_back(cursorPos);
                                }
                                else
                                {
                                    if (lastC == '\\')
                                    {
                                        char text[] = {c, 0};
                                        m_editor->Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(text));
                                    }
                                    else if (c == '\t')
                                    {
                                        m_editor->Call(SCI_TAB);
                                    }
                                    else if (c == '\n')
                                    {
                                        m_editor->Call(SCI_NEWLINE);
                                        m_main->IndentToLastLine(true);
                                    }
                                    else if (c == '\b')
                                    {
                                        m_editor->Call(SCI_DELETEBACK);
                                    }
                                    else if (c != '\\')
                                    {
                                        char text[] = {c, 0};
                                        m_editor->Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(text));
                                    }
                                }
                                last2C = lastC;
                                lastC  = c;
                            }
                            if (!m_snippetPositions.empty())
                            {
                                const auto& [id, posVec] = *m_snippetPositions.begin();
                                bool first               = true;
                                for (auto it = posVec.cbegin(); it != posVec.cend(); ++it)
                                {
                                    auto a = *it;
                                    ++it;
                                    auto b = *it;
                                    if (first)
                                    {
                                        m_editor->Call(SCI_SETSELECTION, a, b);
                                        first = false;
                                    }
                                    else
                                    {
                                        m_editor->Call(SCI_ADDSELECTION, a, b);
                                    }
                                }
                                m_currentSnippetPos = id;
                            }
                            m_editor->Call(SCI_ENDUNDOACTION);
                            MarkSnippetPositions(false);
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
                    m_editor->Call(SCI_AUTOCSELECT, 0, reinterpret_cast<sptr_t>(m_stringToSelect.c_str()));
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
                        for (auto& [id, snippetPositions] : m_snippetPositions)
                        {
                            bool odd = true;
                            for (auto& pos : snippetPositions)
                            {
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
                            }
                        }
                        MarkSnippetPositions(false);
                    }
                }
            }
        }
        break;
    }
}

bool CAutoComplete::HandleChar(WPARAM wParam, LPARAM /*lParam*/)
{
    if (m_snippetPositions.empty())
        return true;
    if (wParam == VK_TAB || wParam == VK_RETURN)
    {
        if (GetKeyState(VK_SHIFT) & 0x8000)
            --m_currentSnippetPos;
        else
            ++m_currentSnippetPos;
        if (m_currentSnippetPos < 0)
        {
            ExitSnippetMode();
            return true;
        }
        if (m_currentSnippetPos < m_snippetPositions.size())
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
                    m_editor->Call(SCI_SETSELECTION, a, b);
                    first = false;
                }
                else
                {
                    m_editor->Call(SCI_ADDSELECTION, a, b);
                }
            }
            return false;
        }
    }
    return true;
}

void CAutoComplete::AddWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_langWordList[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_langWordList[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_docWordList[docID].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_docWordList[docID].insert(words.begin(), words.end());
}

void CAutoComplete::HandleAutoComplete(const SCNotification* scn)
{
    static constexpr auto wordSeparator = '\n';
    static constexpr auto typeSeparator = '?';

    if (m_insertingSnippet)
        return;
    auto pos = m_editor->Call(SCI_GETCURRENTPOS);
    if (pos != m_editor->Call(SCI_WORDENDPOSITION, pos, TRUE))
        return; // don't auto complete if we're not at the end of a word
    auto word = m_editor->GetCurrentWord();

    // path completion: get the current line
    auto currentLine = CUnicodeUtils::StdGetUnicode(m_editor->GetCurrentLine());
    CStringUtils::rtrim(currentLine, L"\r\n");
    std::wstring pathToMatch, rawPath;
    if (getPathsForPathCompletion(currentLine, rawPath, pathToMatch))
    {
        if (m_editor->Call(SCI_AUTOCACTIVE))
            return;

        auto lineStartPos = m_editor->Call(SCI_POSITIONFROMLINE, m_editor->Call(SCI_LINEFROMPOSITION, pos));
        if (currentLine.find(rawPath) == (pos - lineStartPos - CUnicodeUtils::StdGetUTF8(rawPath).size()))
        {
            std::string pathComplete;
            if (pathToMatch.ends_with(':'))
                pathToMatch += '\\';
            SearchReplace(pathToMatch, L"/", L"\\");
            CDirFileEnum fileFinder(pathToMatch);
            bool         bIsDirectory;
            std::wstring filename;

            auto           startTime   = std::chrono::steady_clock::now();
            constexpr auto maxPathTime = std::chrono::milliseconds(400);
            auto           rootDir     = rawPath.substr(0, rawPath.find_last_of(L"\\/") + 1);
            while (fileFinder.NextFile(filename, &bIsDirectory, false))
            {
                filename = rootDir + filename.substr(rootDir.size());
                pathComplete += (CUnicodeUtils::StdGetUTF8(filename) + typeSeparator + std::to_string(static_cast<int>(AutoCompleteType::Path)) + wordSeparator);
                auto elapsedPeriod = std::chrono::steady_clock::now() - startTime;
                if (elapsedPeriod > maxPathTime)
                    break;
            }
            if (!pathComplete.empty())
            {
                m_editor->Call(SCI_AUTOCSETAUTOHIDE, TRUE);
                m_editor->Call(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(wordSeparator));
                m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(typeSeparator));
                m_editor->Call(SCI_AUTOCSHOW, CUnicodeUtils::StdGetUTF8(rawPath).size(), reinterpret_cast<LPARAM>(pathComplete.c_str()));
                return;
            }
        }
    }

    if (scn->ch == '/')
    {
        auto lexer = m_editor->Call(SCI_GETLEXER);
        switch (lexer)
        {
            // add the closing tag only for xml and html lexers
            case SCLEX_XML:
            case SCLEX_HTML:
            {
                auto pos2  = pos - 2;
                auto inner = 1;
                if (m_editor->Call(SCI_GETCHARAT, pos2) == '<')
                {
                    do
                    {
                        pos2 = m_editor->FindText("<", pos2 - 2, 0);
                        if (pos2)
                        {
                            if (m_editor->Call(SCI_GETCHARAT, pos2 + 1) == '/')
                                ++inner;
                            else
                                --inner;
                        }
                    } while (inner && pos2 > 0);
                    std::string tagName;
                    auto        position = pos2 + 1;
                    int         nextChar = static_cast<int>(m_editor->Call(SCI_GETCHARAT, position));
                    while (position < pos && !m_editor->IsXMLWhitespace(nextChar) && nextChar != '/' && nextChar != '>' && nextChar != '\"' && nextChar != '\'')
                    {
                        tagName.push_back(static_cast<char>(nextChar));
                        ++position;
                        nextChar = static_cast<int>(m_editor->Call(SCI_GETCHARAT, position));
                    }
                    if (!tagName.empty())
                    {
                        tagName = tagName + ">" + typeSeparator + "-1";
                        m_editor->Call(SCI_AUTOCSETAUTOHIDE, TRUE);
                        m_editor->Call(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(wordSeparator));
                        m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(typeSeparator));
                        m_editor->Call(SCI_AUTOCSHOW, CUnicodeUtils::StdGetUTF8(rawPath).size(), reinterpret_cast<LPARAM>(tagName.c_str()));
                        return;
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
        if (m_editor->Call(SCI_GETCHARAT, pos - 1) == ' ')
        {
            auto startPos = m_editor->Call(SCI_WORDSTARTPOSITION, pos - 2, true);
            auto endPos   = m_editor->Call(SCI_WORDENDPOSITION, pos - 2, true);
            word          = m_editor->GetTextRange(startPos, endPos);

            if (!word.empty())
            {
                std::string sAutoCompleteString;
                {
                    std::lock_guard<std::mutex> lockGuard(m_mutex);
                    auto                        docID      = m_main->m_tabBar.GetCurrentTabId();
                    auto                        lang       = m_main->m_docManager.GetDocumentFromID(docID).GetLanguage();
                    const auto&                 snippetMap = m_langSnippetList[lang];
                    auto                        foundIt    = snippetMap.find(word);
                    if (foundIt != snippetMap.end())
                    {
                        auto sVal        = foundIt->second;
                        m_stringToSelect = foundIt->first;
                        SearchReplace(sVal, "\n", " ");
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
                        sAutoCompleteString = CStringUtils::Format("%s: %s", foundIt->first.c_str(), sVal.c_str());
                    }
                }
                if (!sAutoCompleteString.empty())
                {
                    m_editor->Call(SCI_AUTOCSETAUTOHIDE, FALSE);
                    m_editor->Call(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(wordSeparator));
                    m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(typeSeparator));
                    m_editor->Call(SCI_AUTOCSHOW, word.size() + 1, reinterpret_cast<sptr_t>(sAutoCompleteString.c_str()));
                }
            }
        }
    }
    else if (CIniSettings::Instance().GetInt64(L"View", L"autocomplete", 1))
    {
        if (m_editor->Call(SCI_AUTOCACTIVE))
            return;

        std::map<std::string, AutoCompleteType> wordset;
        {
            std::lock_guard<std::mutex> lockGuard(m_mutex);
            auto                        docID        = m_main->m_tabBar.GetCurrentTabId();
            auto                        docAutoList  = m_docWordList[docID];
            auto                        langAutoList = m_langWordList[m_main->m_docManager.GetDocumentFromID(docID).GetLanguage()];
            if (docAutoList.empty() && langAutoList.empty())
                return;

            for (const auto& list : {docAutoList, langAutoList})
            {
                for (auto lowerIt = list.lower_bound(word); lowerIt != list.end(); ++lowerIt)
                {
                    int compare = _strnicmp(word.c_str(), lowerIt->first.c_str(), word.size());
                    if (compare == 0)
                    {
                        wordset.emplace(lowerIt->first, lowerIt->second);
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        std::string sAutoCompleteList;
        for (const auto& [word2, autoCompleteType] : wordset)
            sAutoCompleteList += CStringUtils::Format("%s%c%d%c", word2.c_str(), typeSeparator, static_cast<int>(autoCompleteType), wordSeparator);
        if (sAutoCompleteList.empty())
            return;
        if (sAutoCompleteList.size() > 1)
            sAutoCompleteList[sAutoCompleteList.size() - 1] = 0;

        m_editor->Call(SCI_AUTOCSETAUTOHIDE, TRUE);
        m_editor->Call(SCI_AUTOCSETSEPARATOR, static_cast<uptr_t>(wordSeparator));
        m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, static_cast<uptr_t>(typeSeparator));
        m_editor->Call(SCI_AUTOCSHOW, word.size(), reinterpret_cast<sptr_t>(sAutoCompleteList.c_str()));
    }
}

void CAutoComplete::ExitSnippetMode()
{
    MarkSnippetPositions(true);
    m_snippetPositions.clear();
    m_currentSnippetPos = -1;
}

void CAutoComplete::MarkSnippetPositions(bool clearOnly)
{
    m_editor->Call(SCI_SETINDICATORCURRENT, INDIC_SNIPPETPOS);

    Sci_Position firstPos = INT64_MAX;
    Sci_Position lastPos  = 0;

    for (const auto& [id, vec] : m_snippetPositions)
    {
        for (const auto& pos : vec)
        {
            firstPos = min(pos, firstPos);
            lastPos  = max(pos, lastPos);
        }
    }
    m_editor->Call(SCI_INDICATORCLEARRANGE, max(0, firstPos - 100), lastPos - firstPos + 200);
    if (clearOnly)
        return;
    for (const auto& [id, vec] : m_snippetPositions)
    {
        for (auto it = vec.cbegin(); it != vec.cend(); ++it)
        {
            auto a = *it;
            ++it;
            auto b = *it;
            m_editor->Call(SCI_INDICATORFILLRANGE, a, b - a);
        }
    }
}
