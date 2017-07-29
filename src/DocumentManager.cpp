// This file is part of BowPad.
//
// Copyright (C) 2013-2017 - Stefan Kueng
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
#include "ILexer.h"

#include <algorithm>
#include <stdexcept>
#include <Shobjidl.h>

constexpr int ReadBlockSize = 128 * 1024;   //128 kB
constexpr int WriteBlockSize = 128 * 1024;   //128 kB

static CDocument g_EmptyDoc;

static wchar_t inline WideCharSwap(wchar_t nValue)
{
    return (((nValue>> 8)) | (nValue << 8));
}

static UINT64 inline WordSwapBytes(UINT64 nValue)
{
    return ((nValue&0xff00ff00ff00ff)<<8) | ((nValue>>8)&0xff00ff00ff00ff); // swap BYTESs in WORDs
}

static UINT32 inline DwordSwapBytes(UINT32 nValue)
{
    UINT32 nRet = (nValue<<16) | (nValue>>16); // swap WORDs
    nRet = ((nRet&0xff00ff)<<8) | ((nRet>>8)&0xff00ff); // swap BYTESs in WORDs
    return nRet;
}

static UINT64 inline DwordSwapBytes(UINT64 nValue)
{
    UINT64 nRet = ((nValue&0xffff0000ffffL)<<16) | ((nValue>>16)&0xffff0000ffffL); // swap WORDs in DWORDs
    nRet = ((nRet&0xff00ff00ff00ff)<<8) | ((nRet>>8)&0xff00ff00ff00ff); // swap BYTESs in WORDs
    return nRet;
}

static EOLFormat SenseEOLFormat(const char *data, DWORD len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] == '\r')
        {
            if (i + 1 < len && data[i + 1] == '\n')
                return WIN_FORMAT;
            else
                return MAC_FORMAT;
        }
        if (data[i] == '\n')
            return UNIX_FORMAT;
    }
    return UNKNOWN_FORMAT;
}

static void LoadSomeUtf8(ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile, char* data, EOLFormat & eolformat)
{
    char* pData = data;
    // Nothing to convert, just pass it to Scintilla
    if (bFirst && hasBOM)
    {
        pData += 3;
        lenFile -= 3;
    }
    if (eolformat == EOLFormat::UNKNOWN_FORMAT)
        eolformat = SenseEOLFormat(pData, lenFile);
    edit.AddData(pData, lenFile);
    if (bFirst && hasBOM)
        lenFile += 3;
}

static void LoadSomeUtf16le(ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
    char* data, char* charbuf, int charbufSize, wchar_t* widebuf, EOLFormat & eolformat)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 2;
        lenFile -= 2;
    }
    memcpy(widebuf, pData, lenFile);
    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, lenFile / 2, charbuf, charbufSize, 0, nullptr);
    if (eolformat == EOLFormat::UNKNOWN_FORMAT)
        eolformat = SenseEOLFormat(charbuf, charlen);
    edit.AddData(charbuf, charlen);
    if (bFirst && hasBOM)
        lenFile += 2;
}

static void LoadSomeUtf16be(ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
    char* data, char* charbuf, int charbufSize, wchar_t* widebuf, EOLFormat & eolformat)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 2;
        lenFile -= 2;
    }
    memcpy(widebuf, pData, lenFile);
    // make in place WORD BYTEs swap
    UINT64 * p_qw = (UINT64 *)widebuf;
    int nQwords = lenFile / 8;
    for (int nQword = 0; nQword<nQwords; nQword++)
        p_qw[nQword] = WordSwapBytes(p_qw[nQword]);

    wchar_t * p_w = (wchar_t *)p_qw;
    int nWords = lenFile / 2;
    for (int nWord = nQwords * 4; nWord<nWords; nWord++)
        p_w[nWord] = WideCharSwap(p_w[nWord]);

    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, lenFile / 2, charbuf, charbufSize, 0, nullptr);
    if (eolformat == EOLFormat::UNKNOWN_FORMAT)
        eolformat = SenseEOLFormat(charbuf, charlen);
    edit.AddData(charbuf, charlen);
    if (bFirst && hasBOM)
        lenFile += 2;
}

