#include "stdafx.h"
#include "RibbonNotepad.h"
#include "DocumentManager.h"
#include "SmartHandle.h"
#include "UnicodeUtils.h"


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
    : m_scratchScintilla(hInst)
{
    m_scratchScintilla.InitScratch(hInst);
}


CDocumentManager::~CDocumentManager(void)
{
}

void CDocumentManager::AddDocumentAtEnd( Document doc )
{
    m_documents.push_back(doc);
}

void CDocumentManager::ExchangeDocs( int src, int dst )
{
    if (src >= m_documents.size())
        return;
    if (dst >= m_documents.size())
        return;

    Document srcDoc = m_documents[src];
    Document dstDoc = m_documents[dst];
    m_documents[src] = dstDoc;
    m_documents[dst] = srcDoc;
}

Document CDocumentManager::LoadFile( HWND hWnd, const std::wstring& path, int & encoding, bool & hasBOM, FormatType * pFormat )
{
    hasBOM = false;
    CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!hFile.IsValid())
    {
        CFormatMessageWrapper errMsg;
        MessageBox(hWnd, errMsg, L"RibbonNotepad", MB_ICONERROR);
        return NULL;
    }
    BY_HANDLE_FILE_INFORMATION fi = {0};
    if (!GetFileInformationByHandle(hFile, &fi))
    {
        CFormatMessageWrapper errMsg;
        MessageBox(hWnd, errMsg, L"RibbonNotepad", MB_ICONERROR);
        return NULL;
    }
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
    m_scratchScintilla.Call(SCI_SETCODEPAGE, SC_CP_UTF8);

    bool success = true;
    int format = -1;
    const int blockSize = 128 * 1024;   //128 kB
    char data[blockSize+8];
    const int widebufSize = blockSize * 2;
    std::unique_ptr<wchar_t[]> widebuf(new wchar_t[widebufSize]);
    const int charbufSize = widebufSize * 2;
    std::unique_ptr<char[]> charbuf(new char[charbufSize]);

    // First allocate enough memory for the whole file (this will reduce memory copy during loading)
    m_scratchScintilla.Call(SCI_ALLOCATE, WPARAM(bufferSizeRequested));
    if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
    {
        MessageBox(hWnd, CLanguage::Instance().GetTranslatedString(ResString(hInst, IDS_ERR_FILETOOBIG)).c_str(), L"RibbonNotepad", MB_ICONERROR);
        return NULL;
    }
    DWORD lenFile = 0;
    size_t lenConvert = 0;  //just in case conversion results in 0, but file not empty
    bool isFirstTime = true;
    int incompleteMultibyteChar = 0;

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
            encoding = GetCodepageFromBuf(data, lenFile, hasBOM);
        }

        if (encoding == SC_CP_UTF8)
        {
            // Nothing to convert, just pass it to Scintilla
            m_scratchScintilla.Call(SCI_APPENDTEXT, lenFile, (LPARAM)data);
        }
        else
        {
            int wideLen = 0;
            switch (encoding)
            {
            case CP_UTF8:
                {
                    int indexOfLastChar = UTF8Helper::characterStart(data, lenFile-1); // get index of last character
                    if (indexOfLastChar != 0 && !UTF8Helper::isValid(data+indexOfLastChar, lenFile-indexOfLastChar))
                    {
                        // if it is not valid we do not process it right now
                        // (unless its the only character in string, to ensure that we always progress, e.g. that bytesNotProcessed < lenFile)
                        incompleteMultibyteChar = lenFile-indexOfLastChar;
                    }
                    wideLen = MultiByteToWideChar(encoding, 0, data, lenFile-incompleteMultibyteChar, NULL, 0);
                }
                break;
            case 1200: // UTF16_LE
                {
                    memcpy(widebuf.get(), data, lenFile);
                    int charlen = WideCharToMultiByte(SC_CP_UTF8, 0, widebuf.get(), lenFile/2, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
                }
                break;
            case 1201: // UTF16_BE
                {
                    memcpy(widebuf.get(), data, lenFile);
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
                    int charlen = WideCharToMultiByte(SC_CP_UTF8, 0, widebuf.get(), lenFile/2, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
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
                    // UTF32 have four bytes per char
                    int nReadChars = lenFile/4;
                    UINT32 * p32 = (UINT32 *)(void *)data;

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
                    int charlen = WideCharToMultiByte(SC_CP_UTF8, 0, widebuf.get(), lenFile/2, charbuf.get(), charbufSize, 0, NULL);
                    m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
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
                int charlen = WideCharToMultiByte(SC_CP_UTF8, 0, widebuf.get(), wideLen, charbuf.get(), charbufSize, 0, NULL);
                m_scratchScintilla.Call(SCI_APPENDTEXT, charlen, (LPARAM)charbuf.get());
            }
        }

        if (format == -1)
            format = getEOLFormatForm(data);
        if (m_scratchScintilla.Call(SCI_GETSTATUS) != SC_STATUS_OK)
        {
            MessageBox(hWnd, CLanguage::Instance().GetTranslatedString(ResString(hInst, IDS_ERR_FILETOOBIG)).c_str(), L"RibbonNotepad", MB_ICONERROR);
            success = false;
            break;
        }

        if (incompleteMultibyteChar != 0)
        {
            // copy bytes to next buffer
            memcpy(data, data + blockSize - incompleteMultibyteChar, incompleteMultibyteChar);
        }

    } while (lenFile == blockSize);

    if (pFormat != NULL)
    {
        *pFormat = (format == -1)?WIN_FORMAT:(FormatType)format;
    }
    m_scratchScintilla.Call(SCI_EMPTYUNDOBUFFER);
    m_scratchScintilla.Call(SCI_SETSAVEPOINT);
    if (ro)
        m_scratchScintilla.Call(SCI_SETREADONLY, true);
    Document doc = m_scratchScintilla.Call(SCI_GETDOCPOINTER);
    m_scratchScintilla.Call(SCI_ADDREFDOCUMENT, 0, doc);
    m_scratchScintilla.Call(SCI_SETDOCPOINTER, 0, 0);
    return doc;
}

int CDocumentManager::getEOLFormatForm( const char *data ) const
{
    size_t len = strlen(data);
    for (size_t i = 0 ; i < len ; i++)
    {
        if (data[i] == '\r')
        {
            if (i+1 < len &&  data[i+1] == '\n')
            {
                return int(WIN_FORMAT);
            }
            else
            {
                return int(MAC_FORMAT);
            }
        }
        if (data[i] == '\n')
        {
            return int(UNIX_FORMAT);
        }
    }
    return -1;
}
