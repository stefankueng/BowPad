// This file is part of BowPad.
//
// Copyright (C) 2013-2014 - Stefan Kueng
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

// TODO: Currently, we use exceptions and asserts to detect some pretty serious
// issues that may happen at runtime.
// The idea eventually is to move to a model where we can catch these in
// the message loop and ask the user to save their work and close the app.
// For now just diagnose these problems with asserts and exceptions that will
// abort as that's better than leaving the app in a wildly unknown state.
// This allow also allows us to find these problems in testing and users
// can report issues because they will be aware of them too and not just
// continue with silent corruption problems.
// Will envolve this further later but this is an improvement for now.
//
// Some button handling need work cleaning up.

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

#include <algorithm>
#include <stdexcept>
#include <Shobjidl.h>

static const COLORREF foldercolors[] = {
    RGB(177,199,253),
    RGB(221,253,177),
    RGB(253,177,243),
    RGB(177,253,240),
    RGB(253,218,177),
    RGB(196,177,253),
    RGB(180,253,177),
    RGB(253,177,202),
    RGB(177,225,253),
    RGB(247,253,177),
};

static const int MAX_FOLDERCOLORS = (_countof(foldercolors));

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


CDocumentManager::CDocumentManager(void)
    : m_scratchScintilla(hRes)
    , m_lastfoldercolorindex(0)
{
    m_scratchScintilla.InitScratch(hRes);
}

CDocumentManager::~CDocumentManager(void)
{
}

bool CDocumentManager::HasDocumentID(int id) const
{
    // Allow searches of things with an invalid id, to simplify things for the caller.
    auto where = m_documents.find(id);
    if (where == m_documents.end())
        return false;

    // If we find something with an invalid id, something is very wrong.
    if (id < 0)
        APPVERIFY(false);

    return true;
}

// Must exist or it's a bug.
CDocument CDocumentManager::GetDocumentFromID(int id) const
{
    // Pretty catastrophic if this ever fails.
    // Use HasDocumentID to check first if you're not sure.
    if (id < 0)
        APPVERIFY(false);
    auto pos = m_documents.find(id);
    if (pos == std::end(m_documents))
    {
        APPVERIFY(false);
        return CDocument();
    }
    return pos->second;
}


void CDocumentManager::SetDocument(int id, const CDocument& doc)
{
    auto where = m_documents.find(id);
    // SetDocument/Find does not create a position if it doesn't exist.
    // Use Operator [] for that or really AddDocumentAtEnd etc. to add.
    if (where == std::end(m_documents))
    {
        // Should never happen but if it does, it's a serious bug
        // because we are throwing data away.
        // Ideally we want to notify the user about this but their
        // is no consensus to do that.
        APPVERIFY(false);
        return; // Return now to avoid overwriting memory.
    }
    where->second = doc;
}

void CDocumentManager::AddDocumentAtEnd( const CDocument& doc, int id )
{
    // Catch attempts to id's that serve as null type values.
    if (id<0)
        APPVERIFY(false); // Serious bug.
    m_documents[id] = doc;
}

static void LoadSome( int encoding, CScintillaWnd& edit, const CDocument& doc, bool& bFirst, DWORD& lenFile,
    int& incompleteMultibyteChar, char* data, char* charbuf, int charbufSize, wchar_t* widebuf )