static void LoadSomeUtf32be(DWORD lenFile, char* data)
{
    UINT64 * p64 = (UINT64 *)data;
    int nQwords = lenFile / 8;
    for (int nQword = 0; nQword<nQwords; nQword++)
        p64[nQword] = DwordSwapBytes(p64[nQword]);

    UINT32 * p32 = (UINT32 *)p64;
    int nDwords = lenFile / 4;
    for (int nDword = nQwords * 2; nDword<nDwords; nDword++)
        p32[nDword] = DwordSwapBytes(p32[nDword]);
}

static void LoadSomeUtf32le(ILoader& edit, bool hasBOM, bool bFirst, DWORD& lenFile,
    char* data, char* charbuf, int charbufSize, wchar_t* widebuf, EOLFormat & eolformat)
{
    char* pData = data;
    if (bFirst && hasBOM)
    {
        pData += 4;
        lenFile -= 4;
    }
    // UTF32 have four bytes per char
    int nReadChars = lenFile / 4;
    UINT32 * p32 = (UINT32 *)pData;

    // fill buffer
    wchar_t * pOut = (wchar_t *)widebuf;
    for (int i = 0; i<nReadChars; ++i, ++pOut)
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
            pOut[1] = (zChar & 0x7ff) | 0xdc00; // trail surrogate
            pOut++;
        }
        else
        {
            *pOut = (wchar_t)zChar;
        }
    }
    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, nReadChars, charbuf, charbufSize, 0, nullptr);
    if (eolformat == EOLFormat::UNKNOWN_FORMAT)
        eolformat = SenseEOLFormat(charbuf, charlen);
    edit.AddData(charbuf, charlen);
    if (bFirst && hasBOM)
        lenFile += 4;
}

static void LoadSomeOther(ILoader& edit, int encoding, DWORD lenFile,
    int& incompleteMultibyteChar, char* data, char* charbuf, int charbufSize, wchar_t* widebuf, EOLFormat & eolformat)
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
            incompleteMultibyteChar = 1;
        }
    }
    if (wideLen > 0)
    {
        MultiByteToWideChar(encoding, 0, data, lenFile - incompleteMultibyteChar, widebuf, wideLen);
        int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, wideLen, charbuf, charbufSize, 0, nullptr);
        if (eolformat == EOLFormat::UNKNOWN_FORMAT)
            eolformat = SenseEOLFormat(charbuf, charlen);
        edit.AddData(charbuf, charlen);
    }
}

static bool AskToElevatePrivilege(HWND hWnd, const std::wstring& path, PCWSTR sElevate, PCWSTR sDontElevate)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rTitle(hRes, IDS_ACCESS_ELEVATE);
    ResString rQuestion(hRes, IDS_ACCESS_ASK_ELEVATE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = sElevate;
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = sDontElevate;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    tdc.nDefaultButton = 101;

    tdc.hwndParent = hWnd;
    tdc.hInstance = hRes;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.dwCommonButtons = TDCBF_CANCEL_BUTTON;

    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon = TD_SHIELD_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent = sQuestion.c_str();
    int nClickedBtn = 0;
    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        return 0;
    // We've used TDCBF_CANCEL_BUTTON so IDCANCEL can be returned,
    // map that to don't elevate.
    bool bElevate = (nClickedBtn == 101);
    return bElevate;
}

static bool AskToElevatePrivilegeForOpening(HWND hWnd, const std::wstring& path)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rElevate(hRes, IDS_ELEVATEOPEN);
    ResString rDontElevate(hRes, IDS_DONTELEVATEOPEN);
    return AskToElevatePrivilege(hWnd, path, rElevate, rDontElevate);
}

static bool AskToElevatePrivilegeForSaving(HWND hWnd, const std::wstring& path)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rElevate(hRes, IDS_ELEVATESAVE);
    ResString rDontElevate(hRes, IDS_DONTELEVATESAVE);
    return AskToElevatePrivilege(hWnd, path, rElevate, rDontElevate);
}

