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
#include "AppUtils.h"
#include "BowPad.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "SmartHandle.h"
#include "ProgressDlg.h"
#include "version.h"

#include <memory>
#include <time.h>
#include <ctime>
#include <fstream>

#include <Commctrl.h>
#include <Shellapi.h>

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Shell32.lib")

extern IUIFramework *g_pFramework;  // Reference to the Ribbon framework.


std::wstring CAppUtils::updatefilename;
std::wstring CAppUtils::updateurl;

bool CAppUtils::dark = false;


class CCallback : public IBindStatusCallback  
{
public:
    CCallback() : m_pDlg(nullptr) {}
    ~CCallback() {}

    // Pointer to the download progress dialog.
    CProgressDlg* m_pDlg;

    // IBindStatusCallback methods.  Note that the only method called by IE
    // is OnProgress(), so the others just return E_NOTIMPL.

    STDMETHOD(OnStartBinding)(
        /* [in] */ DWORD /*dwReserved*/,
        /* [in] */ IBinding __RPC_FAR * /*pib*/)
    { return E_NOTIMPL; }

    STDMETHOD(GetPriority)(
        /* [out] */ LONG __RPC_FAR * /*pnPriority*/)
    { return E_NOTIMPL; }

    STDMETHOD(OnLowResource)(
        /* [in] */ DWORD /*reserved*/)
    { return E_NOTIMPL; }

    STDMETHOD(OnProgress)(
        /* [in] */ ULONG ulProgress,
        /* [in] */ ULONG ulProgressMax,
        /* [in] */ ULONG /*ulStatusCode*/,
        /* [in] */ LPCWSTR /*wszStatusText*/)
    {
        if (m_pDlg->HasUserCancelled())
            return E_ABORT;

        static TCHAR   szAmtDownloaded[256], szTotalSize[256];

        StrFormatByteSize(ulProgress, szAmtDownloaded, 256);
        StrFormatByteSize(ulProgressMax, szTotalSize, 256);

        std::wstring sLine;
        if (ulProgressMax)
        {
            m_pDlg->SetProgress(ulProgress, ulProgressMax);
            sLine = CStringUtils::Format(L"%s of %s downloaded", szAmtDownloaded, szTotalSize);
        }
        else
            sLine = CStringUtils::Format(L"%s downloaded", szAmtDownloaded);
        m_pDlg->SetLine(1, sLine.c_str());

        return S_OK;
    }

    STDMETHOD(OnStopBinding)(
        /* [in] */ HRESULT /*hresult*/,
        /* [unique][in] */ LPCWSTR /*szError*/)
    { return E_NOTIMPL; }

    STDMETHOD(GetBindInfo)(
        /* [out] */ DWORD __RPC_FAR * /*grfBINDF*/,
        /* [unique][out][in] */ BINDINFO __RPC_FAR * /*pbindinfo*/)
    { return E_NOTIMPL; }

    STDMETHOD(OnDataAvailable)(
        /* [in] */ DWORD /*grfBSCF*/,
        /* [in] */ DWORD /*dwSize*/,
        /* [in] */ FORMATETC __RPC_FAR * /*pformatetc*/,
        /* [in] */ STGMEDIUM __RPC_FAR * /*pstgmed*/)
    { return E_NOTIMPL; }

    STDMETHOD(OnObjectAvailable)(
        /* [in] */ REFIID /*riid*/,
        /* [iid_is][in] */ IUnknown __RPC_FAR * /*punk*/)
    { return E_NOTIMPL; }

    // IUnknown methods.  Note that IE never calls any of these methods, since
    // the caller owns the IBindStatusCallback interface, so the methods all
    // return zero/E_NOTIMPL.

    STDMETHOD_(ULONG,AddRef)()
    { return 0; }

    STDMETHOD_(ULONG,Release)()
    { return 0; }

    STDMETHOD(QueryInterface)(
        /* [in] */ REFIID /*riid*/,
        /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR * /*ppvObject*/)
    { return E_NOTIMPL; }
};



CAppUtils::CAppUtils(void)
{
}


CAppUtils::~CAppUtils(void)
{
}

