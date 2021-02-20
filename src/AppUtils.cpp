// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include "TempFile.h"
#include "UnicodeUtils.h"
#include "SmartHandle.h"
#include "ProgressDlg.h"
#include "PropertySet.h"
#include "ResString.h"
#include "DownloadFile.h"
#include "version.h"

#include <memory>
#include <time.h>
#include <ctime>
#include <fstream>
#include <DbgHelp.h>

#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "DbgHelp.lib")

std::wstring CAppUtils::m_updateFilename;
std::wstring CAppUtils::m_updateUrl;

CAppUtils::CAppUtils()
{
}


CAppUtils::~CAppUtils()
{
}

std::wstring CAppUtils::GetProgramFilesX86Folder()
{
    std::wstring programFiles;
    PWSTR p = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, nullptr, &p);
    if (SUCCEEDED(hr))
    {
        programFiles = p;
        CoTaskMemFree(p);
    }
    return programFiles;
}


std::wstring CAppUtils::GetDataPath(HMODULE hMod)
{
    static std::wstring dataPath;
    if (dataPath.empty())
    {
        // in case BowPad was installed with the installer, there's a registry
        // entry made by the installer.
        HKEY subKey = nullptr;
        LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\BowPad", 0, KEY_READ, &subKey);
        if (result != ERROR_SUCCESS)
        {
            dataPath = CPathUtils::GetLongPathname(CPathUtils::GetModuleDir(hMod));
        }
        else
        {
            RegCloseKey(subKey);
            // BowPad is installed: we must not store the application data
            // in the same directory as the exe is but in %APPDATA%\BowPad instead
            PWSTR outPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &outPath)))
            {
                dataPath = outPath;
                CoTaskMemFree(outPath);
                dataPath += L"\\BowPad";
                dataPath = CPathUtils::GetLongPathname(dataPath);
                CreateDirectory(dataPath.c_str(), nullptr);
            }
        }
    }
    return dataPath;
}

std::wstring CAppUtils::GetSessionID()
{
    std::wstring t;
    CAutoGeneralHandle token;
    BOOL result = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.GetPointer());
    if(result)
    {
        DWORD len = 0;
        GetTokenInformation(token, TokenStatistics, nullptr, 0, &len);
        if (len >= sizeof (TOKEN_STATISTICS))
        {
            auto data = std::make_unique<BYTE[]>(len);
            GetTokenInformation(token, TokenStatistics, data.get(), len, &len);
            LUID uid = reinterpret_cast<PTOKEN_STATISTICS>(data.get())->AuthenticationId;
            t        = CStringUtils::Format(L"-%08x%08x", uid.HighPart, uid.LowPart);
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
            // check for portable version
            HKEY subKey = nullptr;
            LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\BowPad", 0, KEY_READ, &subKey);
            if (result != ERROR_SUCCESS)
            {
                // BowPad was not installed: portable version.
                // No auto update check for portable versions
                if (!force)
                    return false;
            }
            else
                RegCloseKey(subKey);

            std::wstring tempFile = CTempFiles::Instance().GetTempFilePath(true);

            std::wstring sCheckURL = L"https://raw.githubusercontent.com/stefankueng/BowPad/main/version.txt";
            HRESULT res = URLDownloadToFile(nullptr, sCheckURL.c_str(), tempFile.c_str(), 0, nullptr);
            if (res == S_OK)
            {
                CIniSettings::Instance().SetInt64(L"updatecheck", L"last", now);
                std::ifstream file;
                file.open(tempFile.c_str());
                if (file.good())
                {
                    char line[1024];
                    const char* pLine = line;
                    file.getline(line, sizeof(line));
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
                    file.getline(line, sizeof(line));
                    m_updateUrl = CUnicodeUtils::StdGetUnicode(line);
                    file.getline(line, sizeof(line));
                    m_updateFilename = CUnicodeUtils::StdGetUnicode(line);
#else
                    // first two lines are for the 64-bit version
                    File.getline(line, sizeof(line));
                    File.getline(line, sizeof(line));
                    // now for the 32-bit version
                    File.getline(line, sizeof(line));
                    updateUrl = CUnicodeUtils::StdGetUnicode(line);
                    File.getline(line, sizeof(line));
                    updateFilename = CUnicodeUtils::StdGetUnicode(line);
#endif
                }
                file.close();
                DeleteFile(tempFile.c_str());
            }
        }
    }
    return bNewerAvailable;
}