static DWORD RunSelfElevated(HWND hWnd, const std::wstring& params)
{
    std::wstring modpath = CPathUtils::GetModulePath();
    SHELLEXECUTEINFO shExecInfo = { sizeof(SHELLEXECUTEINFO) };

    shExecInfo.hwnd = hWnd;
    shExecInfo.lpVerb = L"runas";
    shExecInfo.lpFile = modpath.c_str();
    shExecInfo.lpParameters = params.c_str();
    shExecInfo.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&shExecInfo))
        return ::GetLastError();

    return 0;
}

static void ShowFileLoadError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg)
{
    ResString rTitle(hRes, IDS_APP_TITLE);
    ResString rLoadErr(hRes, IDS_FAILEDTOLOADFILE);
    MessageBox(hWnd, CStringUtils::Format(rLoadErr, fileName.c_str(), msg).c_str(), (LPCWSTR)rTitle, MB_ICONERROR);
}

static void ShowFileSaveError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg)
{
    ResString rTitle(hRes, IDS_APP_TITLE);
    ResString rSaveErr(hRes, IDS_FAILEDTOSAVEFILE);
    MessageBox(hWnd, CStringUtils::Format(rSaveErr, fileName.c_str(), msg).c_str(), (LPCWSTR)rTitle, MB_ICONERROR);
}

static void SetEOLType(CScintillaWnd& edit, const CDocument& doc)
{
    switch (doc.m_format)
    {
    case WIN_FORMAT:
        edit.SetEOLType(SC_EOL_CRLF);
        break;
    case UNIX_FORMAT:
        edit.SetEOLType(SC_EOL_LF);
        break;
    case MAC_FORMAT:
        edit.SetEOLType(SC_EOL_CR);
        break;
    default:
        break;
    }
}