std::wstring CAppUtils::GetDataPath(HMODULE hMod)
{
    DWORD len = 0;
    DWORD bufferlen = MAX_PATH;     // MAX_PATH is not the limit here!
    std::unique_ptr<wchar_t[]> path(new wchar_t[bufferlen]);
    do
    {
        bufferlen += MAX_PATH;      // MAX_PATH is not the limit here!
        path = std::unique_ptr<wchar_t[]>(new wchar_t[bufferlen]);
        len = GetModuleFileName(hMod, path.get(), bufferlen);
    } while(len == bufferlen);
    std::wstring sPath = path.get();
    sPath = sPath.substr(0, sPath.find_last_of('\\'));
    return sPath;
}

std::wstring CAppUtils::GetSessionID()
{
    std::wstring t;
    CAutoGeneralHandle token;
    BOOL result = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.GetPointer());
    if(result)
    {
        DWORD len = 0;
        GetTokenInformation(token, TokenStatistics, NULL, 0, &len);
        if (len >= sizeof (TOKEN_STATISTICS))
        {
            std::unique_ptr<BYTE[]> data (new BYTE[len]);
            GetTokenInformation(token, TokenStatistics, data.get(), len, &len);
            LUID uid = ((PTOKEN_STATISTICS)data.get())->AuthenticationId;
            t = CStringUtils::Format(L"-%08x%08x", uid.HighPart, uid.LowPart);
        }
    }
    return t;
}

bool CAppUtils::CheckForUpdate(bool force)
{
    bool bNewerAvailable = false;
    // check for newer versions
    if (CIniSettings::Instance().GetInt64(L"updatecheck", L"auto", TRUE) != FALSE)
    {
        time_t now;
        time(&now);
        time_t last = CIniSettings::Instance().GetInt64(L"updatecheck", L"last", 0);
        double days = std::difftime(now, last) / (60 * 60 * 24);
        if ((days >= 7.0) || force)
        {
            std::wstring tempfile = CPathUtils::GetTempFilePath();

            std::wstring sCheckURL = L"https://bowpad.googlecode.com/svn/trunk/version.txt";
            HRESULT res = URLDownloadToFile(NULL, sCheckURL.c_str(), tempfile.c_str(), 0, NULL);
            if (res == S_OK)
            {
                CIniSettings::Instance().SetInt64(L"updatecheck", L"last", now);
                std::ifstream File;
                File.open(tempfile.c_str());
                if (File.good())
                {
                    char line[1024];
                    char * pLine = line;
                    File.getline(line, sizeof(line));
                    int major = 0;
                    int minor = 0;
                    int micro = 0;
                    int build = 0;

                    major = atoi(pLine);
                    pLine = strchr(pLine, '.');
                    if (pLine)
                    {
                        pLine++;
                        minor = atoi(pLine);
                        pLine = strchr(pLine, '.');
                        if (pLine)
                        {
                            pLine++;
                            micro = atoi(pLine);
                            pLine = strchr(pLine, '.');
                            if (pLine)
                            {
                                pLine++;
                                build = atoi(pLine);
                            }
                        }
                    }
                    if (major > BP_VERMAJOR)
                        bNewerAvailable = true;
                    else if ((minor > BP_VERMINOR)&&(major == BP_VERMAJOR))
                        bNewerAvailable = true;
                    else if ((micro > BP_VERMICRO)&&(minor == BP_VERMINOR)&&(major == BP_VERMAJOR))
                        bNewerAvailable = true;
                    else if ((build > BP_VERBUILD)&&(micro == BP_VERMICRO)&&(minor == BP_VERMINOR)&&(major == BP_VERMAJOR))
                        bNewerAvailable = true;
#ifdef _WIN64
                    File.getline(line, sizeof(line));
                    updateurl = CUnicodeUtils::StdGetUnicode(line);
                    File.getline(line, sizeof(line));
                    updatefilename = CUnicodeUtils::StdGetUnicode(line);
#else
                    // first two lines are for the 64-bit version
                    File.getline(line, sizeof(line));
                    File.getline(line, sizeof(line));
                    // now for the 32-bit version
                    File.getline(line, sizeof(line));
                    updateurl = CUnicodeUtils::StdGetUnicode(line);
                    File.getline(line, sizeof(line));
                    updatefilename = CUnicodeUtils::StdGetUnicode(line);
#endif
                }
                File.close();
            }
            DeleteFile(tempfile.c_str());
        }
    }
    return bNewerAvailable;
}

