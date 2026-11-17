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

// Qt port: the control is a QComboBox, and every call is made on the GUI thread

#include "CAnyComboBox.h"

#include <QComboBox>
#include <QLineEdit>

namespace
{
  // As the FINDSTRING messages: the first item after nAfterIndex, going round to the start, whose text starts with
  // the string, or equals it, whatever the case
  template <class Items> int findItem(const Items& itemText, int count, LPCTSTR cszString, int nAfterIndex, bool exact)
  {
    const QString wanted = QString::fromWCharArray(cszString ? cszString : L"");
    for (int i = 0; i < count; ++i)
    {
      const int index = (nAfterIndex + 1 + i) % count;
      const QString text = itemText(index);
      if (exact ? (text.compare(wanted, Qt::CaseInsensitive) == 0) : text.startsWith(wanted, Qt::CaseInsensitive))
        return index;
    }
    return -1;
  }

  int copyText(const QString& text, TCHAR* lpString)
  {
    const std::wstring s = text.toStdWString();
    wmemcpy(lpString, s.data(), s.size());
    lpString[s.size()] = 0;
    return static_cast<int>(s.size());
  }
}

namespace
{
  QComboBox* comboBox(HWND hWnd)
  {
    return qobject_cast<QComboBox*>(hWnd);
  }
}

CAnyComboBox::CAnyComboBox() : CAnyWindow()
{
}

CAnyComboBox::~CAnyComboBox()
{
}

INT CAnyComboBox::AddString(LPCTSTR cszString)
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
    {
      combo->addItem(QString::fromWCharArray(cszString));
      n = combo->count() - 1;
    }
  } );
  return n;
}

INT CAnyComboBox::DeleteString(INT nItemIndex)
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
    {
      if (nItemIndex >= 0 && nItemIndex < combo->count())
      {
        combo->removeItem(nItemIndex);
        n = combo->count();
      }
    }
  } );
  return n;
}

INT CAnyComboBox::GetEditSel(INT* pnStartPos, INT* pnEndPos) const
{
  INT n1 = 0, n2 = 0;
  nppExecOnGuiThread( [&]() {
    auto* combo = comboBox(m_hWnd);
    if (combo && combo->lineEdit())
    {
      QLineEdit* edit = combo->lineEdit();
      if (edit->hasSelectedText())
      {
        n1 = edit->selectionStart();
        n2 = edit->selectionEnd();
      }
      else
        n1 = n2 = edit->cursorPosition();
    }
  } );
  if (pnStartPos)
    *pnStartPos = n1;
  if (pnEndPos)
    *pnEndPos = n2;
  return MAKELONG(n1, n2);
}

INT CAnyComboBox::FindString(LPCTSTR cszString, INT nAfterIndex ) const
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
      n = findItem([combo](int i) { return combo->itemText(i); }, combo->count(), cszString, nAfterIndex, false);
  } );
  return n;
}

INT CAnyComboBox::FindStringExact(LPCTSTR cszString, INT nAfterIndex ) const
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
      n = findItem([combo](int i) { return combo->itemText(i); }, combo->count(), cszString, nAfterIndex, true);
  } );
  return n;
}

INT CAnyComboBox::GetCount() const
{
  INT n = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
      n = combo->count();
  } );
  return n;
}

INT CAnyComboBox::GetCurSel() const
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
      n = combo->currentIndex();
  } );
  return n;
}

INT CAnyComboBox::GetLBText(INT nItemIndex, LPTSTR lpString)
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* combo = comboBox(m_hWnd);
    if (combo && nItemIndex >= 0 && nItemIndex < combo->count())
      n = copyText(combo->itemText(nItemIndex), lpString);
  } );
  return n;
}

INT CAnyComboBox::GetLBTextLength(INT nItemIndex) const
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* combo = comboBox(m_hWnd);
    if (combo && nItemIndex >= 0 && nItemIndex < combo->count())
      n = static_cast<INT>(combo->itemText(nItemIndex).toStdWString().size());
  } );
  return n;
}

INT CAnyComboBox::InsertString(INT nItemIndex, LPCTSTR cszString)
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
    {
      n = (nItemIndex < 0 || nItemIndex > combo->count()) ? combo->count() : nItemIndex;
      combo->insertItem(n, QString::fromWCharArray(cszString));
    }
  } );
  return n;
}

void CAnyComboBox::LimitText(INT nMaxCharacters)
{
  nppExecOnGuiThread( [&]() {
    auto* combo = comboBox(m_hWnd);
    if (combo && combo->lineEdit())
      combo->lineEdit()->setMaxLength(nMaxCharacters > 0 ? nMaxCharacters : 32767);
  } );
}

void CAnyComboBox::ResetContent()
{
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
    {
      // As CB_RESETCONTENT, which also empties the edit field
      combo->clear();
      if (combo->lineEdit())
        combo->lineEdit()->clear();
    }
  } );
}

INT CAnyComboBox::SetCurSel(INT nItemIndex)
{
  INT n = CB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* combo = comboBox(m_hWnd))
    {
      // As CB_SETCURSEL, which notifies nothing
      const QSignalBlocker quiet(combo);
      combo->setCurrentIndex((nItemIndex >= 0 && nItemIndex < combo->count()) ? nItemIndex : -1);
      n = combo->currentIndex();
    }
  } );
  return n;
}

INT CAnyComboBox::SetEditSel(INT nStartPos, INT nEndPos)
{
  nppExecOnGuiThread( [&]() {
    auto* combo = comboBox(m_hWnd);
    if (combo && combo->lineEdit())
    {
      QLineEdit* edit = combo->lineEdit();
      const int length = static_cast<int>(edit->text().size());
      if (nStartPos < 0)
        edit->deselect();
      else
      {
        const int end = (nEndPos < 0) ? length : std::min(nEndPos, length);
        edit->setSelection(std::min(nStartPos, length), end - std::min(nStartPos, length));
      }
    }
  } );
  return TRUE;
}
