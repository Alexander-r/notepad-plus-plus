// This file is part of DSpellCheck Plug-in for Notepad++
// Copyright (C)2019 Sergey Semushin <Predelnik@gmail.com>
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

// Qt port: the standard headers it had, with the compat headers of the plugins in place of the
// Windows ones (Winsock, the PPL tasks, shlwapi, windowsx, Shlobj)
#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <regex>
#include <set>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "plugin/Plugin.h"  // plugin should be available everywhere to provide acess to rc_str function
#include "plugin/resource.h" // resource should be available everywhere to have acess to string ids

using namespace std::literals;
