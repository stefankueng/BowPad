// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "BowPad.h"
#include "DocumentManager.h"
#include "SmartHandle.h"
#include "UnicodeUtils.h"
#include "SysInfo.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "TempFile.h"
#include "AppUtils.h"
#include "OnOutOfScope.h"
#include "ILoader.h"
#include "ResString.h"

#include <stdexcept>
#include <Shobjidl.h>

namespace
{
CDocument g_emptyDoc;

wchar_t WideCharSwap(wchar_t nValue)
{
    return (((nValue >> 8)) | (nValue << 8));
}

UINT64 WordSwapBytes(UINT64 nValue)
{
    return ((nValue & 0xff00ff00ff00ff) << 8) | ((nValue >> 8) & 0xff00ff00ff00ff); // swap BYTESs in WORDs
}

UINT32 DwordSwapBytes(UINT32 nValue)
{
    UINT32 nRet = (nValue << 16) | (nValue >> 16);                     // swap WORDs
    nRet        = ((nRet & 0xff00ff) << 8) | ((nRet >> 8) & 0xff00ff); // swap BYTESs in WORDs
    return nRet;
}

UINT64 DwordSwapBytes(UINT64 nValue)
{
    UINT64 nRet = ((nValue & 0xffff0000ffffL) << 16) | ((nValue >> 16) & 0xffff0000ffffL); // swap WORDs in DWORDs
    nRet        = ((nRet & 0xff00ff00ff00ff) << 8) | ((nRet >> 8) & 0xff00ff00ff00ff);     // swap BYTESs in WORDs
    return nRet;
}

EOLFormat SenseEOLFormat(const char* data, DWORD len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] == '\r')
        {
            if (i + 1 < len && data[i + 1] == '\n')
                return EOLFormat::Win_Format;
            else
                return EOLFormat::Mac_Format;
        }
        if (data[i] == '\n')
            return EOLFormat::Unix_Format;
    }
    return EOLFormat::Unknown_Format;
}

void CheckForTabs(const char* data, DWORD len, TabSpace& tabSpace)
{
    if (tabSpace != TabSpace::Tabs)
    {
        for (DWORD i = 0; i < len; ++i)
        {
            if (data[i] == '\t')
            {
                tabSpace = TabSpace::Tabs;
                break;
            }
        }
    }
}

void LoadSomeUtf8(Scintilla::ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile, char* data, EOLFormat& eolFormat, TabSpace& tabSpace)
{
    char* pData = data;
    // Nothing to convert, just pass it to Scintilla
    if (bFirst && hasBOM)
    {
        pData += 3;
        lenFile -= 3;
    }
    if (eolFormat == EOLFormat::Unknown_Format)
        eolFormat = SenseEOLFormat(pData, lenFile);
    if (tabSpace != TabSpace::Tabs)
        CheckForTabs(pData, lenFile, tabSpace);
    edit.AddData(pData, lenFile);
    if (bFirst && hasBOM)
        lenFile += 3;
}

void loadSomeUtf16Le(Scintilla::ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
                     char* data, char* charBuf, int charBufSize, wchar_t* wideBuf, EOLFormat& eolFormat, TabSpace& tabSpace)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 2;
        lenFile -= 2;
    }
    memcpy(wideBuf, pData, lenFile);
    int charLen = WideCharToMultiByte(CP_UTF8, 0, wideBuf, lenFile / 2, charBuf, charBufSize, nullptr, nullptr);
    if (eolFormat == EOLFormat::Unknown_Format)
        eolFormat = SenseEOLFormat(charBuf, charLen);
    if (tabSpace != TabSpace::Tabs)
        CheckForTabs(charBuf, charLen, tabSpace);
    edit.AddData(charBuf, charLen);
    if (bFirst && hasBOM)
        lenFile += 2;
}

void loadSomeUtf16Be(Scintilla::ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
                     char* data, char* charBuf, int charBufSize, wchar_t* wideBuf, EOLFormat& eolFormat, TabSpace& tabSpace)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 2;
        lenFile -= 2;
    }
    memcpy(wideBuf, pData, lenFile);
    // make in place WORD BYTEs swap
    UINT64* pQw     = reinterpret_cast<UINT64*>(wideBuf);
    int     nQWords = lenFile / 8;
    for (int nQWord = 0; nQWord < nQWords; nQWord++)
        pQw[nQWord] = WordSwapBytes(pQw[nQWord]);

    wchar_t* pW     = reinterpret_cast<wchar_t*>(pQw);
    int      nWords = lenFile / 2;
    for (int nWord = nQWords * 4; nWord < nWords; nWord++)
        pW[nWord] = WideCharSwap(pW[nWord]);

    int charLen = WideCharToMultiByte(CP_UTF8, 0, wideBuf, lenFile / 2, charBuf, charBufSize, nullptr, nullptr);
    if (eolFormat == EOLFormat::Unknown_Format)
        eolFormat = SenseEOLFormat(charBuf, charLen);
    if (tabSpace != TabSpace::Tabs)
        CheckForTabs(charBuf, charLen, tabSpace);
    edit.AddData(charBuf, charLen);
    if (bFirst && hasBOM)
        lenFile += 2;
}

