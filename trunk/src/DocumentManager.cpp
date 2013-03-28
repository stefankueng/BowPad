#include "stdafx.h"
#include "DocumentManager.h"


CDocumentManager::CDocumentManager(void)
{
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
