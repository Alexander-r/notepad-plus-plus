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

#include <QIcon>

#include <vector>

// Icons addressed by their position, loaded from the application resources.
class IconList
{
public:
	void create(const int* iconIDArray, int iconIDArraySize);
	QIcon icon(size_t index) const;
	size_t size() const { return _icons.size(); }

private:
	std::vector<QIcon> _icons;
};
