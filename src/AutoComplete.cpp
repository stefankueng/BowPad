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
#include "MainWindow.h"
#include "ScintillaWnd.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "PathUtils.h"
#include "DirFileEnum.h"
#include "SmartHandle.h"
#include "OnOutOfScope.h"

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
        hIcon = (HICON)LoadImage(hInstance, lpIconName, IMAGE_ICON, iconWidth, iconHeight, LR_DEFAULTCOLOR);
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
    BITMAPINFO infoheader;
    infoheader.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    infoheader.bmiHeader.biWidth       = width;
    infoheader.bmiHeader.biHeight      = height;
    infoheader.bmiHeader.biPlanes      = 1;
    infoheader.bmiHeader.biBitCount    = 24;
    infoheader.bmiHeader.biCompression = BI_RGB;
    infoheader.bmiHeader.biSizeImage   = size;

    auto   ptrb          = std::make_unique<BYTE[]>(size * 2 + height * width * 4);
    LPBYTE pixelsIconRGB = ptrb.get();
    LPBYTE alphaPixels   = pixelsIconRGB + size;
    HDC    hDC           = CreateCompatibleDC(nullptr);
    OnOutOfScope(DeleteDC(hDC));
    HBITMAP hBmpOld = (HBITMAP)SelectObject(hDC, (HGDIOBJ)iconInfo.hbmColor);
    if (!GetDIBits(hDC, iconInfo.hbmColor, 0, height, (LPVOID)pixelsIconRGB, &infoheader, DIB_RGB_COLORS))
        return nullptr;

    SelectObject(hDC, hBmpOld);
    if (!GetDIBits(hDC, iconInfo.hbmMask, 0, height, (LPVOID)alphaPixels, &infoheader, DIB_RGB_COLORS))
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
            imagePixels[currentDestPos] = (((UINT)(
                                               (
                                                   ((pixelsIconRGB[currentSrcPos + 2] /*Red*/) | (pixelsIconRGB[currentSrcPos + 1] << 8 /*Green*/)) | pixelsIconRGB[currentSrcPos] << 16 /*Blue*/
                                                   ) |
                                               ((alphaPixels[currentSrcPos] ? 0 : 0xff) << 24))) &
                                           0xffffffff);
        }
    }
    return imagePixels;
}

static bool isAllowedBeforeDriveLetter(wchar_t c)
{
    return c == '\'' || c == '/' || c == '"' || c == '(' || c == '{' || c == '[' || std::isspace(c);
}

static bool getRawPath(const std::wstring& input, std::wstring& rawPath_out)
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

    rawPath_out = input.substr(lastOccurrence - 1);
    return true;
}

static bool getPathsForPathCompletion(const std::wstring& input, std::wstring& pathRaw, std::wstring& pathToMatch_out)
{
    std::wstring rawPath;
    if (!getRawPath(input, rawPath))
    {
        return false;
    }
    else if (PathIsDirectory(rawPath.c_str()))
    {
        pathToMatch_out = rawPath;
        pathRaw         = rawPath;
        return true;
    }
    else
    {
        auto last_occurrence = rawPath.find_last_of(L"\\/");
        if (last_occurrence == std::string::npos) // No match.
            return false;
        else
        {
            pathToMatch_out = rawPath.substr(0, last_occurrence);
            pathRaw         = rawPath;
            return true;
        }
    }
}

CAutoComplete::CAutoComplete(CMainWindow* main, CScintillaWnd* scintilla)
    : m_main(main)
    , m_editor(scintilla)
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
    m_editor->Call(SCI_AUTOCSETFILLUPS, 0, (LPARAM) "\t([");
    int i = 0;
    for (auto icon : {IDI_SCI_CODE, IDI_SCI_FILE})
    {
        CAutoIcon hIcon = LoadIconEx(g_hInst, MAKEINTRESOURCE(icon), iconWidth, iconHeight);
        auto      bytes = Icon2Image(hIcon);
        m_editor->Call(SCI_REGISTERRGBAIMAGE, i, (LPARAM)bytes.get());
        ++i;
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
    }
}

