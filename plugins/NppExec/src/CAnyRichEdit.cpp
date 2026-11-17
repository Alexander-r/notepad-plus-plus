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

// Qt port: the control is an NppExecRichEditWidget, a QPlainTextEdit, and every call is made on the GUI thread, as
// Windows sends the messages of these calls to the thread of the window. Positions count the characters of the text
// as Qt does, in UTF-16 as RichEdit, and a line ends with one character, "\r" as RichEdit 2.0 gives it.

#include "CAnyRichEdit.h"

#include <vector>

#include <QApplication>
#include <QPalette>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace
{
  NppExecRichEditWidget* richEdit(HWND hWnd)
  {
    return dynamic_cast<NppExecRichEditWidget*>(hWnd);
  }

  // The text between the positions, with RichEdit's line ends
  QString textRange(QPlainTextEdit* edit, int nStart, int nEnd)
  {
    QTextCursor cursor(edit->document());
    const int length = edit->document()->characterCount() - 1;
    cursor.setPosition(std::clamp(nStart, 0, length));
    cursor.setPosition(std::clamp(nEnd, 0, length), QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, u'\r');
    text.replace(QChar::LineSeparator, u'\r');
    return text;
  }

  // Copies the text with a null after it, giving its length
  INT copyOut(const QString& text, TCHAR* lpText, INT nMaxLen = -1)
  {
    const std::wstring s = text.toStdWString();
    INT n = static_cast<INT>(s.size());
    if (nMaxLen >= 0 && n > nMaxLen)
      n = nMaxLen;
    wmemcpy(lpText, s.data(), n);
    lpText[n] = 0;
    return n;
  }

  QTextBlock blockOfLine(QPlainTextEdit* edit, int nLine)
  {
    return edit->document()->findBlockByNumber(nLine);
  }
}

CAnyRichEdit::CAnyRichEdit() : CAnyWindow()
{
}

CAnyRichEdit::~CAnyRichEdit()
{
}

void CAnyRichEdit::AddLine(const TCHAR* cszLine, BOOL bScrollText /* = FALSE */, 
  COLORREF color /* = RGB(0,0,0) */, 
  DWORD dwMask /* = CFM_EFFECTS */, DWORD dwEffects /* = 0 */)
{
  if (cszLine != NULL)
  {
    std::wstring line(cszLine);
    line += _T_RE_EOL;
    AddStr(line.c_str(), bScrollText, color, dwMask, dwEffects);
  }
}

void CAnyRichEdit::AddStr(const TCHAR* cszStr, BOOL bScrollText /* = FALSE */, 
  COLORREF color /* = RGB(0,0,0) */, 
  DWORD dwMask /* = CFM_EFFECTS */, DWORD dwEffects /* = 0 */)
{
  INT nOldLineCount = 0;
  INT nSelStart = 0, nSelEnd = 0;

  if (!bScrollText)
    ExGetSelPos(&nSelStart, &nSelEnd);
  else
    nOldLineCount = GetLineCount();

  // no redraw while adding a string
  if (!bScrollText)
    SetRedraw(FALSE);

  ExSetSel(-1, -1);
  SetCharFormat(dwMask, dwEffects, color);
  ReplaceSelText(cszStr);

  if (!bScrollText)
  {
    ExSetSel(nSelStart, nSelEnd);
    ScrollCaret();
    SetRedraw(TRUE);
    Redraw();
  }
  else
  {
    LineScroll(GetLineCount() - nOldLineCount);
  }
}

INT CAnyRichEdit::ExGetSelPos(INT* pnStartPos, INT* pnEndPos) const
{
  INT nStart = 0, nEnd = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
    {
      const QTextCursor cursor = edit->textCursor();
      nStart = cursor.selectionStart();
      nEnd = cursor.selectionEnd();
    }
  } );
  if (pnStartPos != NULL)
    *pnStartPos = nStart;
  if (pnEndPos != NULL)
    *pnEndPos = nEnd;
  return nStart;
}

void CAnyRichEdit::ExLimitText(INT nMaxLength)
{
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      edit->textLimit = (nMaxLength > 0) ? nMaxLength : 0x7FFFFFFE;
  } );
}

INT CAnyRichEdit::ExLineFromChar(INT nCharacterIndex) const
{
  return LineFromChar(nCharacterIndex);
}