HRESULT CALLBACK CAppUtils::TDLinkClickCallback(HWND hwnd, UINT uNotification, WPARAM /*wParam*/, LPARAM lParam, LONG_PTR /*dwRefData*/)
{
    switch (uNotification)
    {
    case TDN_HYPERLINK_CLICKED:
        ShellExecute(hwnd, _T("open"), (LPCWSTR) lParam, NULL, NULL, SW_SHOW);
        break;
    }

    return S_OK;
}

bool CAppUtils::DownloadUpdate(HWND hWnd, bool bInstall)
{
    if (updatefilename.empty() || updateurl.empty())
        return false;

    PWSTR downloadpath = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &downloadpath);
    if (SUCCEEDED(hr))
    {
        std::wstring sDownloadFile = downloadpath;
        CoTaskMemFree((LPVOID)downloadpath);
        sDownloadFile += L"\\";
        sDownloadFile += updatefilename;

        CCallback callback;
        CProgressDlg progDlg;
        progDlg.SetTitle(L"BowPad Update");
        progDlg.SetLine(1, L"Downloading BowPad Update...");
        progDlg.ResetTimer();
        progDlg.SetTime();
        progDlg.ShowModal(hWnd);
        callback.m_pDlg = &progDlg;

        hr = URLDownloadToFile(NULL, updateurl.c_str(), sDownloadFile.c_str(), 0, &callback);
        if (SUCCEEDED(hr))
        {
            if (bInstall)
            {
                ::ShellExecute(hWnd, L"open", sDownloadFile.c_str(), NULL, NULL, SW_SHOW);
            }
            else
            {
                // open explorer with the downloaded file selected
                PCIDLIST_ABSOLUTE __unaligned pidl = ILCreateFromPath(sDownloadFile.c_str());
                if (pidl)
                {
                    SHOpenFolderAndSelectItems(pidl,0,0,0);
                    CoTaskMemFree((LPVOID)pidl);
                }

            }
        }
    }

    return SUCCEEDED(hr);
}

bool CAppUtils::ShowUpdateAvailableDialog( HWND hWnd )
{
    HRESULT hr;
    TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
    int nClickedBtn;
    BOOL bCheckboxChecked;
    TASKDIALOG_BUTTON aCustomButtons[] =
    {
        { 1000, MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_DOWNLOAD_INSTALL_BTN_TEXT) },
        { 1001, MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_DOWNLOAD_BTN_TEXT) },
        { 1002, MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_DONT_DOWNLOAD_BTN_TEXT) }
    };

    tdc.hwndParent = hWnd;
    tdc.hInstance = hInst;
    tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pButtons = aCustomButtons;
    tdc.cButtons = _countof(aCustomButtons);
    tdc.pszWindowTitle = MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_TASKDLG_TITLE);
    tdc.pszMainIcon = MAKEINTRESOURCE(IDI_BOWPAD);
    tdc.pszMainInstruction = MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_TASKDLG_HEADER);
    tdc.nDefaultButton = 1002;
    tdc.pszVerificationText = MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_TASKDLG_CHKBOX_TEXT);
    tdc.pszFooter = MAKEINTRESOURCE(IDS_UPDATEAVAILABLE_TASKDLG_FOOTER);
    tdc.pszFooterIcon =  TD_INFORMATION_ICON;
    tdc.pfCallback = CAppUtils::TDLinkClickCallback;

    hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
    if (SUCCEEDED(hr))
    {
        if ((nClickedBtn == 1000)||(nClickedBtn == 1001))
        {
            CAppUtils::DownloadUpdate(hWnd, nClickedBtn==1000);
        }
        if (bCheckboxChecked)
            CIniSettings::Instance().SetInt64(L"updatecheck", L"auto", FALSE);
        return true;
    }
    return false;
}