{
    char* pData = data;
    int wideLen = 0;
    switch (encoding)
    {
        case -1:
        case CP_UTF8:
        {
            // Nothing to convert, just pass it to Scintilla
            if (bFirst && doc.m_bHasBOM)
            {
                pData += 3;
                lenFile -= 3;
            }
            edit.AppendText(lenFile, pData);
            if (bFirst && doc.m_bHasBOM)
                lenFile += 3;
        }
        break;
    case 1200: // UTF16_LE
        {
            if (bFirst && doc.m_bHasBOM)
            {
                pData += 2;
                lenFile -= 2;
            }
            memcpy(widebuf, pData, lenFile);
            int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, lenFile/2, charbuf, charbufSize, 0, NULL);
            edit.AppendText(charlen, charbuf);
            if (bFirst && doc.m_bHasBOM)
                lenFile += 2;
        }
        break;
    case 1201: // UTF16_BE
        {
            if (bFirst && doc.m_bHasBOM)
            {
                pData += 2;
                lenFile -= 2;
            }
            memcpy(widebuf, pData, lenFile);
            // make in place WORD BYTEs swap
            UINT64 * p_qw = (UINT64 *)widebuf;
            int nQwords = lenFile/8;
            for (int nQword = 0; nQword<nQwords; nQword++)
            {
                p_qw[nQword] = WordSwapBytes(p_qw[nQword]);
            }
            wchar_t * p_w = (wchar_t *)p_qw;
            int nWords = lenFile/2;
            for (int nWord = nQwords*4; nWord<nWords; nWord++)
            {
                p_w[nWord] = WideCharSwap(p_w[nWord]);
            }
            int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, lenFile/2, charbuf, charbufSize, 0, NULL);
            edit.AppendText(charlen, charbuf);
            if (bFirst && doc.m_bHasBOM)
                lenFile += 2;
        }
        break;
    case 12001: // UTF32_BE
        {
            UINT64 * p64 = (UINT64 *)data;
            int nQwords = lenFile/8;
            for (int nQword = 0; nQword<nQwords; nQword++)
            {
                p64[nQword] = DwordSwapBytes(p64[nQword]);
            }

            UINT32 * p32 = (UINT32 *)p64;
            int nDwords = lenFile/4;
            for (int nDword = nQwords*2; nDword<nDwords; nDword++)
            {
                p32[nDword] = DwordSwapBytes(p32[nDword]);
            }
        }
        // intentional fall-through
    case 12000: // UTF32_LE
        {
            if (bFirst && doc.m_bHasBOM)
            {
                pData += 4;
                lenFile -= 4;
            }
            // UTF32 have four bytes per char
            int nReadChars = lenFile/4;
            UINT32 * p32 = (UINT32 *)pData;

            // fill buffer
            wchar_t * pOut = (wchar_t *)widebuf;
            for (int i = 0; i<nReadChars; ++i, ++pOut)
            {
                UINT32 zChar = p32[i];
                if (zChar>=0x110000)
                {
                    *pOut=0xfffd; // ? mark
                }
                else if (zChar>=0x10000)
                {
                    zChar-=0x10000;
                    pOut[0] = ((zChar>>10)&0x3ff) | 0xd800; // lead surrogate
                    pOut[1] = (zChar&0x7ff) | 0xdc00; // trail surrogate
                    pOut++;
                }
                else
                {
                    *pOut = (wchar_t)zChar;
                }
            }
            int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, nReadChars, charbuf, charbufSize, 0, NULL);
            edit.AppendText(charlen, charbuf);
            if (bFirst && doc.m_bHasBOM)
                lenFile += 4;
        }
        break;
    default:
        {
            // For other encodings, ask system if there are any invalid characters; note that it will
            // not correctly know if the last character is cut when there are invalid characters inside the text
            wideLen = MultiByteToWideChar(encoding, (lenFile == -1) ? 0 : MB_ERR_INVALID_CHARS, data, lenFile, NULL, 0);
            if (wideLen == 0 && GetLastError() == ERROR_NO_UNICODE_TRANSLATION)
            {
                // Test without last byte
                if (lenFile > 1)
                    wideLen = MultiByteToWideChar(encoding, MB_ERR_INVALID_CHARS, data, lenFile-1, NULL, 0);
                if (wideLen == 0)
                {
                    // don't have to check that the error is still ERROR_NO_UNICODE_TRANSLATION,
                    // since only the length parameter changed

                    // TODO: should warn user about incorrect loading due to invalid characters
                    // We still load the file, but the system will either strip or replace invalid characters
                    // (including the last character, if cut in half)
                    wideLen = MultiByteToWideChar(encoding, 0, data, lenFile, NULL, 0);
                }
                else
                {
                    // We found a valid text by removing one byte.
                    incompleteMultibyteChar = 1;
                }
            }
        }
        break;
    }
    if (wideLen > 0)
    {
        MultiByteToWideChar(encoding, 0, data, lenFile-incompleteMultibyteChar, widebuf, wideLen);
        int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf, wideLen, charbuf, charbufSize, 0, NULL);
        edit.AppendText(charlen, charbuf);
    }
}

