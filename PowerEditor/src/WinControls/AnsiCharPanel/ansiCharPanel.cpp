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


#include "ansiCharPanel.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QKeySequence>
#include <QShortcut>
#include <QTableWidget>
#include <QVBoxLayout>

#include <string>

#include <Scintilla.h>

#include "Common.h"
#include "DockingDlgInterface.h"
#include "ListView.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "ansiCharPanel_rc.h"
#include "dockingResource.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"

using namespace std;

void AnsiCharPanel::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	NppParameters& nppParam = NppParameters::getInstance();
	NativeLangSpeaker *pNativeSpeaker = nppParam.getNativeLangSpeaker();
	wstring valStr = pNativeSpeaker->getAttrNameStr(L"Value", "AsciiInsertion", "ColumnVal");
	wstring hexStr = pNativeSpeaker->getAttrNameStr(L"Hex", "AsciiInsertion", "ColumnHex");
	wstring charStr = pNativeSpeaker->getAttrNameStr(L"Character", "AsciiInsertion", "ColumnChar");
	wstring htmlNameStr = pNativeSpeaker->getAttrNameStr(L"HTML Name", "AsciiInsertion", "ColumnHtmlName");
	wstring htmlNumberStr = pNativeSpeaker->getAttrNameStr(L"HTML Decimal", "AsciiInsertion", "ColumnHtmlNumber");
	wstring htmlHexNbStr = pNativeSpeaker->getAttrNameStr(L"HTML Hexadecimal", "AsciiInsertion", "ColumnHtmlHexNumber");

	_listView.addColumn(columnInfo(valStr, 45));
	_listView.addColumn(columnInfo(hexStr, 45));
	_listView.addColumn(columnInfo(charStr, 70));
	_listView.addColumn(columnInfo(htmlNameStr, 90));
	_listView.addColumn(columnInfo(htmlNumberStr, 100));
	_listView.addColumn(columnInfo(htmlHexNbStr, 120));

	_listView.init(_hInst, _hSelf, IDC_LIST_ANSICHAR);
	int codepage = (*_ppEditView)->getCurrentBuffer()->getEncoding();
	_listView.setValues(codepage==-1?0:codepage);

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_listView.getHSelf());

	QTableWidget* list = _listView.table();
	QObject::connect(list, &QTableWidget::cellDoubleClicked, _hSelf, [this](int row, int column)
	{
		if (column != 2)
			insertString(_listView.getText(row, column));
		else
			insertChar(static_cast<unsigned char>(row));
	});

	const auto insertSelected = [this]()
	{
		const int row = _listView.getSelectedIndex();
		if (row != -1)
			insertChar(static_cast<unsigned char>(row));
	};
	for (const int key : { Qt::Key_Return, Qt::Key_Enter })
	{
		auto* shortcut = new QShortcut(QKeySequence(key), list);
		shortcut->setContext(Qt::WidgetShortcut);
		QObject::connect(shortcut, &QShortcut::activated, _hSelf, insertSelected);
	}
}

void AnsiCharPanel::onDockingNotify(const NMHDR& notification)
{
	if (LOWORD(notification.code) == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_EDIT_CHAR_PANEL, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}

void AnsiCharPanel::switchEncoding()
{
	int codepage = (*_ppEditView)->getCurrentBuffer()->getEncoding();
	_listView.resetValues(codepage);
}

void AnsiCharPanel::insertChar(unsigned char char2insert) const
{
	const char charStr[2] { static_cast<char>(char2insert), '\0' };
	WcharMbcsConvertor& wmc = WcharMbcsConvertor::getInstance();

	std::string text;
	int codepage = (*_ppEditView)->getCurrentBuffer()->getEncoding();
	if (codepage == -1)
	{
		bool isUnicode = ((*_ppEditView)->execute(SCI_GETCODEPAGE) == SC_CP_UTF8);
		if (isUnicode)
			text = wstring2string(wmc.char2wchar(charStr, 0), CP_UTF8);
		else // ANSI
			text.assign(1, charStr[0]);
	}
	else
	{
		text = wstring2string(wmc.char2wchar(charStr, codepage), CP_UTF8);
	}

	// The NUL character is a character like any other here
	(*_ppEditView)->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(""));
	const size_t len = (char2insert < 128) ? 1 : text.length();
	if (char2insert == 0)
		text.assign(1, '\0');
	(*_ppEditView)->execute(SCI_ADDTEXT, len, reinterpret_cast<LPARAM>(text.data()));
	(*_ppEditView)->grabFocus();
}

void AnsiCharPanel::insertString(const std::wstring& string2insert) const
{
	std::string text;
	int codepage = (*_ppEditView)->getCurrentBuffer()->getEncoding();
	if (codepage == -1 && (*_ppEditView)->execute(SCI_GETCODEPAGE) != SC_CP_UTF8)
		text = wstring2string(string2insert, 0); // ANSI
	else
		text = wstring2string(string2insert, CP_UTF8);

	(*_ppEditView)->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(""));
	(*_ppEditView)->execute(SCI_ADDTEXT, text.length(), reinterpret_cast<LPARAM>(text.c_str()));
	(*_ppEditView)->grabFocus();
}
