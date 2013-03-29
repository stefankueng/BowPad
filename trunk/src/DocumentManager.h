#pragma once
#include "ScintillaWnd.h"

#include <vector>

typedef uptr_t Document;

enum FormatType { WIN_FORMAT, MAC_FORMAT, UNIX_FORMAT };


class CDocumentManager
{
public:
    CDocumentManager(void);
    ~CDocumentManager(void);

    void AddDocumentAtEnd(Document doc);
    size_t GetCount() const { return m_documents.size(); }
    Document GetDocument(int index) const { return m_documents.at(index); }
    void ExchangeDocs(int src, int dst);

    Document LoadFile(HWND hWnd, const std::wstring& path, int & encoding, bool & hasBOM, FormatType * pFormat);
private:
    int getEOLFormatForm(const char *data) const;

private:
    std::vector<Document>   m_documents;
    CScintillaWnd           m_scratchScintilla;
};

