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

#include "raii.h"

#include "Notepad_plus_msgs.h"

// Qt port: pictures that Notepad++ copies into its tool bar
#include <QCoreApplication>
#include <QIcon>
#include <QPixmap>
#include <QString>

ToolbarIconsWrapper::ToolbarIconsWrapper() : m_icons{std::make_unique<toolbarIconsWithDarkMode>()} {
  m_icons->hToolbarBmp = nullptr;
  m_icons->hToolbarIcon = nullptr;
  m_icons->hToolbarIconDarkMode = nullptr;
}

ToolbarIconsWrapper::~ToolbarIconsWrapper() {
  // Qt port: Qt's pictures cannot go once the application has, which may be before the plugin is unloaded
  if (QCoreApplication::instance() == nullptr)
    return;
  delete m_icons->hToolbarBmp;
  delete m_icons->hToolbarIcon;
  delete m_icons->hToolbarIconDarkMode;
}

ToolbarIconsWrapper::ToolbarIconsWrapper(const wchar_t *normal_name, const wchar_t *dark_mode_name, const wchar_t *bmp_name)
  : ToolbarIconsWrapper() {
  m_icons->hToolbarBmp = new QPixmap(QString::fromWCharArray(bmp_name));
  m_icons->hToolbarIcon = new QIcon(QString::fromWCharArray(normal_name));
  m_icons->hToolbarIconDarkMode = new QIcon(QString::fromWCharArray(dark_mode_name));
}

const toolbarIconsWithDarkMode *ToolbarIconsWrapper::get() {
  return m_icons.get();
}