void CAutoComplete::AddWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_langWordlist[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_langWordlist[lang].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_docWordlist[docID].insert(words.begin(), words.end());
}

void CAutoComplete::AddWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words)
{
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    m_docWordlist[docID].insert(words.begin(), words.end());
}

void CAutoComplete::HandleAutoComplete(const SCNotification* /*scn*/)
{
    static constexpr auto wordSeparator = '\n';
    static constexpr auto typeSeparator = '?';

    if (m_editor->Call(SCI_AUTOCACTIVE))
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
        auto lineStartPos = m_editor->Call(SCI_POSITIONFROMLINE, m_editor->Call(SCI_LINEFROMPOSITION, pos));
        if (currentLine.find(rawPath) == (pos - lineStartPos - CUnicodeUtils::StdGetUTF8(rawPath).size()))
        {
            std::string pathComplete;
            if (pathToMatch.ends_with(':'))
                pathToMatch += '\\';
            SearchReplace(pathToMatch, L"/", L"\\");
            CDirFileEnum filefinder(pathToMatch);
            bool         bIsDirectory;
            std::wstring filename;

            auto           startTime   = std::chrono::steady_clock::now();
            constexpr auto maxPathTime = std::chrono::milliseconds(400);
            auto           rootDir     = rawPath.substr(0, rawPath.find_last_of(L"\\/") + 1);
            while (filefinder.NextFile(filename, &bIsDirectory, false))
            {
                filename = rootDir + filename.substr(rootDir.size());
                pathComplete += (CUnicodeUtils::StdGetUTF8(filename) + typeSeparator + std::to_string(static_cast<int>(AutoCompleteType::Path)) + wordSeparator);
                auto ellapsedPeriod = std::chrono::steady_clock::now() - startTime;
                if (ellapsedPeriod > maxPathTime)
                    break;
            }
            if (!pathComplete.empty())
            {
                m_editor->Call(SCI_AUTOCSETSEPARATOR, (WPARAM)wordSeparator);
                m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, (WPARAM)typeSeparator);
                m_editor->Call(SCI_AUTOCSHOW, CUnicodeUtils::StdGetUTF8(rawPath).size(), (LPARAM)pathComplete.c_str());
                return;
            }
        }
    }

    std::map<std::string, AutoCompleteType> wordset;
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);
        auto                        docID        = m_main->m_TabBar.GetCurrentTabId();
        auto                        docAutoList  = m_docWordlist[docID];
        auto                        langAutoList = m_langWordlist[m_main->m_DocManager.GetDocumentFromID(docID).GetLanguage()];
        if (docAutoList.empty() && langAutoList.empty())
            return;

        for (const auto& list : {docAutoList, langAutoList})
        {
            for (auto lowerit = list.lower_bound(word); lowerit != list.end(); ++lowerit)
            {
                int compare = _strnicmp(word.c_str(), lowerit->first.c_str(), word.size());
                if (compare > 0)
                    continue;
                else if (compare == 0)
                {
                    wordset.emplace(lowerit->first, lowerit->second);
                }
                else
                {
                    break;
                }
            }
        }
    }

    std::string sAutoCompleteList;
    for (const auto& w : wordset)
        sAutoCompleteList += CStringUtils::Format("%s%c%d%c", w.first.c_str(), typeSeparator, static_cast<int>(w.second), wordSeparator);
    if (sAutoCompleteList.empty())
        return;
    if (sAutoCompleteList.size() > 1)
        sAutoCompleteList[sAutoCompleteList.size() - 1] = 0;

    m_editor->Call(SCI_AUTOCSETSEPARATOR, (WPARAM)wordSeparator);
    m_editor->Call(SCI_AUTOCSETTYPESEPARATOR, (WPARAM)typeSeparator);
    m_editor->Call(SCI_AUTOCSHOW, word.size(), (LPARAM)sAutoCompleteList.c_str());
}