void CAnyRichEdit::ExSetSel(INT nStartPos, INT nEndPos)
{
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    const int length = edit->document()->characterCount() - 1;
    // As EM_EXSETSEL: (0, -1) selects all, and a negative start puts the caret at the end
    int nStart = nStartPos, nEnd = nEndPos;
    if (nStart == 0 && nEnd == -1)
      nEnd = length;
    else if (nStart < 0)
      nStart = nEnd = length;
    if (nEnd < 0)
      nEnd = length;
    QTextCursor cursor = edit->textCursor();
    cursor.setPosition(std::clamp(nStart, 0, length));
    cursor.setPosition(std::clamp(nEnd, 0, length), QTextCursor::KeepAnchor);
    edit->setTextCursor(cursor);
  } );
}

TCHAR CAnyRichEdit::GetCharAt(INT nPos) const
{
  TCHAR ch = 0;
  TCHAR str[4];
  if (GetTextAt(nPos, 1, str) > 0)
  {
    ch = str[0];
  }
  return ch;
}

DWORD CAnyRichEdit::GetEventMask() const
{
  DWORD mask = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      mask = edit->eventMask;
  } );
  return mask;
}

INT CAnyRichEdit::GetLine(INT nLine, TCHAR* lpTextBuf, WORD wTextBufSize) const
{
  // As EM_GETLINE: the text of the line, with its line end in RichEdit, and without a null, which this adds after it
  INT nLen = 0;
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    const QTextBlock block = blockOfLine(edit, nLine);
    if (!block.isValid())
      return;
    QString text = block.text();
    if (block.next().isValid() && edit->lineWithEnd)
      text += u'\r';
    nLen = copyOut(text, lpTextBuf, static_cast<INT>(wTextBufSize));
  } );
  lpTextBuf[nLen] = 0;
  return nLen;
}

INT CAnyRichEdit::GetLineCount() const
{
  INT n = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      n = edit->document()->blockCount();
  } );
  return n;
}

BOOL CAnyRichEdit::GetModify() const
{
  BOOL modified = FALSE;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      modified = edit->document()->isModified() ? TRUE : FALSE;
  } );
  return modified;
}

INT CAnyRichEdit::GetSelPos(INT* pnStartPos, INT* pnEndPos) const
{
  return ExGetSelPos(pnStartPos, pnEndPos);
}

INT CAnyRichEdit::GetTextAt(INT nPos, INT nLen, TCHAR* lpText) const
{
  INT nRet = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      nRet = copyOut(textRange(edit, nPos, nPos + nLen), lpText);
    else
      lpText[0] = 0;
  } );
  return nRet;
}

INT CAnyRichEdit::GetTextLengthEx() const
{
  INT n = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      n = edit->document()->characterCount() - 1;
  } );
  return n;
}

BOOL CAnyRichEdit::IsEmpty() const
{
  return (GetTextLengthEx() == 0);
}

INT CAnyRichEdit::LineFromChar(INT nCharacterIndex) const
{
  // As EM_LINEFROMCHAR: -1 is the line of the caret, or of the start of the selection
  INT nLine = 0;
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    const int pos = (nCharacterIndex < 0) ? edit->textCursor().selectionStart() : nCharacterIndex;
    const QTextBlock block = edit->document()->findBlock(std::min(pos, edit->document()->characterCount() - 1));
    nLine = block.isValid() ? block.blockNumber() : edit->document()->blockCount() - 1;
  } );
  return nLine;
}

INT CAnyRichEdit::LineIndex(INT nLineNumber) const
{
  // As EM_LINEINDEX: -1 is the line of the caret, and a line past the last gives -1
  INT nIndex = -1;
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    const QTextBlock block = (nLineNumber < 0) ? edit->textCursor().block() : blockOfLine(edit, nLineNumber);
    if (block.isValid())
      nIndex = block.position();
  } );
  return nIndex;
}

INT CAnyRichEdit::LineLength(INT nCharacterIndex) const
{
  // As EM_LINELENGTH: the length of the line with that character, without its line end
  INT nLen = 0;
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    const int pos = (nCharacterIndex < 0) ? edit->textCursor().position() : nCharacterIndex;
    const QTextBlock block = edit->document()->findBlock(std::min(pos, edit->document()->characterCount() - 1));
    if (block.isValid())
      nLen = block.length() - 1;
  } );
  return nLen;
}

