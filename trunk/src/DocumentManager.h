#pragma once
#include "Scintilla.h"

#include <vector>

typedef uptr_t Document;

class CDocumentManager
{
public:
    CDocumentManager(void);
    ~CDocumentManager(void);

    void AddDocumentAtEnd(Document doc);
    size_t GetCount() const { return m_documents.size(); }
    Document GetDocument(int index) const { return m_documents.at(index); }

private:
    std::vector<Document>   m_documents;
};