static bool AskToElevatePrivilegeForOpening(HWND hWnd, const std::wstring& path)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rTitle(hRes, IDS_ACCESS_ELEVATE);
    ResString rQuestion(hRes, IDS_ACCESS_ASK_ELEVATE);
    ResString rElevate(hRes, IDS_ELEVATEOPEN);
    ResString rDontElevate(hRes, IDS_DONTELEVATEOPEN);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = rElevate;
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = rDontElevate;
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
    HRESULT hr = TaskDialogIndirect( &tdc, &nClickedBtn, NULL, NULL );
    if (CAppUtils::FailedShowMessage(hr))
        return 0;
    // We've used TDCBF_CANCEL_BUTTON so IDCANCEL can be returned,
    // map that to don't elevate.
    bool bElevate = (nClickedBtn == 101);
    return bElevate;
}

static bool AskToElevatePrivilegeForSaving(HWND hWnd, const std::wstring& path)
{
    // access to the file is denied, and we're not running with elevated privileges
    // offer to start BowPad with elevated privileges and open the file in that instance
    ResString rTitle(hRes, IDS_ACCESS_ELEVATE);
    ResString rQuestion(hRes, IDS_ACCESS_ASK_ELEVATE);
    ResString rElevate(hRes, IDS_ELEVATESAVE);
    ResString rDontElevate(hRes, IDS_DONTELEVATESAVE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    TASKDIALOG_BUTTON aCustomButtons[2];
    aCustomButtons[0].nButtonID = 101;
    aCustomButtons[0].pszButtonText = rElevate;
    aCustomButtons[1].nButtonID = 100;
    aCustomButtons[1].pszButtonText = rDontElevate;
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
    HRESULT hr = TaskDialogIndirect( &tdc, &nClickedBtn, NULL, NULL );
    if (CAppUtils::FailedShowMessage(hr))
        return 0;
    // We've used TDCBF_CANCEL_BUTTON so IDCANCEL can be returned,
    // map that to don't elevate.
    bool bElevate = (nClickedBtn == 101);
    return bElevate;
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

    if (! ShellExecuteEx(&shExecInfo))
    {
        return ::GetLastError();
    }
    return 0;
}

static void ShowFileLoadError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg )
{
    ResString rTitle(hRes, IDS_APP_TITLE);
    ResString rLoadErr(hRes, IDS_FAILEDTOLOADFILE);
    MessageBox(hWnd, CStringUtils::Format(rLoadErr, fileName.c_str(), msg).c_str(), (LPCWSTR)rTitle, MB_ICONERROR);
}

static void ShowFileSaveError(HWND hWnd, const std::wstring& fileName, LPCWSTR msg )
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