CDocumentManager::CDocumentManager()
    : m_scratchScintilla(hRes)
{
    m_scratchScintilla.InitScratch(hRes);
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

// Must exist or it's a bug.
const CDocument& CDocumentManager::GetDocumentFromID(DocID id) const
{
    auto pos = m_documents.find(id);
    if (pos == std::end(m_documents))
    {
        APPVERIFY(false);
        return g_EmptyDoc;
    }
    return pos->second;
}

CDocument& CDocumentManager::GetModDocumentFromID(DocID id)
{
    auto pos = m_documents.find(id);
    if (pos == std::end(m_documents))
    {
        APPVERIFY(false);
        return g_EmptyDoc;
    }
    return pos->second;
}

void CDocumentManager::AddDocumentAtEnd( const CDocument& doc, DocID id )
{
    // Catch attempts to id's that serve as null type values.
    APPVERIFY(id.IsValid()); // Serious bug.    
    APPVERIFY(m_documents.find(id) == m_documents.end()); // Should not already exist.
    m_documents[id] = doc;
}

CDocument CDocumentManager::LoadFile( HWND hWnd, const std::wstring& path, int encoding, bool createIfMissing)
{
    CDocument doc;
    doc.m_format = UNKNOWN_FORMAT;

    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, createIfMissing ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        // Capture the access denied error, while it's valid.
        DWORD err = GetLastError();
        CFormatMessageWrapper errMsg(err);
        if ((err == ERROR_ACCESS_DENIED || err == ERROR_WRITE_PROTECT) && (!SysInfo::Instance().IsElevated()))
        {
            if (!PathIsDirectory(path.c_str()) && AskToElevatePrivilegeForOpening(hWnd, path))
            {
                // 1223 - operation canceled by user.
                std::wstring params = L"\"";
                params += path;
                params += L"\"";
                DWORD elevationError = RunSelfElevated(hWnd, params);
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
                    CFormatMessageWrapper errMsgelev(elevationError);
                    ShowFileLoadError(hWnd, path, errMsgelev);
                }
                // Exhausted all operations to work around the problem,
                // fall through to inform that what the final outcome is
                // which is the original error thy got.
            }
            // else if canceled elevation via various means or got an error even asking.
            // just fall through and issue the error that failed.
        }
        ShowFileLoadError(hWnd,path,errMsg);
        return doc;
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        CFormatMessageWrapper errMsg; // Calls GetLastError itself.
        ShowFileLoadError(hWnd,path,errMsg);
        return doc;
    }
    doc.m_bIsReadonly = (fi.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;
    doc.m_lastWriteTime = fi.ftLastWriteTime;
    doc.m_path = path;
    unsigned __int64 fileSize = static_cast<__int64>(fi.nFileSizeHigh) << 32 | fi.nFileSizeLow;
    // add more room for Scintilla (usually 1/6 more for editing)
    unsigned __int64 bufferSizeRequested = fileSize + min(1<<20,fileSize/6);

#ifdef _DEBUG
    ProfileTimer timer(L"LoadFile");
#endif

    // Setup our scratch scintilla control to load the data
    m_scratchScintilla.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    bool ro = m_scratchScintilla.Call(SCI_GETREADONLY) != 0;
    if (ro)
        m_scratchScintilla.Call(SCI_SETREADONLY, false);    // we need write access
    m_scratchScintilla.Call(SCI_SETUNDOCOLLECTION, 0);
    m_scratchScintilla.Call(SCI_CLEARALL);
    m_scratchScintilla.Call(SCI_SETCODEPAGE, CP_UTF8);

    ILoader* pdocLoad = reinterpret_cast<ILoader*>(m_scratchScintilla.Call(SCI_CREATELOADER, (int)bufferSizeRequested));
    if (pdocLoad == nullptr)
    {
        ShowFileLoadError(hWnd, path,
                          CLanguage::Instance().GetTranslatedString(ResString(hRes, IDS_ERR_FILETOOBIG)).c_str());
        return doc;
    }
    auto& edit = *pdocLoad;

    char data[ReadBlockSize+8];
    const int widebufSize = ReadBlockSize * 2;
    auto widebuf = std::make_unique<wchar_t[]>(widebufSize);
    const int charbufSize = widebufSize * 2;
    auto charbuf = std::make_unique<char[]>(charbufSize);

    DWORD lenFile = 0;
    int incompleteMultibyteChar = 0;
    bool bFirst = true;
    bool preferutf8 = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingutf8overansi", 0) != 0;
    bool inconclusive = false;
    bool encodingset = encoding != -1;
    do
    {
        if (!ReadFile(hFile, data + incompleteMultibyteChar, ReadBlockSize - incompleteMultibyteChar, &lenFile, nullptr))
            lenFile = 0;
        else
            lenFile += incompleteMultibyteChar;

        if ((!encodingset) || (inconclusive && encoding == CP_ACP))
        {
            encoding = GetCodepageFromBuf(data, lenFile, doc.m_bHasBOM, inconclusive);
        }
        encodingset = true;

        doc.m_encoding = encoding;

        switch (encoding)
        {
        case -1:
        case CP_UTF8:
            LoadSomeUtf8(edit, doc.m_bHasBOM, bFirst, lenFile, data, doc.m_format);
            break;
        case 1200: // UTF16_LE
            LoadSomeUtf16le(edit, doc.m_bHasBOM, bFirst, lenFile, data, charbuf.get(), charbufSize, widebuf.get(), doc.m_format);
            break;
        case 1201: // UTF16_BE
            LoadSomeUtf16be(edit, doc.m_bHasBOM, bFirst, lenFile, data, charbuf.get(), charbufSize, widebuf.get(), doc.m_format);
            break;
        case 12001: // UTF32_BE
            LoadSomeUtf32be(lenFile, data); // Doesn't load, falls through to load.
            // intentional fall-through
        case 12000: // UTF32_LE
            LoadSomeUtf32le(edit, doc.m_bHasBOM, bFirst, lenFile, data, charbuf.get(), charbufSize, widebuf.get(), doc.m_format);
            break;
        default:
            LoadSomeOther(edit, encoding, lenFile, incompleteMultibyteChar, data, charbuf.get(), charbufSize, widebuf.get(), doc.m_format);
            break;
        }

        if (incompleteMultibyteChar != 0) // copy bytes to next buffer
            memcpy(data, data + ReadBlockSize - incompleteMultibyteChar, incompleteMultibyteChar);

        bFirst = false;
    } while (lenFile == ReadBlockSize);

    if (preferutf8 && inconclusive && doc.m_encoding == CP_ACP)
        doc.m_encoding = CP_UTF8;

    if (doc.m_format == UNKNOWN_FORMAT)
        doc.m_format = WIN_FORMAT;

    sptr_t loadeddoc = (sptr_t)pdocLoad->ConvertToDocument();   // loadeddoc has reference count 1
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, loadeddoc);   // doc in scratch has reference count 2 (loadeddoc 1, added one)
    m_scratchScintilla.Call(SCI_SETUNDOCOLLECTION, 1);
    m_scratchScintilla.Call(SCI_EMPTYUNDOBUFFER);
    m_scratchScintilla.Call(SCI_SETSAVEPOINT);
    SetEOLType(m_scratchScintilla, doc);
    if (ro || doc.m_bIsReadonly || doc.m_bIsWriteProtected)
        m_scratchScintilla.Call(SCI_SETREADONLY, true);
    doc.m_document = m_scratchScintilla.Call(SCI_GETDOCPOINTER);    // doc.m_document has reference count of 2
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);               // now doc.m_document has reference count of 1, and the scratch does not hold any doc anymore

    return doc;
}

