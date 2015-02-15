// This file is part of BowPad.
//
// Copyright (C) 2014 - Stefan Kueng
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
#include "BowPadUI.h"

#include <vector>

namespace
{
struct SortOptions;
};

class CCmdSort : public ICommand
{
public:
    CCmdSort(void* obj) : ICommand(obj)
    {
    }
    ~CCmdSort(void)
    {
    }

    UINT GetCmdId() override { return cmdSort; }
    bool Execute() override;

private:
    void Sort(std::vector<std::wstring>& lines) const;
    SortOptions GetSortOptions() const;
};