BOOL CAnyRichEdit::LineScroll(INT nVerticalLines)
{
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
    {
      QScrollBar* bar = edit->verticalScrollBar();
      bar->setValue(bar->value() + nVerticalLines);
    }
  } );
  return TRUE;
}

void CAnyRichEdit::ReplaceSelText(const TCHAR* cszText, BOOL /*bCanUndo*/ )
{
  QString text = QString::fromWCharArray(cszText ? cszText : L"");
  // RichEdit 2.0 keeps "\r" as its line end, and takes "\r\n" and "\n" as one
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(u'\r', u'\n');
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    QTextCursor cursor = edit->textCursor();
    const int length = edit->document()->characterCount() - 1;
    const int room = edit->textLimit - (length - (cursor.selectionEnd() - cursor.selectionStart()));
    if (room <= 0)
      return;
    if (text.size() > room)
      text.truncate(room);
    if (edit->hasPendingFormat)
    {
      cursor.insertText(text, edit->pendingFormat);
      edit->hasPendingFormat = false;
    }
    else
      cursor.insertText(text);
    edit->setTextCursor(cursor);
  } );
}

void CAnyRichEdit::ScrollCaret()
{
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      edit->ensureCursorVisible();
  } );
}

BOOL CAnyRichEdit::SetCharFormat(DWORD dwMask,
  DWORD dwEffects, COLORREF color, DWORD dwOptions)
{
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    QTextCharFormat format;
    if (dwMask & CFM_COLOR)
    {
      if (dwEffects & CFE_AUTOCOLOR)
        format.clearForeground();
      else
        format.setForeground(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    }
    if (dwMask & CFM_BOLD)
      format.setFontWeight((dwEffects & CFE_BOLD) ? QFont::Bold : QFont::Normal);
    if (dwMask & CFM_ITALIC)
      format.setFontItalic((dwEffects & CFE_ITALIC) != 0);
    if (dwMask & CFM_UNDERLINE)
      format.setFontUnderline((dwEffects & CFE_UNDERLINE) != 0);
    if (dwMask & CFM_STRIKEOUT)
      format.setFontStrikeOut((dwEffects & CFE_STRIKEOUT) != 0);

    QTextCursor cursor = edit->textCursor();
    if (dwOptions & SCF_ALL)
    {
      QTextCursor all(edit->document());
      all.select(QTextCursor::Document);
      all.mergeCharFormat(format);
    }
    else if (cursor.hasSelection())
    {
      cursor.mergeCharFormat(format);
    }
    else
    {
      // The format of the insertion point, for the text that comes next
      edit->pendingFormat = format;
      edit->hasPendingFormat = true;
    }
  } );
  return TRUE;
}

DWORD CAnyRichEdit::SetEventMask(DWORD nEventMask)
{
  DWORD previous = 0;
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
    {
      previous = edit->eventMask;
      edit->eventMask = nEventMask;
    }
  } );
  return previous;
}

BOOL CAnyRichEdit::SetFont(const LOGFONT* plf)
{
  nppExecOnGuiThread( [&]() {
    auto* edit = richEdit(m_hWnd);
    if (!edit)
      return;
    QFont font = edit->font();
    font.setFamily(QString::fromWCharArray(plf->lfFaceName));
    // lfHeight in pixels, negative for the height of the characters as Windows has it
    const int pixels = (plf->lfHeight < 0) ? -plf->lfHeight : plf->lfHeight;
    if (pixels > 0)
      font.setPixelSize(pixels);
    font.setBold(plf->lfWeight >= FW_BOLD);
    font.setItalic(plf->lfItalic != 0);
    edit->setFont(font);
  } );
  return TRUE;
}

void CAnyRichEdit::SetModify(BOOL bModified)
{
  nppExecOnGuiThread( [&]() {
    if (auto* edit = richEdit(m_hWnd))
      edit->document()->setModified(bModified != FALSE);
  } );
}

void CAnyRichEdit::SetSel(INT nStartPos, INT nEndPos)
{
  ExSetSel(nStartPos, nEndPos);
}

