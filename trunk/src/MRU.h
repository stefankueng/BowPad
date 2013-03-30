#pragma once
#include <string>
#include <vector>

class CMRU
{
private:
    CMRU(void);
    ~CMRU(void);

public:
    static CMRU& Instance();

    HRESULT                         PopulateRibbonRecentItems(PROPVARIANT* pvarValue);
    void                            AddPath(const std::wstring& path);
    void                            PinPath(const std::wstring& path, bool bPin);

private:
    void                            Load();
    void                            Save();
    std::wstring                    GetPath();

private:
    bool                            m_bLoaded;
    std::vector<std::tuple<std::wstring, bool>>     m_mruVec;
};

