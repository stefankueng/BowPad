// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020 - Stefan Kueng
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
#pragma once
#include "ICommand.h"

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <cassert>

class CCommandHandler
{
public:
    CCommandHandler();
    ~CCommandHandler() = default;

public:
    static CCommandHandler& Instance();
    static void             ShutDown();

    void        Init(void* obj);
    ICommand*   GetCommand(UINT cmdId);
    void        ScintillaNotify(SCNotification* pScn);
    void        TabNotify(TBHDR* ptbhdr);
    void        OnClose();
    void        OnDocumentClose(DocID id);
    void        OnDocumentOpen(DocID id);
    void        OnDocumentSave(DocID id, bool bSaveAs);
    void        OnClipboardChanged();
    void        BeforeLoad();
    void        AfterInit();
    void        OnTimer(UINT id);
    void        OnThemeChanged(bool bDark);
    void        OnLangChanged();
    const auto& GetPluginMap() { return m_plugins; }
    int         GetPluginVersion(const std::wstring& name);
    void        AddCommand(ICommand* cmd);
    void        AddCommand(UINT cmdId);
    void        InsertPlugins(void* obj);
    void        PluginNotify(UINT cmdId, const std::wstring& pluginName, LPARAM data);

    const std::map<UINT, std::unique_ptr<ICommand>>& GetCommands() const
    {
        return m_commands;
    };
    const std::map<UINT, ICommand*>& GetNoDeleteCommands() const
    {
        return m_nodeletecommands;
    };

private:
    template <typename T, typename... ARGS>
    T* Add(ARGS... args)
    {
        // Construct the type we want. We need to get the id out of it.
        // Move it into the map, then return the pointer we got
        // out. We know it must be the type we want because we just created it.
        // We could use shared_ptr here but we control the life time so
        // no point paying the price as if we didn't.
        auto pCmd      = std::make_unique<T>(std::forward<ARGS>(args)...);
        auto cmdId     = pCmd->GetCmdId();
        m_highestCmdId = max(m_highestCmdId, cmdId);
        auto at        = m_commands.emplace(cmdId, std::move(pCmd));
        assert(at.second); // Verify no command has the same ID as an existing command.
        return static_cast<T*>(at.first->second.get());
    }

    std::map<UINT, std::unique_ptr<ICommand>> m_commands;
    std::vector<ICommand*>                    m_loadfirstcommands;
    std::map<UINT, ICommand*>                 m_nodeletecommands;
    std::map<UINT, std::wstring>              m_plugins;
    std::map<std::wstring, int>               m_pluginversion;
    UINT                                      m_highestCmdId;
    static std::unique_ptr<CCommandHandler>   m_instance;
};