static bool SaveAsUtf16(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    const int writeWidebufSize = WriteBlockSize * 2;
    auto widebuf = std::make_unique<wchar_t[]>(writeWidebufSize);
    err.clear();
    DWORD bytesWritten = 0;
    if (doc.m_bHasBOM)
    {
        BOOL result;
        if (doc.m_encoding == 1200)
            result = WriteFile(hFile, "\xFF\xFE", 2, &bytesWritten, nullptr);
        else
            result = WriteFile(hFile, "\xFE\xFF", 2, &bytesWritten, nullptr);
        if (!result || bytesWritten != 2)
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
            return false;
        }
    }
    char * writeBuf = buf;
    do
    {
        int charStart = UTF8Helper::characterStart(writeBuf, (int)min(WriteBlockSize, lengthDoc));
        int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, widebuf.get(), writeWidebufSize);
        if (doc.m_encoding == 1201)
        {
            UINT64 * p_qw = reinterpret_cast<UINT64 *>(widebuf.get());
            int nQwords = widelen/4;
            for (int nQword = 0; nQword<nQwords; nQword++)
                p_qw[nQword] = WordSwapBytes(p_qw[nQword]);
            wchar_t * p_w = (wchar_t *)p_qw;
            int nWords = widelen;
            for (int nWord = nQwords*4; nWord<nWords; nWord++)
                p_w[nWord] = WideCharSwap(p_w[nWord]);
        }
        if (!WriteFile(hFile, widebuf.get(), widelen*2, &bytesWritten, nullptr) || widelen != int(bytesWritten/2))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
            return false;
        }
        writeBuf += charStart;
        lengthDoc -= charStart;
    } while (lengthDoc > 0);
    return true;
}

