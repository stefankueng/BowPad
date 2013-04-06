#pragma once
#include "BaseWindow.h"
#include "resource.h"
#include "StatusBar.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"

#include <UIRibbon.h>
#include <UIRibbonPropertyHelpers.h>

class CMainWindow : public CWindow, public IUIApplication, public IUICommandHandler
{
public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = NULL);
    ~CMainWindow(void);

    /**
     * Registers the window class and creates the window.
     */
    bool                RegisterAndCreateWindow();

    bool                Initialize();
    bool                OpenFiles(const std::vector<std::wstring>& files);
    void                EnsureAtLeastOneTab();
    void                GoToLine( size_t line );

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IUIApplication methods
    STDMETHOD(OnCreateUICommand)(UINT nCmdID,
        UI_COMMANDTYPE typeID,
        IUICommandHandler** ppCommandHandler);

    STDMETHOD(OnViewChanged)(UINT viewId,
        UI_VIEWTYPE typeId,
        IUnknown* pView,
        UI_VIEWVERB verb,
        INT uReasonCode);

    STDMETHOD(OnDestroyUICommand)(UINT32 commandId, 
        UI_COMMANDTYPE typeID,
        IUICommandHandler* commandHandler);

    // IUICommandHandler methods
    STDMETHOD(UpdateProperty)(UINT nCmdID,
        REFPROPERTYKEY key,
        const PROPVARIANT* ppropvarCurrentValue,
        PROPVARIANT* ppropvarNewValue);

    STDMETHOD(Execute)(UINT nCmdID,
        UI_EXECUTIONVERB verb, 
        const PROPERTYKEY* key,
        const PROPVARIANT* ppropvarValue,
        IUISimplePropertySet* pCommandExecutionProperties);

protected:
    /// the message handler for this window
    LRESULT CALLBACK    WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT             DoCommand(int id);
    BOOL                GetStatus(int cmdId);

private:
    void                ResizeChildWindows();
    bool                SaveCurrentTab();
private:
    LONG                m_cRef;
    IUIRibbon *         m_pRibbon;
    UINT                m_RibbonHeight;

    CStatusBar          m_StatusBar;
    CTabBar             m_TabBar;
    CScintillaWnd       m_scintilla;
    CDocumentManager    m_DocManager;
};
