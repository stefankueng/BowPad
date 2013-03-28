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