CDocument CDocumentManager::LoadFile( HWND hWnd, const std::wstring& path, int encoding, bool createIfMissing)
{
    CDocument doc;
    std::wstring sFileName = CPathUtils::GetFileName(path);

    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, createIfMissing ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        // Capture the access denied error, while it's valid.
        DWORD err = GetLastError();
        CFormatMessageWrapper errMsg(err);
        if (((err == ERROR_ACCESS_DENIED)||(err == ERROR_WRITE_PROTECT)) && (!SysInfo::Instance().IsElevated()))
        {
            if (AskToElevatePrivilegeForOpening(hWnd,path))
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
                    CFormatMessageWrapper errMsg(elevationError);
                    ShowFileLoadError(hWnd, path, errMsg);
                }
                // Exhausted all operations to work around the problem,
                // fall through to inform that what the final outcome is
                // which is the original error thy got.
            }
            // else if canceled elevation via various means or got an error even asking.
            // just fall through and issue the error that failed.
        }
        ShowFileLoadError(hWnd,sFileName,errMsg);
        return doc;
    }
    BY_HANDLE_FILE_INFORMATION fi = {0};
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        CFormatMessageWrapper errMsg; // Calls GetLastError itself.
        ShowFileLoadError(hWnd,sFileName,errMsg);
        return doc;
    }
    doc.m_bIsReadonly = (fi.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;
    doc.m_lastWriteTime = fi.ftLastWriteTime;
    doc.m_path = path;
    unsigned __int64 fileSize = static_cast<__int64>(fi.nFileSizeHigh) << 32 | fi.nFileSizeLow;
    // add more room for Scintilla (usually 1/6 more for editing)
    unsigned __int64 bufferSizeRequested = fileSize + min(1<<20,fileSize/6);


    // Setup our scratch scintilla control to load the data
    m_scratchScintilla.Call(SCI_SETSTATUS, SC_STATUS_OK);   // reset error status
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    bool ro = m_scratchScintilla.Call(SCI_GETREADONLY) != 0;
    if (ro)
        m_scratchScintilla.Call(SCI_SETREADONLY, false);    // we need write access
    m_scratchScintilla.Call(SCI_CLEARALL);
    //m_scratchScintilla.Call(SCI_SETLEXER, ScintillaEditView::langNames[language].lexerID);
    m_scratchScintilla.Call(SCI_SETCODEPAGE, CP_UTF8);
    std::wstring ext = CPathUtils::GetFileExtension(path);
    m_scratchScintilla.SetupLexerForExt(ext);

    const int blockSize = 128 * 1024;   //128 kB
    char data[blockSize+8];
    const int widebufSize = blockSize * 2;
    auto widebuf = std::make_unique<wchar_t[]>(widebufSize);
    const int charbufSize = widebufSize * 2;
    auto charbuf = std::make_unique<char[]>(charbufSize);

    // First allocate enough memory for the whole file (this will reduce memory copy during loading)
    m_scratchScintilla.Call(SCI_ALLOCATE, WPARAM(bufferSizeRequested));
    if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
    {
        ShowFileLoadError(hWnd,sFileName,
            CLanguage::Instance().GetTranslatedString(ResString(hRes, IDS_ERR_FILETOOBIG)).c_str());
        return doc;
    }
    DWORD lenFile = 0;
    int incompleteMultibyteChar = 0;
    bool bFirst = true;
    do
    {
        if (!ReadFile(hFile, data + incompleteMultibyteChar, blockSize - incompleteMultibyteChar, &lenFile, NULL))
        {
            lenFile = 0;
        }
        else
            lenFile += incompleteMultibyteChar;

        if (encoding == -1)
        {
            encoding = GetCodepageFromBuf(data, lenFile, doc.m_bHasBOM);
        }
        doc.m_encoding = encoding;
        LoadSome(encoding, m_scratchScintilla, doc, bFirst, lenFile, incompleteMultibyteChar,
                  data, charbuf.get(), charbufSize, widebuf.get() );

        if (doc.m_format == UNKNOWN_FORMAT)
            doc.m_format = GetEOLFormatForm(data, lenFile);
        if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
        {
            ShowFileLoadError(hWnd,sFileName,
                CLanguage::Instance().GetTranslatedString(ResString(hRes, IDS_ERR_FILETOOBIG)).c_str());
            break;
        }

        if (incompleteMultibyteChar != 0)
        {
            // copy bytes to next buffer
            memcpy(data, data + blockSize - incompleteMultibyteChar, incompleteMultibyteChar);
        }
        bFirst = false;
    } while (lenFile == blockSize);

    if (doc.m_format == UNKNOWN_FORMAT)
        doc.m_format = WIN_FORMAT;

    SetEOLType(m_scratchScintilla, doc);

    m_scratchScintilla.Call(SCI_EMPTYUNDOBUFFER);
    m_scratchScintilla.Call(SCI_SETSAVEPOINT);
    if (ro || doc.m_bIsReadonly)
        m_scratchScintilla.Call(SCI_SETREADONLY, true);
    doc.m_document = m_scratchScintilla.Call(SCI_GETDOCPOINTER);
    m_scratchScintilla.Call(SCI_ADDREFDOCUMENT, 0, doc.m_document);
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    return doc;
}

FormatType CDocumentManager::GetEOLFormatForm( const char *data, DWORD len ) const
{
    for (size_t i = 0 ; i < len ; i++)
    {
        if (data[i] == '\r')
        {
            if (i+1 < len && data[i+1] == '\n')
            {
                return WIN_FORMAT;
            }
            else
            {
                return MAC_FORMAT;
            }
        }
        if (data[i] == '\n')
        {
            return UNIX_FORMAT;
        }
    }
    return UNKNOWN_FORMAT;
}

