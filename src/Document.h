#pragma once

typedef uptr_t Document;

enum FormatType { UNKNOWN_FORMAT, WIN_FORMAT, MAC_FORMAT, UNIX_FORMAT };

class CPosData
{
public:
    CPosData() 
        : m_nFirstVisibleLine(0)
        , m_nStartPos(0)
        , m_nEndPos(0)
        , m_xOffset(0)
        , m_nScrollWidth(1)
        , m_nSelMode(0)
    {};
    ~CPosData(){}

    size_t m_nFirstVisibleLine;
    size_t m_nStartPos;
    size_t m_nEndPos;
    size_t m_xOffset;
    size_t m_nSelMode;
    size_t m_nScrollWidth;
};

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
    CPosData                m_position;
};