HRESULT CALLBACK CAppUtils::TDLinkClickCallback(HWND hWnd, UINT uNotification, WPARAM /*wParam*/, LPARAM lParam, LONG_PTR /*dwRefData*/)
{
    switch (uNotification)
    {
    case TDN_HYPERLINK_CLICKED:
        ShellExecute(hWnd, L"open", reinterpret_cast<LPCWSTR>(lParam), nullptr, nullptr, SW_SHOW);
        break;
    }

    return S_OK;
}

bool CAppUtils::DownloadUpdate(HWND hWnd, bool bInstall)
{
    if (m_updateFilename.empty() || m_updateUrl.empty())
        return false;

    PWSTR downloadPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadPath);
    if (SUCCEEDED(hr))
    {
        std::wstring sDownloadFile = downloadPath;
        CoTaskMemFree(downloadPath);
        sDownloadFile += L"\\";
        sDownloadFile += m_updateFilename;

        CProgressDlg progDlg;
        progDlg.SetTitle(L"BowPad Update");
        progDlg.SetLine(1, L"Downloading BowPad Update...");
        progDlg.ResetTimer();
        progDlg.SetTime();
        progDlg.ShowModal(hWnd);

        CDownloadFile fileDownloader(L"BowPad", &progDlg);

        if (fileDownloader.DownloadFile(m_updateUrl, sDownloadFile))
        {
            if (bInstall)
            {
                ::ShellExecute(hWnd, L"open", sDownloadFile.c_str(), nullptr, nullptr, SW_SHOW);
                PostQuitMessage(0);
            }
            else
            {
                // open explorer with the downloaded file selected
                PCIDLIST_ABSOLUTE __unaligned pidl = ILCreateFromPath(sDownloadFile.c_str());
                if (pidl)
                {
                    SHOpenFolderAndSelectItems(pidl,0,nullptr,0);
                    CoTaskMemFree(static_cast<LPVOID>(const_cast<PIDLIST_ABSOLUTE>(pidl)));
                }

            }
        }
    }

    return SUCCEEDED(hr);
}

bool CAppUtils::ShowUpdateAvailableDialog( HWND hWnd )
{
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
    tdc.hInstance = g_hRes;
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

    HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &bCheckboxChecked);
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

bool CAppUtils::HasSameMajorVersion( const std::wstring& path )
{
    std::wstring sVer = CPathUtils::GetVersionFromFile(path);
    // cut off the build version
    sVer = sVer.substr(0, sVer.find_last_of('.'));
    std::wstring sAppVer = TEXT(STRPRODUCTVER);
    sAppVer = sAppVer.substr(0, sAppVer.find_last_of('.'));
    if (_wcsicmp(sVer.c_str(), sAppVer.c_str()) == 0)
    {
        // ensure the bitness matches as well
        CAutoFile hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile)
        {
            CAutoFile hFileMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (hFileMapping)
            {
                void* imagePtr = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
                if (imagePtr)
                {
                    PIMAGE_NT_HEADERS headersPtr = ImageNtHeader(imagePtr);
                    auto machine = headersPtr->FileHeader.Machine;
                    UnmapViewOfFile(imagePtr);
#ifdef _WIN64
                    return machine == IMAGE_FILE_MACHINE_AMD64;
#else
                    return machine == IMAGE_FILE_MACHINE_I386;
#endif
                }
            }
        }
    }
    return false;
}

bool CAppUtils::FailedShowMessage(HRESULT hr)
{
    if (FAILED(hr))
    {
        _com_error err(hr);
        if (CIniSettings::Instance().GetInt64(L"Debug", L"usemessagebox", 0))
        {
            MessageBox(nullptr, L"BowPad", err.ErrorMessage(), MB_ICONERROR);
        }
        else
        {
            CTraceToOutputDebugString::Instance()(L"BowPad : ");
            CTraceToOutputDebugString::Instance()(err.ErrorMessage());
            CTraceToOutputDebugString::Instance()(L"\n");
        }
        return true;
    }
    return false;
}