bool CDocumentManager::SaveDoc( HWND hWnd, const std::wstring& path, const CDocument& doc )
{
    if (path.empty())
        return false;
    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        ShowFileSaveError(hWnd,path,errMsg);
        return false;
    }

    const int blockSize = 128 * 1024;   //128 kB
    const int widebufSize = blockSize * 2;
    auto widebuf = std::make_unique<wchar_t[]>(widebufSize);
    auto wide32buf = std::make_unique<wchar_t[]>(widebufSize*2);
    const int charbufSize = widebufSize * 2;
    auto charbuf = std::make_unique<char[]>(charbufSize);

    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, doc.m_document);
    size_t lengthDoc = m_scratchScintilla.Call(SCI_GETLENGTH);
    char* buf = (char*)m_scratchScintilla.Call(SCI_GETCHARACTERPOINTER);    // get characters directly from Scintilla buffer
    DWORD bytesWritten = 0;
    switch (doc.m_encoding)
    {
    case 1200: // UTF16_LE
    case 1201: // UTF16_BE
        {
            if (doc.m_bHasBOM)
            {
                if (doc.m_encoding == 1200)
                    WriteFile(hFile, "\xFF\xFE", 2, &bytesWritten, NULL);
                else
                    WriteFile(hFile, "\xFE\xFF", 2, &bytesWritten, NULL);
            }
            char * writeBuf = buf;
            do
            {
                int charStart = UTF8Helper::characterStart(writeBuf, (int)min(blockSize, lengthDoc));
                int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, widebuf.get(), widebufSize);
                if (doc.m_encoding == 1201)
                {
                    UINT64 * p_qw = (UINT64 *)(void *)widebuf.get();
                    int nQwords = widelen/4;
                    for (int nQword = 0; nQword<nQwords; nQword++)
                    {
                        p_qw[nQword] = WordSwapBytes(p_qw[nQword]);
                    }
                    wchar_t * p_w = (wchar_t *)p_qw;
                    int nWords = widelen;
                    for (int nWord = nQwords*4; nWord<nWords; nWord++)
                    {
                        p_w[nWord] = WideCharSwap(p_w[nWord]);
                    }
                }
                if ((!WriteFile(hFile, widebuf.get(), widelen*2, &bytesWritten, NULL) || (widelen != int(bytesWritten/2))))
                {
                    CFormatMessageWrapper errMsg;
                    ShowFileSaveError(hWnd,path,errMsg);
                    return false;
                }
                writeBuf += charStart;
                lengthDoc -= charStart;
            } while (lengthDoc > 0);
        }
        break;
    case 12000: // UTF32_LE
    case 12001: // UTF32_BE
        {
            if (doc.m_encoding == 12000)
                WriteFile(hFile, "\xFF\xFE\0\0", 4, &bytesWritten, NULL);
            else
                WriteFile(hFile, "\0\0\xFE\xFF", 4, &bytesWritten, NULL);
            char * writeBuf = buf;
            do
            {
                int charStart = UTF8Helper::characterStart(writeBuf, (int)min(blockSize, lengthDoc));
                int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, widebuf.get(), widebufSize);

                LPCWSTR p_In = (LPCWSTR)widebuf.get();
                UINT32 * p_Out = (UINT32 *)(void *)wide32buf.get();
                int nOutDword = 0;
                for (int nInWord = 0; nInWord<widelen; nInWord++, nOutDword++)
                {
                    UINT32 zChar = p_In[nInWord];
                    if ((zChar&0xfc00) == 0xd800) // lead surrogate
                    {
                        if (nInWord+1<widelen && (p_In[nInWord+1]&0xfc00) == 0xdc00) // trail surrogate follows
                        {
                            zChar = 0x10000 + ((zChar&0x3ff)<<10) + (p_In[++nInWord]&0x3ff);
                        }
                        else
                        {
                            zChar = 0xfffd; // ? mark
                        }
                    }
                    else if ((zChar&0xfc00) == 0xdc00) // trail surrogate without lead
                    {
                        zChar = 0xfffd; // ? mark
                    }
                    p_Out[nOutDword] = zChar;
                }


                if (doc.m_encoding == 12001)
                {
                    UINT64 * p64 = (UINT64 *)(void *)wide32buf.get();
                    int nQwords = widelen/2;
                    for (int nQword = 0; nQword<nQwords; nQword++)
                    {
                        p64[nQword] = DwordSwapBytes(p64[nQword]);
                    }

                    UINT32 * p32 = (UINT32 *)p64;
                    int nDwords = widelen;
                    for (int nDword = nQwords*2; nDword<nDwords; nDword++)
                    {
                        p32[nDword] = DwordSwapBytes(p32[nDword]);
                    }
                }
                if ((!WriteFile(hFile, wide32buf.get(), widelen*4, &bytesWritten, NULL) || (widelen != int(bytesWritten/4))))
                {
                    CFormatMessageWrapper errMsg;
                    ShowFileSaveError(hWnd,path,errMsg);
                    return false;
                }
                writeBuf += charStart;
                lengthDoc -= charStart;
            } while (lengthDoc > 0);
        }
        break;
    case CP_UTF8:
        // UTF8: save the buffer as it is
        if (doc.m_bHasBOM)
            WriteFile(hFile, "\xEF\xBB\xBF", 3, &bytesWritten, NULL);
        do
        {
            DWORD writeLen = (DWORD)min(blockSize, lengthDoc);
            if (!WriteFile(hFile, buf, writeLen, &bytesWritten, NULL))
            {
                CFormatMessageWrapper errMsg;
                ShowFileSaveError(hWnd,path,errMsg);
                return false;
            }
            lengthDoc -= writeLen;
            buf += writeLen;
        } while (lengthDoc > 0);
        break;
    default:
        {
            // first convert to wide char, then to the requested codepage
            char * writeBuf = buf;
            do
            {
                int charStart = UTF8Helper::characterStart(writeBuf, (int)min(blockSize, lengthDoc));
                int widelen = MultiByteToWideChar(CP_UTF8, 0, writeBuf, charStart, widebuf.get(), widebufSize);
                int charlen = WideCharToMultiByte(doc.m_encoding < 0 ? CP_ACP : doc.m_encoding, 0, widebuf.get(), widelen, charbuf.get(), charbufSize, 0, NULL);
                if ((!WriteFile(hFile, charbuf.get(), charlen, &bytesWritten, NULL) || (charlen != int(bytesWritten))))
                {
                    CFormatMessageWrapper errMsg;
                    ShowFileSaveError(hWnd,path,errMsg);
                    return false;
                }
                writeBuf += charStart;
                lengthDoc -= charStart;
            } while (lengthDoc > 0);
        }
        break;
    }
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
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
                hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
                        CFormatMessageWrapper errMsg(elevationError);
                        ShowFileSaveError(hWnd, doc.m_path, errMsg);
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
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            doc.m_lastWriteTime.dwLowDateTime  = 0;
            doc.m_lastWriteTime.dwHighDateTime = 0;
        }
        return false;
    }
    BY_HANDLE_FILE_INFORMATION fi = {0};
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        return false;
    }
    doc.m_lastWriteTime = fi.ftLastWriteTime;
    if (bIncludeReadonly)
        doc.m_bIsReadonly = (fi.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;
    return true;
}

