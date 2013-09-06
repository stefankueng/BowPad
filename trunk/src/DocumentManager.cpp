// This file is part of BowPad.
//
// Copyright (C) 2013 - Stefan Kueng
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

#include <algorithm>

COLORREF foldercolors[] = {
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

#define MAX_FOLDERCOLORS (_countof(foldercolors))

wchar_t inline WideCharSwap(wchar_t nValue)
{
    return (((nValue>> 8)) | (nValue << 8));
}

UINT64 inline WordSwapBytes(UINT64 nValue)
{
    return ((nValue&0xff00ff00ff00ff)<<8) | ((nValue>>8)&0xff00ff00ff00ff); // swap BYTESs in WORDs
}

UINT32 inline DwordSwapBytes(UINT32 nValue)
{
    UINT32 nRet = (nValue<<16) | (nValue>>16); // swap WORDs
    nRet = ((nRet&0xff00ff)<<8) | ((nRet>>8)&0xff00ff); // swap BYTESs in WORDs
    return nRet;
}

UINT64 inline DwordSwapBytes(UINT64 nValue)
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

void CDocumentManager::AddDocumentAtEnd( CDocument doc )
{
    m_documents.push_back(doc);
}

void CDocumentManager::ExchangeDocs( int src, int dst )
{
    if (src >= (int)m_documents.size())
        return;
    if (dst >= (int)m_documents.size())
        return;

    CDocument srcDoc = m_documents[src];
    CDocument dstDoc = m_documents[dst];
    m_documents[src] = dstDoc;
    m_documents[dst] = srcDoc;
}

CDocument CDocumentManager::LoadFile( HWND hWnd, const std::wstring& path, int encoding)
{
    CDocument doc;
    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
        return doc;
    }
    BY_HANDLE_FILE_INFORMATION fi = {0};
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        CFormatMessageWrapper errMsg;
        MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
        return doc;
    }
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
    std::wstring ext = path.substr(path.find_last_of('.')+1);
    m_scratchScintilla.SetupLexerForExt(ext);

    bool success = true;
    const int blockSize = 128 * 1024;   //128 kB
    char data[blockSize+8];
    char * pData = data;
    const int widebufSize = blockSize * 2;
    std::unique_ptr<wchar_t[]> widebuf(new wchar_t[widebufSize]);
    const int charbufSize = widebufSize * 2;
    std::unique_ptr<char[]> charbuf(new char[charbufSize]);

    // First allocate enough memory for the whole file (this will reduce memory copy during loading)
    m_scratchScintilla.Call(SCI_ALLOCATE, WPARAM(bufferSizeRequested));
    if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
    {
        MessageBox(hWnd, CLanguage::Instance().GetTranslatedString(ResString(hRes, IDS_ERR_FILETOOBIG)).c_str(), L"BowPad", MB_ICONERROR);
        return doc;
    }
    DWORD lenFile = 0;
    int incompleteMultibyteChar = 0;
    bool bFirst = true;
    do
    {
        if (!ReadFile(hFile, data + incompleteMultibyteChar, blockSize - incompleteMultibyteChar, &lenFile, NULL))
        {
            success = false;
            lenFile = 0;
        }
        else
            lenFile += incompleteMultibyteChar;

        if (encoding == -1)
        {
            encoding = GetCodepageFromBuf(data, lenFile, doc.m_bHasBOM);
        }
        doc.m_encoding = encoding;
        if ((encoding == CP_UTF8) || (encoding == -1))
        {
            // Nothing to convert, just pass it to Scintilla
            if (bFirst && doc.m_bHasBOM)
            {
                pData += 3;
                lenFile -= 3;
            }
            m_scratchScintilla.Call(SCI_APPENDTEXT, lenFile, (LPARAM)pData);
            if (bFirst && doc.m_bHasBOM)
                lenFile += 3;
        }
        else
        {
            int wideLen = 0;
            switch (encoding)
            {
            case 1200: // UTF16_LE
                {
                    if (bFirst && doc.m_bHasBOM)
                    {
                        pData += 2;
                        lenFile -= 2;
                    }
                    memcpy(widebuf.get(), pData, lenFile);
                    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf.get(), lenFile/2, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
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
                    memcpy(widebuf.get(), pData, lenFile);
                    // make in place WORD BYTEs swap
                    UINT64 * p_qw = (UINT64 *)(void *)widebuf.get();
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
                    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf.get(), lenFile/2, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
                    if (bFirst && doc.m_bHasBOM)
                        lenFile += 2;
                }
                break;
            case 12001: // UTF32_BE
                {
                    UINT64 * p64 = (UINT64 *)(void *)data;
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
                    UINT32 * p32 = (UINT32 *)(void *)pData;

                    // fill buffer
                    wchar_t * pOut = (wchar_t *)widebuf.get();
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
                    int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf.get(), nReadChars, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
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
                MultiByteToWideChar(encoding, 0, data, lenFile-incompleteMultibyteChar, widebuf.get(), wideLen);
                int charlen = WideCharToMultiByte(CP_UTF8, 0, widebuf.get(), wideLen, charbuf.get(), charbufSize, 0, NULL);
                m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
            }
        }

        if (doc.m_format == UNKNOWN_FORMAT)
            doc.m_format = GetEOLFormatForm(data);
        if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
        {
            MessageBox(hWnd, CLanguage::Instance().GetTranslatedString(ResString(hRes, IDS_ERR_FILETOOBIG)).c_str(), L"BowPad", MB_ICONERROR);
            success = false;
            break;
        }

        if (incompleteMultibyteChar != 0)
        {
            // copy bytes to next buffer
            memcpy(data, data + blockSize - incompleteMultibyteChar, incompleteMultibyteChar);
        }
        bFirst = false;
        pData = data;
    } while (lenFile == blockSize);

    if (doc.m_format == UNKNOWN_FORMAT)
        doc.m_format = WIN_FORMAT;
    switch (doc.m_format)
    {
    case WIN_FORMAT:
        m_scratchScintilla.Call(SCI_SETEOLMODE, SC_EOL_CRLF);
        break;
    case UNIX_FORMAT:
        m_scratchScintilla.Call(SCI_SETEOLMODE, SC_EOL_LF);
        break;
    case MAC_FORMAT:
        m_scratchScintilla.Call(SCI_SETEOLMODE, SC_EOL_CR);
        break;
    default:
        break;
    }
    m_scratchScintilla.Call(SCI_EMPTYUNDOBUFFER);
    m_scratchScintilla.Call(SCI_SETSAVEPOINT);
    if (ro)
        m_scratchScintilla.Call(SCI_SETREADONLY, true);
    doc.m_document = m_scratchScintilla.Call(SCI_GETDOCPOINTER);
    m_scratchScintilla.Call(SCI_ADDREFDOCUMENT, 0, doc.m_document);
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    return doc;
}

