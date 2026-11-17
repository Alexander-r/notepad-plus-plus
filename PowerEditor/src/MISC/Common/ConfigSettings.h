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

#include "NppXml.h"

// Stores the tree of config.xml in an INI file through QSettings, and reads it back. The code that fills
// and reads this tree stays as it was, so preferences keep their names and defaults.
//
// Mapping, for an element at group G:
//   attribute name=value      G/name=value
//   element text              G/_text=value
//   <GUIConfig name="X">      G/X, as these are looked up by name
//   any other single child    G/Tag
//   repeated children         G/Tag/1, G/Tag/2, ... in document order
//   an empty element          G/_empty=true, so that it is not lost

bool loadConfigFromSettings(NppXml::Document doc, const std::wstring& iniPath);
bool saveConfigToSettings(NppXml::Document doc, const std::wstring& iniPath);
