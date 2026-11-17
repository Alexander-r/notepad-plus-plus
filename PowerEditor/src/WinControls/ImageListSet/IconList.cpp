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

#include "IconList.h"

#include <QString>

#include "NppResources.h"

void IconList::create(const int* iconIDArray, int iconIDArraySize)
{
	_icons.clear();
	_icons.reserve(static_cast<size_t>(iconIDArraySize));
	for (int i = 0; i < iconIDArraySize; ++i)
	{
		const char* path = nppImageResourcePath(iconIDArray[i]);
		_icons.push_back(path ? QIcon(QString::fromLatin1(path)) : QIcon());
	}
}

QIcon IconList::icon(size_t index) const
{
	return index < _icons.size() ? _icons[index] : QIcon();
}

QIcon* nppLoadIcon(int resourceId)
{
	const char* path = nppImageResourcePath(resourceId);
	return path ? new QIcon(QString::fromLatin1(path)) : nullptr;
}