FormatType CDocumentManager::GetEOLFormatForm( const char *data ) const
{
    size_t len = strlen(data);
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

bool CDocumentManager::SaveFile( HWND hWnd, const CDocument& doc )
{
    if (doc.m_path.empty())
        return false;
    CAutoFile hFile = CreateFile(doc.m_path.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
        return false;
    }

    const int blockSize = 128 * 1024;   //128 kB
    const int widebufSize = blockSize * 2;
    std::unique_ptr<wchar_t[]> widebuf(new wchar_t[widebufSize]);
    std::unique_ptr<wchar_t[]> wide32buf(new wchar_t[widebufSize*2]);
    const int charbufSize = widebufSize * 2;
    std::unique_ptr<char[]> charbuf(new char[charbufSize]);

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
                    MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
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
                    MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
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
                MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
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
                    MessageBox(hWnd, errMsg, L"BowPad", MB_ICONERROR);
                    return false;
                }
                writeBuf += charStart;
                lengthDoc -= charStart;
            } while (lengthDoc > 0);
        }
        break;
    }
    m_scratchScintilla.Call(SCI_SETSAVEPOINT);
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    return true;
}

bool CDocumentManager::UpdateFileTime( CDocument& doc )
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
    return true;
}

DocModifiedState CDocumentManager::HasFileChanged( int index )
{
    CDocument doc = GetDocument(index);
    if (doc.m_path.empty() || ((doc.m_lastWriteTime.dwLowDateTime==0)&&(doc.m_lastWriteTime.dwHighDateTime==0)))
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

int CDocumentManager::GetIndexForPath( const std::wstring& path ) const
{
    for (size_t i = 0; i < m_documents.size(); ++i)
    {
        if (_wcsicmp(m_documents[i].m_path.c_str(), path.c_str()) == 0)
            return (int)i;
    }
    return (int)-1;
}

void CDocumentManager::RemoveDocument( int index )
{
    CDocument doc = GetDocument(index);
    m_scratchScintilla.Call(SCI_RELEASEDOCUMENT, 0, doc.m_document);
    m_documents.erase(m_documents.begin() + index);
}

COLORREF CDocumentManager::GetColorForDocument( int index )
{
    CDocument doc = GetDocument(index);
    std::wstring folderpath = doc.m_path.substr(0, doc.m_path.find_last_of('\\')+1);
    std::transform(folderpath.begin(), folderpath.end(), folderpath.begin(), ::tolower);
    auto foundIt = m_foldercolorindexes.find(folderpath);
    if (foundIt != m_foldercolorindexes.end())
    {
        return foldercolors[foundIt->second % MAX_FOLDERCOLORS];
    }
    m_foldercolorindexes[folderpath] = m_lastfoldercolorindex;
    return foldercolors[m_lastfoldercolorindex++ % MAX_FOLDERCOLORS];
}