HRESULT CAppUtils::AddStringItem(IUICollectionPtr& collection, LPCWSTR text, int cat, IUIImagePtr pImg)
{
    CPropertySet* pItem=nullptr;
    HRESULT       hr = CPropertySet::CreateInstance(&pItem);
    if (FailedShowMessage(hr))
        return hr;
    pItem->InitializeItemProperties(pImg, text, cat);

    // Add the newly-created property set to the collection supplied by the framework.
    hr = collection->Add(pItem);
    FailedShowMessage(hr);
    pItem->Release();
    return hr;
}

HRESULT CAppUtils::AddCommandItem(IUICollectionPtr& collection, int cat, int commandId, UI_COMMANDTYPE commandType)
{
    CPropertySet* pItem=nullptr;
    HRESULT       hr = CPropertySet::CreateInstance(&pItem);
    if (FailedShowMessage(hr))
        return hr;

    pItem->InitializeCommandProperties(cat, commandId, commandType);

    // Add the newly-created property set to the collection supplied by the framework.
    hr = collection->Add(pItem);
    FailedShowMessage(hr);
    pItem->Release();
    return hr;
}

HRESULT CAppUtils::AddCategory(IUICollectionPtr& coll, int catId, int catNameResId)
{
    // TODO! Use a RAII type to manage life time of the category objects.
    // Note Categories appear in the list in the order of their creation.
    CPropertySet* cat=nullptr;
    HRESULT hr = CPropertySet::CreateInstance(&cat);
    if (CAppUtils::FailedShowMessage(hr))
        return hr;

    ResString catName(g_hRes, catNameResId);
    cat->InitializeCategoryProperties(catName, catId);
    hr = coll->Add(cat);
    cat->Release();
    return hr;
}

HRESULT CAppUtils::AddResStringItem(IUICollectionPtr& collection, int resId, int cat, IUIImagePtr pImg)
{
    ResString rs(g_hRes, resId);
    return AddStringItem(collection, rs, cat, pImg);
}

bool CAppUtils::ShowDropDownList(HWND hWnd, LPCWSTR ctrlName)
{
    // open the dropdown gallery using windows automation
    IUIAutomationPtr pAutomation;
     HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), reinterpret_cast<void**>(&pAutomation));
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // get the top element of this app window
    IUIAutomationElementPtr pParent;
    hr = pAutomation->ElementFromHandle(hWnd, &pParent);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // set up conditions to find the control
    // first condition is the name
    // second condition is the accessibility role id, which is 0x38 for a dropdown control
    // the second condition is required since there's also a normal button with the same name:
    // the default button of the dropdown gallery
    IUIAutomationConditionPtr pCondition;
    VARIANT varProp;
    varProp.vt = VT_BSTR;
    varProp.bstrVal = SysAllocString(ctrlName);
    hr = pAutomation->CreatePropertyCondition(UIA_NamePropertyId, varProp, &pCondition);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    varProp.vt = VT_INT;
    varProp.intVal = 0x38;
    IUIAutomationConditionPtr pCondition2;
    hr = pAutomation->CreatePropertyCondition(UIA_LegacyIAccessibleRolePropertyId, varProp, &pCondition2);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // both conditions must be true
    IUIAutomationConditionPtr pCondition3;
    hr = pAutomation->CreateAndCondition(pCondition, pCondition2, &pCondition3);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // now try to find the control
    IUIAutomationElementPtr pFound;
    hr = pParent->FindFirst(TreeScope_Descendants, pCondition3, &pFound);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    if (!pFound)
        return false;

    // the invoke pattern of the control so we can invoke it
    IUIAutomationInvokePatternPtr pInvoke;
    hr = pFound->GetCurrentPatternAs(UIA_InvokePatternId, __uuidof(IUIAutomationInvokePattern),
        reinterpret_cast<void**>(&pInvoke));
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    // finally, invoke the command which drops down the gallery
    pInvoke->Invoke();
    pFound->SetFocus();
    return true;
}