void loadSomeUtf32Be(DWORD lenFile, char* data)
{
    UINT64* p64     = reinterpret_cast<UINT64*>(data);
    int     nQWords = lenFile / 8;
    for (int nQWord = 0; nQWord < nQWords; nQWord++)
        p64[nQWord] = DwordSwapBytes(p64[nQWord]);

    UINT32* p32     = reinterpret_cast<UINT32*>(p64);
    int     nDWords = lenFile / 4;
    for (int nDword = nQWords * 2; nDword < nDWords; nDword++)
        p32[nDword] = DwordSwapBytes(p32[nDword]);
}

void loadSomeUtf32Le(Scintilla::ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
                     char* data, char* charBuf, int charBufSize, wchar_t* wideBuf, EOLFormat& eolFormat, TabSpace& tabSpace)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 4;
        lenFile -= 4;
    }
    // UTF32 have four bytes per char
    int     nReadChars = lenFile / 4;
    UINT32* p32        = reinterpret_cast<UINT32*>(pData);

    // fill buffer
    wchar_t* pOut = static_cast<wchar_t*>(wideBuf);
    for (int i = 0; i < nReadChars; ++i, ++pOut)
    {
        UINT32 zChar = p32[i];
        if (zChar >= 0x110000)
        {
            *pOut = 0xfffd; // ? mark
        }
        else if (zChar >= 0x10000)
        {
            zChar -= 0x10000;
            pOut[0] = ((zChar >> 10) & 0x3ff) | 0xd800; // lead surrogate
            pOut[1] = (zChar & 0x7ff) | 0xdc00;         // trail surrogate
            pOut++;
        }
        else
        {
            *pOut = static_cast<wchar_t>(zChar);
        }
    }
    int charLen = WideCharToMultiByte(CP_UTF8, 0, wideBuf, nReadChars, charBuf, charBufSize, nullptr, nullptr);
    if (eolFormat == EOLFormat::Unknown_Format)
        eolFormat = SenseEOLFormat(charBuf, charLen);
    if (tabSpace != TabSpace::Tabs)
        CheckForTabs(charBuf, charLen, tabSpace);
    edit.AddData(charBuf, charLen);
    if (bFirst && hasBOM)
        lenFile += 4;
}

void LoadSomeOther(Scintilla::ILoader& edit, int encoding, DWORD lenFile,
                   int& incompleteMultiByteChar, char* data, char* charBuf, int charBufSize, wchar_t* wideBuf, EOLFormat& eolFormat, TabSpace& tabSpace)
{
    // For other encodings, ask system if there are any invalid characters; note that it will
    // not correctly know if the last character is cut when there are invalid characters inside the text
    int wideLen = MultiByteToWideChar(encoding, (lenFile == -1) ? 0 : MB_ERR_INVALID_CHARS, data, lenFile, nullptr, 0);
    if (wideLen == 0 && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
    {
        // Test without last byte
        if (lenFile > 1)
            wideLen = MultiByteToWideChar(encoding, MB_ERR_INVALID_CHARS, data, lenFile - 1, nullptr, 0);
        if (wideLen == 0)
        {
            // don't have to check that the error is still ERROR_NO_UNICODE_TRANSLATION,
            // since only the length parameter changed

            // TODO: should warn user about incorrect loading due to invalid characters
            // We still load the file, but the system will either strip or replace invalid characters
            // (including the last character, if cut in half)
            wideLen = MultiByteToWideChar(encoding, 0, data, lenFile, nullptr, 0);
        }
        else
        {
            // We found a valid text by removing one byte.
            incompleteMultiByteChar = 1;
        }
    }
    if (wideLen > 0)
    {
        MultiByteToWideChar(encoding, 0, data, lenFile - incompleteMultiByteChar, wideBuf, wideLen);
        int charLen = WideCharToMultiByte(CP_UTF8, 0, wideBuf, wideLen, charBuf, charBufSize, nullptr, nullptr);
        if (eolFormat == EOLFormat::Unknown_Format)
            eolFormat = SenseEOLFormat(charBuf, charLen);
        if (tabSpace != TabSpace::Tabs)
            CheckForTabs(charBuf, charLen, tabSpace);
        edit.AddData(charBuf, charLen);
    }
}

bool AskToElevatePrivilege(HWND hWnd, const std::wstring& path, PCWSTR sElevate, PCWSTR sDontElevate)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString    rTitle(g_hRes, IDS_ACCESS_ELEVATE);
    ResString    rQuestion(g_hRes, IDS_ACCESS_ASK_ELEVATE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 101;
    aCustomButtons[0].pszButtonText     = sElevate;
    aCustomButtons[1].nButtonID         = 100;
    aCustomButtons[1].pszButtonText     = sDontElevate;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    tdc.nDefaultButton                  = 101;

    tdc.hwndParent      = hWnd;
    tdc.hInstance       = g_hRes;
    tdc.dwFlags         = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;

    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_SHIELD_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // We've used TDCBF_CANCEL_BUTTON so IDCANCEL can be returned,
    // map that to don't elevate.
    bool bElevate = (nClickedBtn == 101);
    return bElevate;
}

bool AskToElevatePrivilegeForOpening(HWND hWnd, const std::wstring& path)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rElevate(g_hRes, IDS_ELEVATEOPEN);
    ResString rDontElevate(g_hRes, IDS_DONTELEVATEOPEN);
    return AskToElevatePrivilege(hWnd, path, rElevate, rDontElevate);
}