static bool SaveAsUtf32(const CDocument& doc, char*buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    const int writeWidebufSize = WriteBlockSize * 2;
    auto writeWidebuf = std::make_unique<wchar_t[]>(writeWidebufSize);
    auto writeWide32buf = std::make_unique<wchar_t[]>(writeWidebufSize*2);
    DWORD bytesWritten = 0;
    BOOL result;
    if (doc.m_encoding == 12000)
        result = WriteFile(hFile, "\xFF\xFE\0\0", 4, &bytesWritten, nullptr);
    else
        result= WriteFile(hFile, "\0\0\xFE\xFF", 4, &bytesWritten, nullptr);
    if (!result || bytesWritten != 4)
    {
        CFormatMessageWrapper errMsg;
        err = errMsg;
        return false;
    }
    char * writeBuf = buf;
    do
    {
        int charStart = UTF8Helper::characterStart(writeBuf, (int)min(WriteBlockSize, lengthDoc));
        int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, writeWidebuf.get(), writeWidebufSize);

        LPCWSTR p_In = (LPCWSTR)writeWidebuf.get();
        UINT32 * p_Out = (UINT32 *)writeWide32buf.get();
        int nOutDword = 0;
        for (int nInWord = 0; nInWord<widelen; nInWord++, nOutDword++)
        {
            UINT32 zChar = p_In[nInWord];
            if ((zChar&0xfc00) == 0xd800) // lead surrogate
            {
                if (nInWord+1<widelen && (p_In[nInWord+1]&0xfc00) == 0xdc00) // trail surrogate follows
                    zChar = 0x10000 + ((zChar&0x3ff)<<10) + (p_In[++nInWord]&0x3ff);
                else
                    zChar = 0xfffd; // ? mark
            }
            else if ((zChar&0xfc00) == 0xdc00) // trail surrogate without lead
                zChar = 0xfffd; // ? mark
            p_Out[nOutDword] = zChar;
        }

        if (doc.m_encoding == 12001)
        {
            UINT64 * p64 = reinterpret_cast<UINT64 *>(writeWide32buf.get());
            int nQwords = widelen/2;
            for (int nQword = 0; nQword<nQwords; nQword++)
                p64[nQword] = DwordSwapBytes(p64[nQword]);

            UINT32 * p32 = (UINT32 *)p64;
            int nDwords = widelen;
            for (int nDword = nQwords*2; nDword<nDwords; nDword++)
                p32[nDword] = DwordSwapBytes(p32[nDword]);
        }
        if (!WriteFile(hFile, writeWide32buf.get(), widelen*4, &bytesWritten, nullptr) || widelen != int(bytesWritten/4))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
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
    if (doc.m_bHasBOM)
    {
        if (!WriteFile(hFile, "\xEF\xBB\xBF", 3, &bytesWritten, nullptr) || bytesWritten != 3)
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
            return false;
        }
    }
    do
    {
        DWORD writeLen = (DWORD)min(WriteBlockSize, lengthDoc);
        if (!WriteFile(hFile, buf, writeLen, &bytesWritten, nullptr))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
            return false;
        }
        lengthDoc -= writeLen;
        buf += writeLen;
    } while (lengthDoc > 0);
    return true;
}

static bool SaveAsOther(const CDocument& doc, char* buf, size_t lengthDoc, CAutoFile& hFile, std::wstring& err)
{
    const int widebufSize = WriteBlockSize * 2;
    auto widebuf = std::make_unique<wchar_t[]>(widebufSize);
    const int charbufSize = widebufSize * 2;
    auto charbuf = std::make_unique<char[]>(charbufSize);
    // first convert to wide char, then to the requested codepage
    DWORD bytesWritten = 0;
    char * writeBuf = buf;
    do
    {
        int charStart = UTF8Helper::characterStart(writeBuf, (int)min(WriteBlockSize, lengthDoc));
        int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, widebuf.get(), widebufSize);
        int charlen = WideCharToMultiByte(doc.m_encoding < 0 ? CP_ACP : doc.m_encoding, 0, widebuf.get(), widelen, charbuf.get(), charbufSize, 0, nullptr);
        if (!WriteFile(hFile, charbuf.get(), charlen, &bytesWritten, nullptr) || charlen != int(bytesWritten))
        {
            CFormatMessageWrapper errMsg;
            err = errMsg;
            return false;
        }
        writeBuf += charStart;
        lengthDoc -= charStart;
    } while (lengthDoc > 0);
    return true;
}

