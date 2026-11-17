/*
This file is part of NppExec
Copyright (C) 2013 DV <dvv81 (at) ukr (dot) net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Qt port: the control is a QAbstractButton, a check box or a radio button

#include "CAnyCheckBox.h"

#include <QAbstractButton>

CAnyCheckBox::CAnyCheckBox() : CAnyWindow()
{
}

CAnyCheckBox::~CAnyCheckBox()
{
}

BOOL CAnyCheckBox::IsChecked()
{
  BOOL checked = FALSE;
  nppExecOnGuiThread( [&]() {
    if (auto* button = qobject_cast<QAbstractButton*>(m_hWnd))
      checked = button->isChecked() ? TRUE : FALSE;
  } );
  return checked;
}

void CAnyCheckBox::SetCheck(BOOL bChecked)
{
  nppExecOnGuiThread( [&]() {
    if (auto* button = qobject_cast<QAbstractButton*>(m_hWnd))
      button->setChecked(bChecked != FALSE);
  } );
}