DWORD RunSelfElevated(HWND hWnd, const std::wstring& params, bool wait)
{
    std::wstring     modPath    = CPathUtils::GetModulePath();
    SHELLEXECUTEINFO shExecInfo = {sizeof(SHELLEXECUTEINFO)};

    shExecInfo.hwnd         = hWnd;
    shExecInfo.fMask        = SEE_MASK_NOCLOSEPROCESS;
    shExecInfo.lpVerb       = L"runas";
    shExecInfo.lpFile       = modPath.c_str();
    shExecInfo.lpParameters = params.c_str();
    shExecInfo.nShow        = SW_NORMAL;
    OnOutOfScope(CloseHandle(shExecInfo.hProcess));
    if (!ShellExecuteEx(&shExecInfo))
        return ::GetLastError();

    if (wait && shExecInfo.hProcess)
        WaitForSingleObject(shExecInfo.hProcess, INFINITE);

    return 0;
}

void ShowFileLoadError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg)
{
    ResString rTitle(g_hRes, IDS_APP_TITLE);
    ResString rLoadErr(g_hRes, IDS_FAILEDTOLOADFILE);
    MessageBox(hWnd, CStringUtils::Format(rLoadErr, fileName.c_str(), msg).c_str(), static_cast<LPCWSTR>(rTitle), MB_ICONERROR);
}

void ShowFileSaveError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg)
{
    ResString rTitle(g_hRes, IDS_APP_TITLE);
    ResString rSaveErr(g_hRes, IDS_FAILEDTOSAVEFILE);
    MessageBox(hWnd, CStringUtils::Format(rSaveErr, fileName.c_str(), msg).c_str(), static_cast<LPCWSTR>(rTitle), MB_ICONERROR);
}

void SetEOLType(const CScintillaWnd& edit, const CDocument& doc)
{
    switch (doc.m_format)
    {
        case EOLFormat::Win_Format:
            edit.SetEOLType(Scintilla::EndOfLine::CrLf);
            break;
        case EOLFormat::Unix_Format:
            edit.SetEOLType(Scintilla::EndOfLine::Lf);
            break;
        case EOLFormat::Mac_Format:
            edit.SetEOLType(Scintilla::EndOfLine::Cr);
            break;
        default:
            break;
    }
}
} // namespace

CDocumentManager::CDocumentManager()
    : m_scratchScintilla(g_hRes)
    , m_data{}
{
    m_scratchScintilla.InitScratch(g_hRes);
    m_wideBuf = std::make_unique<wchar_t[]>(m_wideBufSize);
    m_charBuf = std::make_unique<char[]>(m_charBufSize);
}

CDocumentManager::~CDocumentManager()
{
}

bool CDocumentManager::HasDocumentID(DocID id) const
{
    // Allow searches of things with an invalid id, to simplify things for the caller.
    auto where = m_documents.find(id);
    if (where == m_documents.end())
        return false;

    // If we find something with an invalid id, something is very wrong.
    APPVERIFY(id.IsValid());

    return true;
}

const CDocument& CDocumentManager::GetDocumentFromID(DocID id) const
{
    auto pos = m_documents.find(id);
    if (pos == std::end(m_documents))
    {
        APPVERIFY(false);
        return g_emptyDoc;
    }
    return pos->second;
}

