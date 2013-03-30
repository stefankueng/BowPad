#pragma once
#include "ScintillaWnd.h"

#include <vector>

typedef uptr_t Document;

enum FormatType { UNKNOWN_FORMAT, WIN_FORMAT, MAC_FORMAT, UNIX_FORMAT };

class CDocument
{
public:
    CDocument()
        : m_document(NULL)
        , m_format(UNKNOWN_FORMAT)
        , m_bHasBOM(false)
        , m_encoding(-1)
    {
    }


    Document                m_document;
    FormatType              m_format;
    bool                    m_bHasBOM;
    int                     m_encoding;
    std::wstring            m_path;
};

class CDocumentManager
{
public:
    CDocumentManager(void);
    ~CDocumentManager(void);

    void AddDocumentAtEnd(CDocument doc);
    size_t GetCount() const { return m_documents.size(); }
    size_t GetIndexForPath(const std::wstring& path) const;
    CDocument GetDocument(int index) const { return m_documents.at(index); }
    Document GetScintillaDocument(int index) const { return m_documents.at(index).m_document; }
    void ExchangeDocs(int src, int dst);

    CDocument LoadFile(HWND hWnd, const std::wstring& path, int encoding);
    bool SaveFile(HWND hWnd, const CDocument& doc);
private:
    FormatType GetEOLFormatForm(const char *data) const;

private:
    std::vector<CDocument>  m_documents;
    CScintillaWnd           m_scratchScintilla;
};