bool CDocumentManager::SaveDoc( HWND hWnd, const std::wstring& path, const CDocument& doc )
{
    if (path.empty())
        return false;
    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        ShowFileSaveError(hWnd,path,errMsg);
        return false;
    }

    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    size_t lengthDoc = m_scratchScintilla.Call(SCI_GETLENGTH);
    // get characters directly from Scintilla buffer
    char* buf = (char*)m_scratchScintilla.Call(SCI_GETCHARACTERPOINTER);
    bool ok = false;
    std::wstring err;
    switch (doc.m_encoding)
    {
    case CP_UTF8:
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

bool CDocumentManager::SaveFile( HWND hWnd, const CDocument& doc, bool & bTabMoved )
{
    bTabMoved = false;
    if (doc.m_path.empty())
        return false;
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    DWORD err = 0;
    // when opening files, always 'share' as much as possible to reduce problems with virus scanners
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
        err = GetLastError();
    // If the file can't be created, check if the file attributes are the reason we can't open
    // the file for writing. If so, remove the offending ones, we'll re-apply them later.
    // If we can't get the attributes or they aren't the ones we thought were causing the
    // problem or we can't remove them; fall through and go with our original error.
    if (!hFile.IsValid() && err == ERROR_ACCESS_DENIED)
    {
        DWORD undesiredAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;
        attributes = GetFileAttributes(doc.m_path.c_str());
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
            if (AskToElevatePrivilegeForSaving(hWnd,doc.m_path))
            {
                std::wstring temppath = CTempFiles::Instance().GetTempFilePath(true);

                if (SaveDoc(hWnd, temppath, doc))
                {
                    std::wstring cmdline = CStringUtils::Format(L"/elevate /savepath:\"%s\" /path:\"%s\"", doc.m_path.c_str(), temppath.c_str());
                    DWORD elevationError = RunSelfElevated(hWnd, cmdline);
                    // We don't know if saving worked or not.
                    // So return false since this instance didn't do the saving.
                    if (elevationError == 0)
                    {
                        for (int i = 0; i < 20; ++i)
                        {
                            Sleep(100);
                            if (!PathFileExists(temppath.c_str()))
                            {
                                bTabMoved = true;
                                return false;
                            }
                        }
                        return false; // Temp path "belongs" to other instance.
                    }
                    if (elevationError != ERROR_CANCELLED)
                    {
                        // Can't elevate. Explain the error,
                        // don't refer to the temp path, that would be confusing.
                        CFormatMessageWrapper errMsgelev(elevationError);
                        ShowFileSaveError(hWnd, doc.m_path, errMsgelev);
                    }
                }
             }
        }
        ShowFileSaveError(hWnd,doc.m_path,errMsg);
        return false;
    }

    if (SaveDoc(hWnd, doc.m_path, doc))
    {
        m_scratchScintilla.Call(SCI_SETSAVEPOINT);
        m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    }
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        // reset the file attributes after saving
        SetFileAttributes(doc.m_path.c_str(), attributes);
    }
    return true;
}

bool CDocumentManager::UpdateFileTime(CDocument& doc, bool bIncludeReadonly)
{
    if (doc.m_path.empty())
        return false;
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
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

DocModifiedState CDocumentManager::HasFileChanged(DocID id ) const
{
    const auto& doc = GetDocumentFromID(id);
    if (doc.m_path.empty() || ((doc.m_lastWriteTime.dwLowDateTime == 0) && (doc.m_lastWriteTime.dwHighDateTime == 0)) || doc.m_bDoSaveAs)
        return DM_Unmodified;

    // get the last write time of the base doc file
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!hFile.IsValid())
    {
        auto lastError = GetLastError();
        if ((lastError == ERROR_FILE_NOT_FOUND) || (lastError == ERROR_PATH_NOT_FOUND))
            return DM_Removed;
        return DM_Unknown;
    }
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(hFile, &fi))
        return DM_Unknown;

    if (CompareFileTime(&doc.m_lastWriteTime, &fi.ftLastWriteTime))
        return DM_Modified;

    return DM_Unmodified;
}

DocID CDocumentManager::GetIdForPath( const std::wstring& path ) const
{
    for (const auto& d : m_documents)
    {
        if (CPathUtils::PathCompare(d.second.m_path, path) == 0)
            return d.first;
    }
    return{};
}

void CDocumentManager::RemoveDocument(DocID id )
{
    const auto& doc = GetDocumentFromID(id);
    m_scratchScintilla.Call(SCI_RELEASEDOCUMENT, 0, doc.m_document);
    m_documents.erase(id);
}