CDocument& CDocumentManager::GetModDocumentFromID(DocID id)
{
    auto pos = m_documents.find(id);
    if (pos == std::end(m_documents))
    {
        APPVERIFY(false);
        return g_emptyDoc;
    }
    return pos->second;
}

void CDocumentManager::AddDocumentAtEnd(const CDocument& doc, DocID id)
{
    // Catch attempts to id's that serve as null type values.
    APPVERIFY(id.IsValid());                              // Serious bug.
    APPVERIFY(m_documents.find(id) == m_documents.end()); // Should not already exist.
    m_documents[id] = doc;
}

CDocument CDocumentManager::LoadFile(HWND hWnd, const std::wstring& path, int encoding, bool createIfMissing)
{
    CDocument doc;
    doc.m_format = EOLFormat::Unknown_Format;

    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, createIfMissing ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        // Capture the access denied error, while it's valid.
        DWORD                 err = GetLastError();
        CFormatMessageWrapper errMsg(err);
        if ((err == ERROR_ACCESS_DENIED || err == ERROR_WRITE_PROTECT) && (!SysInfo::Instance().IsElevated()))
        {
            if (!PathIsDirectory(path.c_str()) && AskToElevatePrivilegeForOpening(hWnd, path))
            {
                // 1223 - operation canceled by user.
                std::wstring params = L"\"";
                params += path;
                params += L"\"";
                DWORD elevationError = RunSelfElevated(hWnd, params, false);
                // If we get no error attempting to running another instance elevated,
                // assume any further errors that might occur completing the operation
                // will be issued by that instance, so return now.
                if (elevationError == 0)
                    return doc;
                // If the user hasn't canceled explain why we
                // couldn't elevate. If they did cancel, they no that so no need
                // to tell them that!
                if (elevationError != ERROR_CANCELLED)
                {
                    CFormatMessageWrapper errMsgElev(elevationError);
                    ShowFileLoadError(hWnd, path, errMsgElev);
                }
                // Exhausted all operations to work around the problem,
                // fall through to inform that what the final outcome is
                // which is the original error thy got.
            }
            // else if canceled elevation via various means or got an error even asking.
            // just fall through and issue the error that failed.
        }
        ShowFileLoadError(hWnd, path, errMsg);
        return doc;
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        CFormatMessageWrapper errMsg; // Calls GetLastError itself.
        ShowFileLoadError(hWnd, path, errMsg);
        return doc;
    }
    doc.m_bIsReadonly         = (fi.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;
    doc.m_lastWriteTime       = fi.ftLastWriteTime;
    doc.m_path                = path;
    unsigned __int64 fileSize = static_cast<__int64>(fi.nFileSizeHigh) << 32 | fi.nFileSizeLow;
    // add more room for Scintilla (usually 1/6 more for editing)
    unsigned __int64 bufferSizeRequested = fileSize + min(1 << 20, fileSize / 6);

#ifdef _DEBUG
    ProfileTimer timer(L"LoadFile");
#endif

    // Setup our scratch scintilla control to load the data
    m_scratchScintilla.Scintilla().SetStatus(Scintilla::Status::Ok); // reset error status
    m_scratchScintilla.Scintilla().SetDocPointer(nullptr);
    bool ro = m_scratchScintilla.Scintilla().ReadOnly() != 0;
    if (ro)
        m_scratchScintilla.Scintilla().SetReadOnly(false); // we need write access
    m_scratchScintilla.Scintilla().SetUndoCollection(false);
    m_scratchScintilla.Scintilla().ClearAll();
    m_scratchScintilla.Scintilla().SetCodePage(CP_UTF8);
    Scintilla::DocumentOption docOptions = Scintilla::DocumentOption::Default;
    if (bufferSizeRequested > INT_MAX)
        docOptions = Scintilla::DocumentOption::TextLarge | Scintilla::DocumentOption::StylesNone;
    Scintilla::ILoader* pdocLoad = static_cast<Scintilla::ILoader*>(m_scratchScintilla.Scintilla().CreateLoader(static_cast<uptr_t>(bufferSizeRequested), docOptions));
    if (pdocLoad == nullptr)
    {
        ShowFileLoadError(hWnd, path,
                          CLanguage::Instance().GetTranslatedString(ResString(g_hRes, IDS_ERR_FILETOOBIG)).c_str());
        return doc;
    }
    auto& edit = *pdocLoad;

    DWORD lenFile                 = 0;
    int   incompleteMultiByteChar = 0;
    bool  bFirst                  = true;
    bool  preferUtf8              = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingutf8overansi", 0) != 0;
    bool  inconclusive            = false;
    bool  encodingSet             = encoding != -1;
    int   skip                    = 0;
    do
    {
        if (!ReadFile(hFile, m_data + incompleteMultiByteChar, ReadBlockSize - incompleteMultiByteChar, &lenFile, nullptr))
            lenFile = 0;
        else
            lenFile += incompleteMultiByteChar;
        incompleteMultiByteChar = 0;

        if ((!encodingSet) || (inconclusive && encoding == CP_ACP))
        {
            encoding = GetCodepageFromBuf(m_data + skip, lenFile - skip, doc.m_bHasBOM, inconclusive, skip);
            if (inconclusive && encoding == CP_ACP)
                encoding = CP_UTF8;
        }
        encodingSet = true;

        doc.m_encoding = encoding;

        switch (encoding)
        {
            case -1:
            case CP_UTF8:
                LoadSomeUtf8(edit, doc.m_bHasBOM, bFirst, lenFile, m_data, doc.m_format, doc.m_tabSpace);
                break;
            case 1200: // UTF16_LE
                loadSomeUtf16Le(edit, doc.m_bHasBOM, bFirst, lenFile, m_data, m_charBuf.get(), m_charBufSize, m_wideBuf.get(), doc.m_format, doc.m_tabSpace);
                break;
            case 1201: // UTF16_BE
                loadSomeUtf16Be(edit, doc.m_bHasBOM, bFirst, lenFile, m_data, m_charBuf.get(), m_charBufSize, m_wideBuf.get(), doc.m_format, doc.m_tabSpace);
                break;
            case 12001:                           // UTF32_BE
                loadSomeUtf32Be(lenFile, m_data); // Doesn't load, falls through to load.
                [[fallthrough]];
            case 12000: // UTF32_LE
                loadSomeUtf32Le(edit, doc.m_bHasBOM, bFirst, lenFile, m_data, m_charBuf.get(), m_charBufSize, m_wideBuf.get(), doc.m_format, doc.m_tabSpace);
                break;
            default:
                LoadSomeOther(edit, encoding, lenFile, incompleteMultiByteChar, m_data, m_charBuf.get(), m_charBufSize, m_wideBuf.get(), doc.m_format, doc.m_tabSpace);
                break;
        }

        if (incompleteMultiByteChar != 0) // copy bytes to next buffer
            memcpy(m_data, m_data + ReadBlockSize - incompleteMultiByteChar, incompleteMultiByteChar);

        bFirst = false;
    } while (lenFile == ReadBlockSize);

    if (preferUtf8 && inconclusive && doc.m_encoding == CP_ACP)
        doc.m_encoding = CP_UTF8;

    if (doc.m_format == EOLFormat::Unknown_Format)
        doc.m_format = EOLFormat::Win_Format;

    auto loadedDoc = pdocLoad->ConvertToDocument();          // loadedDoc has reference count 1
    m_scratchScintilla.Scintilla().SetDocPointer(loadedDoc); // doc in scratch has reference count 2 (loadedDoc 1, added one)
    m_scratchScintilla.Scintilla().SetUndoCollection(true);
    m_scratchScintilla.Scintilla().EmptyUndoBuffer();
    m_scratchScintilla.Scintilla().SetSavePoint();
    SetEOLType(m_scratchScintilla, doc);
    if (ro || doc.m_bIsReadonly || doc.m_bIsWriteProtected)
        m_scratchScintilla.Scintilla().SetReadOnly(true);
    doc.m_document = m_scratchScintilla.Scintilla().DocPointer(); // doc.m_document has reference count of 2
    m_scratchScintilla.Scintilla().SetDocPointer(nullptr);        // now doc.m_document has reference count of 1, and the scratch does not hold any doc anymore

    return doc;
}

