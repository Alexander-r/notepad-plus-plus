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

// Qt port: the control is a QListWidget, and every call is made on the GUI thread

#include "CAnyListBox.h"

#include <QListWidget>

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
  QListWidget* listBox(HWND hWnd)
  {
    return qobject_cast<QListWidget*>(hWnd);
  }
}

CAnyListBox::CAnyListBox() : CAnyWindow()
{
}

CAnyListBox::~CAnyListBox()
{
}

int CAnyListBox::AddString(LPCTSTR cszText)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
    {
      // In its place in a sorted list, as with LBS_SORT
      auto* item = new QListWidgetItem(QString::fromWCharArray(cszText), list);
      n = list->row(item);
    }
  } );
  return n;
}

int CAnyListBox::DeleteString(int nItemIndex)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* list = listBox(m_hWnd);
    if (list && nItemIndex >= 0 && nItemIndex < list->count())
    {
      delete list->takeItem(nItemIndex);
      n = list->count();
    }
  } );
  return n;
}

int CAnyListBox::FindString(LPCTSTR cszText, int nAfterIndex ) const
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
      n = findItem([list](int i) { return list->item(i)->text(); }, list->count(), cszText, nAfterIndex, false);
  } );
  return n;
}

int CAnyListBox::FindStringExact(LPCTSTR cszText, int nAfterIndex ) const
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
      n = findItem([list](int i) { return list->item(i)->text(); }, list->count(), cszText, nAfterIndex, true);
  } );
  return n;
}

int CAnyListBox::GetCount() const
{
  int n = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
      n = list->count();
  } );
  return n;
}

int CAnyListBox::GetCurSel() const
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
    {
      const QListWidgetItem* item = list->currentItem();
      n = (item && item->isSelected()) ? list->row(item) : LB_ERR;
    }
  } );
  return n;
}

LRESULT CAnyListBox::GetItemData(int nItemIndex) const
{
  LRESULT data = LB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* list = listBox(m_hWnd);
    if (list && nItemIndex >= 0 && nItemIndex < list->count())
      data = static_cast<LRESULT>(list->item(nItemIndex)->data(Qt::UserRole).toLongLong());
  } );
  return data;
}

int CAnyListBox::GetString(int nItemIndex, TCHAR* lpText)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* list = listBox(m_hWnd);
    if (list && nItemIndex >= 0 && nItemIndex < list->count())
      n = copyText(list->item(nItemIndex)->text(), lpText);
  } );
  return n;
}

int CAnyListBox::GetStringLength(int nItemIndex) const
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* list = listBox(m_hWnd);
    if (list && nItemIndex >= 0 && nItemIndex < list->count())
      n = static_cast<int>(list->item(nItemIndex)->text().toStdWString().size());
  } );
  return n;
}

int CAnyListBox::InsertString(int nItemIndex, LPCTSTR cszText)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
    {
      n = (nItemIndex < 0 || nItemIndex > list->count()) ? list->count() : nItemIndex;
      list->insertItem(n, QString::fromWCharArray(cszText));
    }
  } );
  return n;
}

void CAnyListBox::ResetContent()
{
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
      list->clear();
  } );
}

int CAnyListBox::SetCurSel(int nItemIndex)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    if (auto* list = listBox(m_hWnd))
    {
      // As LB_SETCURSEL, which notifies nothing
      const QSignalBlocker quiet(list);
      if (nItemIndex >= 0 && nItemIndex < list->count())
      {
        list->setCurrentRow(nItemIndex);
        n = nItemIndex;
      }
      else
        list->setCurrentRow(-1);
    }
  } );
  return n;
}

int CAnyListBox::SetItemData(int nItemIndex, LPARAM lData)
{
  int n = LB_ERR;
  nppExecOnGuiThread( [&]() {
    auto* list = listBox(m_hWnd);
    if (list && nItemIndex >= 0 && nItemIndex < list->count())
    {
      list->item(nItemIndex)->setData(Qt::UserRole, static_cast<qlonglong>(lData));
      n = 0;
    }
  } );
  return n;
}
