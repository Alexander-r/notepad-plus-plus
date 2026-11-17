// This file is part of Notepad++ project
// Copyright (c) 2021 adzm / Adam D. Walling

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <string>

#include "NppPlatform.h"

// File operations with administrator rights, run through pkexec. The desktop's authentication dialog
// names the command it is about to run. All of them return NO_ERROR or an errno value, and report to the
// user themselves when no authentication agent is running.

// Writes in place, so an existing file keeps its owner and permissions.
DWORD nppElevatedSave(const std::wstring& path, const std::string& content);
DWORD nppElevatedMove(const std::wstring& from, const std::wstring& to);
DWORD nppElevatedCreateEmptyFile(const std::wstring& path);
DWORD nppElevatedSetWritable(const std::wstring& path, bool writable);