static bool SaveAsUtf16(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    constexpr int writeWideBufSize = WriteBlockSize * 2;
    auto          wideBuf          = std::make_unique<wchar_t[]>(writeWideBufSize);
    err.clear();
    DWORD bytesWritten = 0;

    auto encoding = doc.m_encoding;
    auto hasBOM   = doc.m_bHasBOM;
    if (doc.m_encodingSaving != -1)
    {
        encoding = doc.m_encodingSaving;
        hasBOM   = doc.m_bHasBOMSaving;
    }

    if (hasBOM)
    {
        BOOL result = FALSE;
        if (encoding == 1200)
            result = WriteFile(hFile, "\xFF\xFE", 2, &bytesWritten, nullptr);
        else
            result = WriteFile(hFile, "\xFE\xFF", 2, &bytesWritten, nullptr);
        if (!result || bytesWritten != 2)
        {
            CFormatMessageWrapper errMsg;
            err = errMsg.c_str();
            return false;
        }
    }
    char* writeBuf = buf;
    do
    {
        int charStart = UTF8Helper::characterStart(writeBuf, static_cast<int>(min(WriteBlockSize, lengthDoc)));
        int wideLen   = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, wideBuf.get(), writeWideBufSize);
        if (encoding == 1201)
        {
            UINT64* pQw     = reinterpret_cast<UINT64*>(wideBuf.get());
            int     nQWords = wideLen / 4;
            for (int nQWord = 0; nQWord < nQWords; nQWord++)
                pQw[nQWord] = WordSwapBytes(pQw[nQWord]);
            auto* pW     = reinterpret_cast<wchar_t*>(pQw);
            int   nWords = wideLen;
            for (int nWord = nQWords * 4; nWord < nWords; nWord++)
                pW[nWord] = WideCharSwap(pW[nWord]);
        }
        if (!WriteFile(hFile, wideBuf.get(), wideLen * 2, &bytesWritten, nullptr) || wideLen != static_cast<int>(bytesWritten / 2))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg.c_str();
            return false;
        }
        writeBuf += charStart;
        lengthDoc -= charStart;
    } while (lengthDoc > 0);
    return true;
}