bool nppExecRichEditMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
  // Qt port: the messages of RichEdit and the edit control that NppExec sends itself
  auto* edit = richEdit(hWnd);
  if (!edit)
    return false;
  CAnyRichEdit re;
  re.m_hWnd = hWnd;
  *result = 0;
  switch (message)
  {
    case EM_GETSEL:
    {
      INT nStart = 0, nEnd = 0;
      re.ExGetSelPos(&nStart, &nEnd);
      if (wParam)
        *reinterpret_cast<DWORD*>(wParam) = static_cast<DWORD>(nStart);
      if (lParam)
        *reinterpret_cast<DWORD*>(lParam) = static_cast<DWORD>(nEnd);
      *result = MAKELONG(static_cast<WORD>(nStart), static_cast<WORD>(nEnd));
      return true;
    }
    case EM_SETSEL:
      if (static_cast<INT>(wParam) == -1)
      {
        // As the edit control: no selection, with the caret where it is
        INT nStart = 0;
        re.ExGetSelPos(&nStart, NULL);
        re.ExSetSel(nStart, nStart);
      }
      else
        re.ExSetSel(static_cast<INT>(wParam), static_cast<INT>(lParam));
      return true;
    case EM_REPLACESEL:
      re.ReplaceSelText(reinterpret_cast<const TCHAR*>(lParam), static_cast<BOOL>(wParam));
      return true;
    case EM_LINEFROMCHAR:
      *result = re.LineFromChar(static_cast<INT>(wParam));
      return true;
    case EM_LINEINDEX:
      *result = re.LineIndex(static_cast<INT>(wParam));
      return true;
    case EM_LINELENGTH:
      *result = re.LineLength(static_cast<INT>(wParam));
      return true;
    case EM_GETLINECOUNT:
      *result = re.GetLineCount();
      return true;
    case EM_GETLINE:
    {
      // The size of the buffer is in its first WORD, and the line comes without a null after it
      TCHAR* buf = reinterpret_cast<TCHAR*>(lParam);
      const WORD size = *reinterpret_cast<WORD*>(buf);
      std::vector<TCHAR> line(static_cast<size_t>(size) + 1);
      const INT n = re.GetLine(static_cast<INT>(wParam), line.data(), size);
      wmemcpy(buf, line.data(), n);
      *result = n;
      return true;
    }
    case EM_SCROLLCARET:
      re.ScrollCaret();
      return true;
    case EM_GETFIRSTVISIBLELINE:
      nppExecOnGuiThread( [&]() { *result = edit->firstVisibleLine(); } );
      return true;
    case WM_SETFONT:
      nppExecOnGuiThread( [&]() {
        if (const auto* font = reinterpret_cast<const QFont*>(wParam))
          edit->setFont(*font);
      } );
      return true;
    case EM_SETTARGETDEVICE:
      // RichEdit's word wrap: a line width of 0 wraps at the window's edge, and 1 does not wrap
      nppExecOnGuiThread( [&]() { edit->setLineWrapMode(lParam == 0 ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap); } );
      *result = TRUE;
      return true;
    case EM_FINDTEXT:
    {
      // From cpMin, to cpMax or the end (-1), backwards without FR_DOWN; the start of what is found, or -1
      const FINDTEXT* ft = reinterpret_cast<const FINDTEXT*>(lParam);
      *result = -1;
      nppExecOnGuiThread( [&]() {
        QTextDocument::FindFlags flags;
        if (wParam & FR_MATCHCASE)
          flags |= QTextDocument::FindCaseSensitively;
        if (wParam & FR_WHOLEWORD)
          flags |= QTextDocument::FindWholeWords;
        const bool down = (wParam & FR_DOWN) != 0;
        if (!down)
          flags |= QTextDocument::FindBackward;
        const int length = edit->document()->characterCount() - 1;
        const QTextCursor found = edit->document()->find(QString::fromWCharArray(ft->lpstrText),
          std::clamp(static_cast<int>(down ? ft->chrg.cpMin : ft->chrg.cpMin + 1), 0, length), flags);
        if (found.isNull())
          return;
        const int limit = (ft->chrg.cpMax < 0) ? length : static_cast<int>(ft->chrg.cpMax);
        if (down ? (found.selectionEnd() <= limit) : (found.selectionStart() >= limit))
          *result = found.selectionStart();
      } );
      return true;
    }
    case EM_SETBKGNDCOLOR:
      nppExecOnGuiThread( [&]() {
        QPalette palette = edit->palette();
        if (wParam)
          palette.setColor(QPalette::Base, QApplication::palette().color(QPalette::Base));
        else
          palette.setColor(QPalette::Base, QColor(GetRValue(lParam), GetGValue(lParam), GetBValue(lParam)));
        edit->setPalette(palette);
      } );
      return true;
    default:
      return false;
  }
}