DocModifiedState CDocumentManager::HasFileChanged( int id ) const
{
    CDocument doc = GetDocumentFromID(id);
    if (doc.m_path.empty() || ((doc.m_lastWriteTime.dwLowDateTime == 0) && (doc.m_lastWriteTime.dwHighDateTime == 0)) || doc.m_bDoSaveAs)
        return DM_Unmodified;

    // get the last write time of the base doc file
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return DM_Removed;
        return DM_Unknown;
    }
    BY_HANDLE_FILE_INFORMATION fi = {0};
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        return DM_Unknown;
    }
    if (CompareFileTime(&doc.m_lastWriteTime, &fi.ftLastWriteTime))
        return DM_Modified;

    return DM_Unmodified;
}

int CDocumentManager::GetIdForPath( const std::wstring& path ) const
{
    for (const auto& d : m_documents)
    {
        if (CPathUtils::PathCompare(d.second.m_path, path) == 0)
            return d.first;
    }
    return -1;
}

void CDocumentManager::RemoveDocument( int id )
{
    CDocument doc = GetDocumentFromID(id);
    m_scratchScintilla.Call(SCI_RELEASEDOCUMENT, 0, doc.m_document);
    m_documents.erase(id);
}

COLORREF CDocumentManager::GetColorForDocument( int id )
{
    CDocument doc = GetDocumentFromID(id);
    std::wstring folderpath = CPathUtils::GetParentDirectory(doc.m_path);
    CStringUtils::emplace_to_lower(folderpath);

    auto foundIt = m_foldercolorindexes.find(folderpath);
    if (foundIt != m_foldercolorindexes.end())
        return foldercolors[foundIt->second % MAX_FOLDERCOLORS];

    m_foldercolorindexes[folderpath] = m_lastfoldercolorindex;
    return foldercolors[m_lastfoldercolorindex++ % MAX_FOLDERCOLORS];
}