static bool SaveAsUtf32(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    constexpr int writeWideBufSize = WriteBlockSize * 2;
    auto          writeWideBuf     = std::make_unique<wchar_t[]>(writeWideBufSize);
    auto          writeWide32Buf   = std::make_unique<wchar_t[]>(writeWideBufSize * 2);
    DWORD         bytesWritten     = 0;
    BOOL          result           = FALSE;

    auto encoding = doc.m_encoding;
    if (doc.m_encodingSaving != -1)
        encoding = doc.m_encodingSaving;

    if (encoding == 12000)
        result = WriteFile(hFile, "\xFF\xFE\0\0", 4, &bytesWritten, nullptr);
    else
        result = WriteFile(hFile, "\0\0\xFE\xFF", 4, &bytesWritten, nullptr);
    if (!result || bytesWritten != 4)
    {
        CFormatMessageWrapper errMsg;
        err = errMsg.c_str();
        return false;
    }
    char* writeBuf = buf;
    do
    {
        int charStart = UTF8Helper::characterStart(writeBuf, static_cast<int>(min(WriteBlockSize, lengthDoc)));
        int wideLen   = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, writeWideBuf.get(), writeWideBufSize);

        LPCWSTR pIn       = static_cast<LPCWSTR>(writeWideBuf.get());
        UINT32* pOut      = reinterpret_cast<UINT32*>(writeWide32Buf.get());
        int     nOutDWord = 0;
        for (int nInWord = 0; nInWord < wideLen; nInWord++, nOutDWord++)
        {
            UINT32 zChar = pIn[nInWord];
            if ((zChar & 0xfc00) == 0xd800) // lead surrogate
            {
                if (nInWord + 1 < wideLen && (pIn[nInWord + 1] & 0xfc00) == 0xdc00) // trail surrogate follows
                    zChar = 0x10000 + ((zChar & 0x3ff) << 10) + (pIn[++nInWord] & 0x3ff);
                else
                    zChar = 0xfffd; // ? mark
            }
            else if ((zChar & 0xfc00) == 0xdc00) // trail surrogate without lead
                zChar = 0xfffd;                  // ? mark
            pOut[nOutDWord] = zChar;
        }

        if (encoding == 12001)
        {
            UINT64* p64     = reinterpret_cast<UINT64*>(writeWide32Buf.get());
            int     nQWords = wideLen / 2;
            for (int nQWord = 0; nQWord < nQWords; nQWord++)
                p64[nQWord] = DwordSwapBytes(p64[nQWord]);

            UINT32* p32     = reinterpret_cast<UINT32*>(p64);
            int     nDWords = wideLen;
            for (int nDword = nQWords * 2; nDword < nDWords; nDword++)
                p32[nDword] = DwordSwapBytes(p32[nDword]);
        }
        if (!WriteFile(hFile, writeWide32Buf.get(), wideLen * 4, &bytesWritten, nullptr) || wideLen != static_cast<int>(bytesWritten / 4))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg.c_str();
            return false;
        }
        writeBuf += charStart;
        lengthDoc -= charStart;
    } while (lengthDoc > 0);
    return true;
}

