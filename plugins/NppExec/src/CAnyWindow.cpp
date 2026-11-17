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

// Qt port: the window is a Qt widget, and every call is made on the GUI thread, as Windows sends the messages of
// these calls to the thread of the window.

#include "CAnyWindow.h"
#include "qtport/NppExecWidgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTextEdit>
#include <QWidget>

QString nppExecWidgetText(const QWidget* widget)
{
  if (!widget)
    return QString();
  if (const auto* edit = qobject_cast<const QLineEdit*>(widget))
    return edit->text();
  // A multi-line edit gives its lines with "\r\n", as on Windows
  if (const auto* edit = qobject_cast<const QPlainTextEdit*>(widget))
    return edit->toPlainText().replace(u'\n', QStringLiteral("\r\n"));
  if (const auto* edit = qobject_cast<const QTextEdit*>(widget))
    return edit->toPlainText().replace(u'\n', QStringLiteral("\r\n"));
  if (const auto* combo = qobject_cast<const QComboBox*>(widget))
    return combo->currentText();
  if (const auto* button = qobject_cast<const QAbstractButton*>(widget))
    return button->text();
  if (const auto* label = qobject_cast<const QLabel*>(widget))
    return label->text();
  if (const auto* group = qobject_cast<const QGroupBox*>(widget))
    return group->title();
  return widget->windowTitle();
}

void nppExecSetWidgetText(QWidget* widget, const QString& text)
{
  if (!widget)
    return;
  // "\r\n" and "\r" end the lines of a multi-line edit, as on Windows
  QString lines = text;
  lines.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  lines.replace(u'\r', u'\n');
  if (auto* edit = qobject_cast<QLineEdit*>(widget))
    edit->setText(text);
  else if (auto* edit = qobject_cast<QPlainTextEdit*>(widget))
    edit->setPlainText(lines);
  else if (auto* edit = qobject_cast<QTextEdit*>(widget))
    edit->setPlainText(lines);
  else if (auto* combo = qobject_cast<QComboBox*>(widget))
  {
    if (combo->isEditable())
      combo->setEditText(text);
  }
  else if (auto* button = qobject_cast<QAbstractButton*>(widget))
    button->setText(text);
  else if (auto* label = qobject_cast<QLabel*>(widget))
    label->setText(text);
  else if (auto* group = qobject_cast<QGroupBox*>(widget))
    group->setTitle(text);
  else
    widget->setWindowTitle(text);
}

HWND GetDlgItem(HWND dialog, int id)
{
  if (!dialog)
    return nullptr;
  HWND found = nullptr;
  nppExecOnGuiThread( [&]() {
    const QList<QWidget*> children = dialog->findChildren<QWidget*>();
    for (QWidget* child : children)
    {
      const QVariant value = child->property("nppId");
      if (value.isValid() && value.toInt() == id)
      {
        found = child;
        return;
      }
    }
  } );
  return found;
}

int GetDlgCtrlID(HWND control)
{
  return control ? control->property("nppId").toInt() : 0;
}

CAnyWindow::CAnyWindow()
{
  m_hWnd = NULL;
}

CAnyWindow::~CAnyWindow()
{
}

BOOL CAnyWindow::BringWindowToTop()
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [this]() { m_hWnd->window()->raise(); m_hWnd->window()->activateWindow(); } );
  return TRUE;
}

BOOL CAnyWindow::CenterWindow(HWND hParentWnd, BOOL /*bRepaint*/)
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [this, hParentWnd]() {
    QRect area;
    if (hParentWnd)
      area = hParentWnd->window()->frameGeometry();
    else if (const QScreen* screen = m_hWnd->screen())
      area = screen->availableGeometry();
    QRect frame = m_hWnd->frameGeometry();
    frame.moveCenter(area.center());
    m_hWnd->move(frame.topLeft());
  } );
  return TRUE;
}

BOOL CAnyWindow::EnableWindow(BOOL bEnable)
{
  if (!m_hWnd)
    return FALSE;
  BOOL wasDisabled = FALSE;
  nppExecOnGuiThread( [&]() {
    wasDisabled = m_hWnd->isEnabled() ? FALSE : TRUE;
    m_hWnd->setEnabled(bEnable != FALSE);
  } );
  return wasDisabled;
}

