// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

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


#include "GoToLineDlg.h"

#include <QAbstractButton>
#include <QDialog>
#include <QLineEdit>

#include "Parameters.h"
#include "ui_GolineDlg.h"

void GoToLineDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::GolineDlg ui;
		ui.setupUi(dialog);

		ui.radioGotoline->setChecked(true);
		goToCenter();

		QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { onGo(); });
		QObject::connect(ui.golineEdit, &QLineEdit::returnPressed, dialog, [this]() { onGo(); });
		QObject::connect(ui.radioGotoline, &QAbstractButton::clicked, dialog, [this]() { _mode = go2line; updateLinesNumbers(); });
		QObject::connect(ui.radioGotooffset, &QAbstractButton::clicked, dialog, [this]() { _mode = go2offsset; updateLinesNumbers(); });
	}
	display();
}

void GoToLineDlg::display(bool toShow) const
{
	Window::display(toShow);
	if (toShow)
	{
		updateLinesNumbers();
		dlgItem(ID_GOLINE_EDIT)->setFocus();
	}
	else if (_hSelf)
	{
		// clean Line Edit
		setItemText(ID_GOLINE_EDIT, L"");
	}
}

long long GoToLineDlg::getLine() const
{
	const std::wstring text = getItemText(ID_GOLINE_EDIT);
	if (text.empty())
		return -1;
	return wcstoll(text.c_str(), nullptr, 10);
}

void GoToLineDlg::onGo()
{
	(*_ppEditView)->execute(SCI_SETMODEVENTMASK, MODEVENTMASK_OFF);
	long long line = getLine();
	if (line != -1)
	{
	    display(false);
		if (_mode == go2line)
		{
			(*_ppEditView)->execute(SCI_ENSUREVISIBLE, static_cast<WPARAM>(line - 1));
			(*_ppEditView)->execute(SCI_GOTOLINE, static_cast<WPARAM>(line - 1));
		}
		else
		{
			size_t posToGoto = 0;
			if (line > 0)
			{
				// make sure not jumping into the middle of a multibyte character
				// or into the middle of a CR/LF pair for Windows files
				auto before = (*_ppEditView)->execute(SCI_POSITIONBEFORE, static_cast<WPARAM>(line));
				posToGoto = (*_ppEditView)->execute(SCI_POSITIONAFTER, before);
			}
			auto sci_line = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, posToGoto);
			(*_ppEditView)->execute(SCI_ENSUREVISIBLE, sci_line);
			(*_ppEditView)->execute(SCI_GOTOPOS, posToGoto);
		}
	}
	unsigned long MODEVENTMASK_ON = NppParameters::getInstance().getScintillaModEventMask();
	(*_ppEditView)->execute(SCI_SETMODEVENTMASK, MODEVENTMASK_ON);

	SCNotification notification{};
	notification.nmhdr.code = SCN_PAINTED;
	notification.nmhdr.hwndFrom = _hSelf;
	notification.nmhdr.idFrom = 0;
	::SendMessage(_hParent, WM_NOTIFY, LINKTRIGGERED, reinterpret_cast<LPARAM>(&notification));

	(*_ppEditView)->grabFocus();
}

void GoToLineDlg::updateLinesNumbers() const
{
	size_t current = 0;
	size_t limit = 0;
	
	if (_mode == go2line)
	{
		current = (*_ppEditView)->getCurrentLineNumber() + 1;
		limit = (*_ppEditView)->execute(SCI_GETLINECOUNT);
	}
	else
	{
		current = (*_ppEditView)->execute(SCI_GETCURRENTPOS);
		size_t currentDocLength = (*_ppEditView)->getCurrentDocLen();
		limit = (currentDocLength > 0 ? currentDocLength - 1 : 0);
	}

	setItemText(ID_CURRLINE_EDIT, std::to_wstring(current));
	setItemText(ID_LASTLINE, std::to_wstring(limit));
}