static bool SaveAsUtf8(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    // UTF8: save the buffer as it is
    DWORD bytesWritten = 0;
    auto  hasBOM       = doc.m_bHasBOM;
    if (doc.m_encodingSaving != -1)
        hasBOM = doc.m_bHasBOMSaving;

    if (hasBOM)
    {
        if (!WriteFile(hFile, "\xEF\xBB\xBF", 3, &bytesWritten, nullptr) || bytesWritten != 3)
        {
            CFormatMessageWrapper errMsg;
            err = errMsg.c_str();
            return false;
        }
    }
    do
    {
        DWORD writeLen = static_cast<DWORD>(min(WriteBlockSize, lengthDoc));
        if (!WriteFile(hFile, buf, writeLen, &bytesWritten, nullptr))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg.c_str();
            return false;
        }
        lengthDoc -= writeLen;
        buf += writeLen;
    } while (lengthDoc > 0);
    return true;
}

static bool SaveAsOther(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    constexpr int wideBufSize = WriteBlockSize * 2;
    auto          wideBuf     = std::make_unique<wchar_t[]>(wideBufSize);
    constexpr int charBufSize = wideBufSize * 2;
    auto          charBuf     = std::make_unique<char[]>(charBufSize);
    // first convert to wide char, then to the requested codepage
    DWORD bytesWritten = 0;
    auto  encoding     = doc.m_encoding;
    if (doc.m_encodingSaving != -1)
        encoding = doc.m_encodingSaving;

    char* writeBuf = buf;
    do
    {
        int  charStart       = UTF8Helper::characterStart(writeBuf, static_cast<int>(min(WriteBlockSize, lengthDoc)));
        int  wideLen         = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, wideBuf.get(), wideBufSize);
        BOOL usedDefaultChar = FALSE;
        int  charLen         = WideCharToMultiByte(encoding < 0 ? CP_ACP : encoding, 0, wideBuf.get(), wideLen, charBuf.get(), charBufSize, nullptr, &usedDefaultChar);
        if (usedDefaultChar && doc.m_encodingSaving == -1)
        {
            // stream could not be properly converted to ANSI, write it 'as is'
            if (!WriteFile(hFile, writeBuf, charStart, &bytesWritten, nullptr) || charLen != static_cast<int>(bytesWritten))
            {
                CFormatMessageWrapper errMsg;
                err = errMsg.c_str();
                return false;
            }
        }
        else
        {
            if (!WriteFile(hFile, charBuf.get(), charLen, &bytesWritten, nullptr) || charLen != static_cast<int>(bytesWritten))
            {
                CFormatMessageWrapper errMsg;
                err = errMsg.c_str();
                return false;
            }
        }
        writeBuf += charStart;
        lengthDoc -= charStart;
    } while (lengthDoc > 0);
    return true;
}

bool CDocumentManager::SaveDoc(HWND hWnd, const std::wstring& path, const CDocument& doc) const
{
    if (path.empty())
        return false;
    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        ShowFileSaveError(hWnd, path, errMsg);
        return false;
    }

    m_scratchScintilla.Scintilla().SetDocPointer(doc.m_document);
    size_t lengthDoc = m_scratchScintilla.Scintilla().Length();
    // get characters directly from Scintilla buffer
    char*        buf = static_cast<char*>(m_scratchScintilla.Scintilla().CharacterPointer());
    bool         ok  = false;
    std::wstring err;
    auto         encoding = doc.m_encoding;
    if (doc.m_encodingSaving != -1)
        encoding = doc.m_encodingSaving;

    switch (encoding)
    {
        case CP_UTF8:
        case -1:
            ok = SaveAsUtf8(doc, buf, lengthDoc, hFile, err);
            break;
        case 1200: // UTF16_LE
        case 1201: // UTF16_BE
            ok = SaveAsUtf16(doc, buf, lengthDoc, hFile, err);
            break;
        case 12000: // UTF32_LE
        case 12001: // UTF32_BE
            ok = SaveAsUtf32(doc, buf, lengthDoc, hFile, err);
            break;
        default:
            ok = SaveAsOther(doc, buf, lengthDoc, hFile, err);
            break;
    }
    if (!ok)
        ShowFileSaveError(hWnd, path, err.c_str());
    return true;
}