void CAppUtils::RGBToHSB( COLORREF rgb, BYTE& hue, BYTE& saturation, BYTE& brightness )
{
    double minRGB = min(min(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    double maxRGB = max(max(GetRValue(rgb), GetGValue(rgb)), GetBValue(rgb));
    double delta = maxRGB - minRGB;
    double b = maxRGB;
    double s = 0.0;
    double h = 0.0;
    if (maxRGB)
        s = 255 * delta / maxRGB;

    if (s != 0.0)
    {
        if (GetRValue(rgb) == maxRGB)
            h = (double(GetGValue(rgb)) - double(GetBValue(rgb))) / delta;
        else
        {
            if (GetGValue(rgb) == maxRGB)
                h = 2.0 + (double(GetBValue(rgb)) - double(GetRValue(rgb))) / delta;
            else if (GetBValue(rgb) == maxRGB)
                h = 4.0 + (double(GetRValue(rgb)) - double(GetGValue(rgb))) / delta;
        }
    }
    else
        h = -1.0;
    h = h * 60.0;
    if (h < 0.0)
        h = h + 360.0;

    hue         = BYTE(h);
    saturation  = BYTE(s * 100.0 / 255.0);
    brightness  = BYTE(b * 100.0 / 255.0);
}

void CAppUtils::RGBtoHSL(COLORREF color, unsigned int& h, unsigned int& s, unsigned int& l)
{
    UINT r = GetRValue(color);
    UINT g = GetGValue(color);
    UINT b = GetBValue(color);

    float r_percent = ((float)r)/255;
    float g_percent = ((float)g)/255;
    float b_percent = ((float)b)/255;

    float max_color = 0;
    if((r_percent >= g_percent) && (r_percent >= b_percent))
    {
        max_color = r_percent;
    }
    if((g_percent >= r_percent) && (g_percent >= b_percent))
        max_color = g_percent;
    if((b_percent >= r_percent) && (b_percent >= g_percent))
        max_color = b_percent;

    float min_color = 0;
    if((r_percent <= g_percent) && (r_percent <= b_percent))
        min_color = r_percent;
    if((g_percent <= r_percent) && (g_percent <= b_percent))
        min_color = g_percent;
    if((b_percent <= r_percent) && (b_percent <= g_percent))
        min_color = b_percent;

    float L = 0;
    float S = 0;
    float H = 0;

    L = (max_color + min_color)/2;

    if(max_color == min_color)
    {
        S = 0;
        H = 0;
    }
    else
    {
        if(L < .50)
        {
            S = (max_color - min_color)/(max_color + min_color);
        }
        else
        {
            S = (max_color - min_color)/(2 - max_color - min_color);
        }
        if(max_color == r_percent)
        {
            H = (g_percent - b_percent)/(max_color - min_color);
        }
        if(max_color == g_percent)
        {
            H = 2 + (b_percent - r_percent)/(max_color - min_color);
        }
        if(max_color == b_percent)
        {
            H = 4 + (r_percent - g_percent)/(max_color - min_color);
        }
    }
    s = (unsigned int)(S*100);
    l = (unsigned int)(L*100);
    H = H*60;
    if(H < 0)
        H += 360;
    h = (unsigned int)H;
}

static void HSLtoRGB_Subfunction(unsigned int& c, const float& temp1, const float& temp2, const float& temp3)
{
    if((temp3 * 6) < 1)
        c = (unsigned int)((temp2 + (temp1 - temp2)*6*temp3)*100);
    else
        if((temp3 * 2) < 1)
            c = (unsigned int)(temp1*100);
        else
            if((temp3 * 3) < 2)
                c = (unsigned int)((temp2 + (temp1 - temp2)*(.66666 - temp3)*6)*100);
            else
                c = (unsigned int)(temp2*100);
    return;
}

COLORREF CAppUtils::HSLtoRGB(const unsigned int& h, const unsigned int& s, const unsigned int& l)
{
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;

    float L = ((float)l)/100;
    float S = ((float)s)/100;
    float H = ((float)h)/360;

    if(s == 0)
    {
        r = l;
        g = l;
        b = l;
    }
    else
    {
        float temp1 = 0;
        if(L < .50)
        {
            temp1 = L*(1 + S);
        }
        else
        {
            temp1 = L + S - (L*S);
        }

        float temp2 = 2*L - temp1;

        float temp3 = 0;
        for(int i = 0 ; i < 3 ; i++)
        {
            switch(i)
            {
            case 0: // red
                {
                    temp3 = H + .33333f;
                    if(temp3 > 1)
                        temp3 -= 1;
                    HSLtoRGB_Subfunction(r,temp1,temp2,temp3);
                    break;
                }
            case 1: // green
                {
                    temp3 = H;
                    HSLtoRGB_Subfunction(g,temp1,temp2,temp3);
                    break;
                }
            case 2: // blue
                {
                    temp3 = H - .33333f;
                    if(temp3 < 0)
                        temp3 += 1;
                    HSLtoRGB_Subfunction(b,temp1,temp2,temp3);
                    break;
                }
            default:
                {

                }
            }
        }
    }
    r = (unsigned int)((((float)r)/100)*255);
    g = (unsigned int)((((float)g)/100)*255);
    b = (unsigned int)((((float)b)/100)*255);
    return RGB(r,g,b);
}

COLORREF CAppUtils::GetThemeColor( COLORREF clr )
{
    if (dark)
    {
        unsigned int h;
        unsigned int s;
        unsigned int l;
        RGBtoHSL(clr, h, s, l);
        l = 100 - l;
        return HSLtoRGB(h, s, l);
    }

    return clr;
}

void CAppUtils::SetRibbonColors( COLORREF text, COLORREF background, COLORREF highlight )
{
    CComPtr<IPropertyStore> spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned 
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        // UI_HSBCOLOR is a type defined in UIRibbon.h that is composed of 
        // three component values: hue, saturation and brightness, respectively.
        BYTE h, s, b;
        RGBToHSB(text, h, s, b);
        UI_HSBCOLOR TextColor = UI_HSB(h, s, b);
        RGBToHSB(background, h, s, b);
        UI_HSBCOLOR BackgroundColor = UI_HSB(h, s, b);
        RGBToHSB(highlight, h, s, b);
        UI_HSBCOLOR HighlightColor = UI_HSB(h, s, b);

        InitPropVariantFromUInt32(BackgroundColor, &propvarBackground);
        InitPropVariantFromUInt32(HighlightColor, &propvarHighlight);
        InitPropVariantFromUInt32(TextColor, &propvarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propvarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propvarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propvarText);

        spPropertyStore->Commit();
    }
}

void CAppUtils::SetRibbonColorsHSB( UI_HSBCOLOR text, UI_HSBCOLOR background, UI_HSBCOLOR highlight )
{
    CComPtr<IPropertyStore> spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned 
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        InitPropVariantFromUInt32(background, &propvarBackground);
        InitPropVariantFromUInt32(highlight, &propvarHighlight);
        InitPropVariantFromUInt32(text, &propvarText);

        spPropertyStore->SetValue(UI_PKEY_GlobalBackgroundColor, propvarBackground);
        spPropertyStore->SetValue(UI_PKEY_GlobalHighlightColor, propvarHighlight);
        spPropertyStore->SetValue(UI_PKEY_GlobalTextColor, propvarText);

        spPropertyStore->Commit();
    }
}

void CAppUtils::GetRibbonColors( UI_HSBCOLOR& text, UI_HSBCOLOR& background, UI_HSBCOLOR& highlight )
{
    CComPtr<IPropertyStore> spPropertyStore;

    // g_pFramework is a pointer to the IUIFramework interface that is assigned 
    // when the Ribbon is initialized.
    if (SUCCEEDED(g_pFramework->QueryInterface(&spPropertyStore)))
    {
        PROPVARIANT propvarBackground;
        PROPVARIANT propvarHighlight;
        PROPVARIANT propvarText;

        spPropertyStore->GetValue(UI_PKEY_GlobalBackgroundColor, &propvarBackground);
        spPropertyStore->GetValue(UI_PKEY_GlobalHighlightColor, &propvarHighlight);
        spPropertyStore->GetValue(UI_PKEY_GlobalTextColor, &propvarText);

        text = propvarText.uintVal;
        background = propvarBackground.uintVal;
        highlight = propvarHighlight.uintVal;
    }
}