INT CAnyWindow::GetText(TCHAR* lpTextBuf, INT nTextBufSize)
{
  return GetWindowText(lpTextBuf, nTextBufSize);
}

INT CAnyWindow::GetTextLength() const
{
  return GetWindowTextLength();
}

INT CAnyWindow::GetWindowText(TCHAR* lpTextBuf, INT nTextBufSize)
{
  if (!lpTextBuf || nTextBufSize <= 0)
    return 0;
  QString text;
  nppExecOnGuiThread( [&]() { text = nppExecWidgetText(m_hWnd); } );
  const std::wstring s = text.toStdWString();
  const INT n = static_cast<INT>(std::min<size_t>(s.size(), static_cast<size_t>(nTextBufSize - 1)));
  wmemcpy(lpTextBuf, s.data(), n);
  lpTextBuf[n] = 0;
  return n;
}

INT CAnyWindow::GetWindowTextLength() const
{
  QString text;
  nppExecOnGuiThread( [&]() { text = nppExecWidgetText(m_hWnd); } );
  return static_cast<INT>(text.toStdWString().size());
}

BOOL CAnyWindow::IsWindowVisible() const
{
  BOOL visible = FALSE;
  nppExecOnGuiThread( [&]() { visible = (m_hWnd && m_hWnd->isVisible()) ? TRUE : FALSE; } );
  return visible;
}

BOOL CAnyWindow::MoveWindow(INT x, INT y, BOOL /*bRepaint*/)
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [&]() { m_hWnd->move(x, y); } );
  return TRUE;
}

BOOL CAnyWindow::MoveWindow(INT x, INT y, INT width, INT height, BOOL /*bRepaint*/)
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [&]() { m_hWnd->setGeometry(x, y, width, height); } );
  return TRUE;
}

BOOL CAnyWindow::Redraw()
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [this]() { m_hWnd->update(); } );
  return TRUE;
}

BOOL CAnyWindow::ResizeWindow(INT width, INT height, BOOL /*bRepaint*/)
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [&]() { m_hWnd->resize(width, height); } );
  return TRUE;
}

LRESULT CAnyWindow::SendMsg(UINT Msg, WPARAM wParam, LPARAM lParam) const
{
  return ::SendMessage(m_hWnd, Msg, wParam, lParam);
}

HWND CAnyWindow::SetFocus()
{
  HWND previous = NULL;
  if (m_hWnd)
    nppExecOnGuiThread( [&]() { previous = QApplication::focusWidget(); m_hWnd->setFocus(); } );
  return previous;
}

HWND CAnyWindow::SetParent(HWND hWndNewParent)
{
  HWND previous = NULL;
  if (m_hWnd)
    nppExecOnGuiThread( [&]() { previous = m_hWnd->parentWidget(); m_hWnd->setParent(hWndNewParent); } );
  return previous;
}

BOOL CAnyWindow::SetText(const TCHAR* cszText)
{
  return SetWindowText(cszText);
}

void CAnyWindow::SetRedraw(BOOL bRedraw)
{
  if (m_hWnd)
    nppExecOnGuiThread( [&]() { m_hWnd->setUpdatesEnabled(bRedraw != FALSE); } );
}

BOOL CAnyWindow::SetWindowText(const TCHAR* cszText)
{
  if (!m_hWnd)
    return FALSE;
  const QString text = QString::fromWCharArray(cszText ? cszText : L"");
  nppExecOnGuiThread( [&]() { nppExecSetWidgetText(m_hWnd, text); } );
  return TRUE;
}

BOOL CAnyWindow::ShowWindow(INT nCmdShow)
{
  if (!m_hWnd)
    return FALSE;
  BOOL wasVisible = FALSE;
  nppExecOnGuiThread( [&]() {
    wasVisible = m_hWnd->isVisible() ? TRUE : FALSE;
    m_hWnd->setVisible(nCmdShow != SW_HIDE);
  } );
  return wasVisible;
}

BOOL CAnyWindow::UpdateWindow()
{
  if (!m_hWnd)
    return FALSE;
  nppExecOnGuiThread( [this]() { m_hWnd->repaint(); } );
  return TRUE;
}