bool CDocumentManager::SaveFile(HWND hWnd, CDocument& doc, bool& bTabMoved) const
{
    bTabMoved = false;
    if (doc.m_path.empty())
        return false;
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    DWORD err        = 0;
    // when opening files, always 'share' as much as possible to reduce problems with virus scanners
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
        err = GetLastError();
    // If the file can't be created, check if the file attributes are the reason we can't open
    // the file for writing. If so, remove the offending ones, we'll re-apply them later.
    // If we can't get the attributes or they aren't the ones we thought were causing the
    // problem or we can't remove them; fall through and go with our original error.
    if (!hFile.IsValid() && err == ERROR_ACCESS_DENIED)
    {
        DWORD undesiredAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;
        attributes                = GetFileAttributes(doc.m_path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & undesiredAttributes) != 0)
        {
            DWORD desiredAttributes = attributes & ~undesiredAttributes;
            if (SetFileAttributes(doc.m_path.c_str(), desiredAttributes))
            {
                hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            }
        }
    }
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg(err);
        if (((err == ERROR_ACCESS_DENIED) || (err == ERROR_WRITE_PROTECT)) && (!SysInfo::Instance().IsElevated()))
        {
            std::wstring tempPath = CTempFiles::Instance().GetTempFilePath(true);

            if (SaveDoc(hWnd, tempPath, doc))
            {
                std::wstring cmdline        = CStringUtils::Format(L"/elevate /savepath:\"%s\" /path:\"%s\"", doc.m_path.c_str(), tempPath.c_str());
                DWORD        elevationError = RunSelfElevated(hWnd, cmdline, true);
                // We don't know if saving worked or not.
                // So return false since this instance didn't do the saving.
                if (elevationError == 0)
                {
                    for (int i = 0; i < 20; ++i)
                    {
                        Sleep(100);
                        if (!PathFileExists(tempPath.c_str()))
                        {
                            bTabMoved = true;
                            return true;
                        }
                    }
                    return false; // Temp path "belongs" to other instance.
                }
                if (elevationError != ERROR_CANCELLED)
                {
                    // Can't elevate. Explain the error,
                    // don't refer to the temp path, that would be confusing.
                    CFormatMessageWrapper errMsgElev(elevationError);
                    ShowFileSaveError(hWnd, doc.m_path, errMsgElev);
                }
            }
        }
        ShowFileSaveError(hWnd, doc.m_path, errMsg);
        return false;
    }

    if (SaveDoc(hWnd, doc.m_path, doc))
    {
        m_scratchScintilla.Scintilla().SetSavePoint();
        m_scratchScintilla.Scintilla().SetDocPointer(nullptr);
        if (doc.m_encodingSaving != -1)
        {
            doc.m_encoding       = doc.m_encodingSaving;
            doc.m_encodingSaving = -1;
            doc.m_bHasBOM        = doc.m_bHasBOMSaving;
            doc.m_bHasBOMSaving  = false;
        }
    }
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        // reset the file attributes after saving
        SetFileAttributes(doc.m_path.c_str(), attributes);
    }
    return true;
}

bool CDocumentManager::SaveFile(HWND hWnd, CDocument& doc, const std::wstring& path) const
{
    return SaveDoc(hWnd, path, doc);
}

bool CDocumentManager::UpdateFileTime(CDocument& doc, bool bIncludeReadonly)
{
    if (doc.m_path.empty())
        return false;
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        auto lastError = GetLastError();
        if ((lastError == ERROR_FILE_NOT_FOUND) || (lastError == ERROR_PATH_NOT_FOUND))
        {
            doc.m_lastWriteTime.dwLowDateTime  = 0;
            doc.m_lastWriteTime.dwHighDateTime = 0;
        }
        return false;
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi))
        return false;

    doc.m_lastWriteTime = fi.ftLastWriteTime;
    if (bIncludeReadonly)
        doc.m_bIsReadonly = (fi.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;
    return true;
}

DocModifiedState CDocumentManager::HasFileChanged(DocID id) const
{
    const auto& doc = GetDocumentFromID(id);
    if (doc.m_path.empty() || ((doc.m_lastWriteTime.dwLowDateTime == 0) && (doc.m_lastWriteTime.dwHighDateTime == 0)) || doc.m_bDoSaveAs)
        return DocModifiedState::Unmodified;

    // get the last write time of the base doc file
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        auto lastError = GetLastError();
        if ((lastError == ERROR_FILE_NOT_FOUND) || (lastError == ERROR_PATH_NOT_FOUND))
            return DocModifiedState::Removed;
        return DocModifiedState::Unknown;
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi))
        return DocModifiedState::Unknown;

    if (CompareFileTime(&doc.m_lastWriteTime, &fi.ftLastWriteTime))
        return DocModifiedState::Modified;

    return DocModifiedState::Unmodified;
}

DocID CDocumentManager::GetIdForPath(const std::wstring& path) const
{
    for (const auto& [docId, doc] : m_documents)
    {
        if (CPathUtils::PathCompare(doc.m_path, path) == 0)
            return docId;
    }
    return {};
}

void CDocumentManager::RemoveDocument(DocID id)
{
    const auto& doc = GetDocumentFromID(id);
    m_scratchScintilla.Scintilla().ReleaseDocument(doc.m_document);
    m_documents.erase(id);
}