HRESULT CAppUtils::CreateImage(LPCWSTR resName, IUIImagePtr& pOutImg, int width/* = 0*/, int height/* = 0*/)
{
    pOutImg = nullptr;
    // Create an IUIImage from a resource id.
    IUIImagePtr           pImg;
    IUIImageFromBitmapPtr pifbFactory;
    HRESULT               hr = CoCreateInstance(CLSID_UIRibbonImageFromBitmapFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pifbFactory));
    if (SUCCEEDED(hr))
    {
        // Load the bitmap from the resource file.
        auto hbm = static_cast<HBITMAP>(LoadImage(GetModuleHandle(nullptr), resName, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        if (hbm)
        {
            // Use the factory implemented by the framework to produce an IUIImage.
            hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
            if (FAILED(hr))
            {
                DeleteObject(hbm);
                pImg = nullptr;
            }
            else
            {
                pOutImg = pImg;
            }
        }
        else
        {
            hr = E_FAIL;
            // before giving up, try loading the image resource with GDI+
            DWORD resLen = 0;
            auto resourceData = GetResourceData(L"IMAGE", LOWORD(resName), resLen);

            if (resLen)
            {
                auto hBuffer = ::GlobalAlloc(GMEM_MOVEABLE, resLen);
                if (hBuffer)
                {
                    void* pBuffer = ::GlobalLock(hBuffer);
                    if (pBuffer)
                    {
                        CopyMemory(pBuffer, resourceData, resLen);

                        IStream* pStream = nullptr;
                        if (::CreateStreamOnHGlobal(hBuffer, FALSE, &pStream) == S_OK)
                        {
                            auto pBitmap = Gdiplus::Bitmap::FromStream(pStream);
                            pStream->Release();
                            if (pBitmap)
                            {
                                if (pBitmap->GetLastStatus() == Gdiplus::Ok)
                                {
                                    if (width || height)
                                    {
                                        auto resized = new Gdiplus::Bitmap(width, height);
                                        auto g = Gdiplus::Graphics::FromImage(resized);
                                        g->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                                        g->Clear(Gdiplus::Color::Transparent);
                                        g->DrawImage(pBitmap, Gdiplus::Rect(0, 0, width, height));
                                        delete pBitmap;
                                        pBitmap = resized;
                                    }
                                    pBitmap->GetHBITMAP(Gdiplus::Color(0xFFFFFFFF), &hbm);
                                    hr = pifbFactory->CreateImage(hbm, UI_OWNERSHIP_TRANSFER, &pImg);
                                    if (SUCCEEDED(hr))
                                        pOutImg = pImg;
                                    else
                                        ::DeleteObject(hbm);
                                }

                                delete pBitmap;
                            }
                        }
                        ::GlobalUnlock(hBuffer);
                    }
                    ::GlobalFree(hBuffer);
                }
            }
        }
    }
    return hr;
}

bool CAppUtils::TryParse(const wchar_t* s, int& result, bool emptyOk, int def, int base)
{
    // At the time of writing _wtoi doesn't appear to set errno at all,
    // despite what the documentation suggests.
    // Using std::stoi isn't ideal as exceptions aren't really wanted here,
    // and they cloud the output window in VS with often unwanted exception
    // info for exceptions that are handled so aren't of interest.
    // However, for the uses intended so far exceptions shouldn't be thrown
    // so hopefully this will not be a problem.
    if (!*s)
    {
        // Don't even try to convert empty strings.
        // Success or failure is determined by the caller.
        result = def;
        return emptyOk;
    }
    try
    {
        result  = std::stoi(s, nullptr, base);
    }
    catch (const std::invalid_argument& /*ex*/)
    {
        result = def;
        return false;
    }
    return true;
}

bool CAppUtils::TryParse(const wchar_t* s, unsigned long & result, bool emptyOk, unsigned long def, int base)
{
    if (!*s)
    {
        result = def;
        return emptyOk;
    }
    try
    {
        result = std::stoi(s, nullptr, base);
    }
    catch (const std::invalid_argument& /*ex*/)
    {
        result = def;
        return false;
    }
    return true;
}

const char* CAppUtils::GetResourceData(const wchar_t * resName, int id, DWORD& resLen)
{
    resLen = 0;
    auto hResource = FindResource(nullptr, MAKEINTRESOURCE(id), resName);
    if (!hResource)
        return nullptr;
    auto hResourceLoaded = LoadResource(nullptr, hResource);
    if (!hResourceLoaded)
        return nullptr;
    auto lpResLock = static_cast<const char*>(LockResource(hResourceLoaded));
    resLen         = SizeofResource(nullptr, hResource);
    return lpResLock;
}

