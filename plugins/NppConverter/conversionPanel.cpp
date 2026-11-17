// This file is part of Notepad++ project
// Copyright (C)2022 Don HO <don.h@free.fr>

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


#include "conversionPanel.h"
#include "PluginDefinition.h"
#include <stdio.h>

// Qt port: the panel and the clipboard are Qt's
#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpressionValidator>

#include <functional>


extern NppData nppData;


#ifdef UNICODE
#define generic_strtoul wcstoul
#define generic_sprintf swprintf
#else
#define generic_strtoul strtoul
#define generic_sprintf sprintf
#endif

#define BCKGRD_COLOR (RGB(255,102,102))
#define TXT_COLOR    (RGB(255,255,255))
#define CF_NPPBINARY	TEXT("application/x-notepad-plus-plus-binary") // Qt port: the format of Paste Binary Content

// Qt port: the dialog IDD_CONVERSION_PANEL of conversionPanel.rc, with its controls connected as run_dlgProc
// handled their WM_COMMAND. Each control keeps its ID in the "nppId" property, as the dialogs of Notepad++ do.
void ConversionPanel::createForm(QWidget* form)
{
	form->setWindowTitle(QStringLiteral("Conversion"));
	auto* grid = new QGridLayout(form);

	const auto addLabel = [form, grid](int id, const char* text, int row)
	{
		auto* label = new QLabel(QString::fromLatin1(text), form);
		label->setProperty("nppId", id);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter); // WS_EX_RIGHT
		grid->addWidget(label, row, 0);
	};
	const auto addEdit = [this, form](int id, bool isNumber)
	{
		auto* edit = new QLineEdit(form);
		edit->setProperty("nppId", id);
		if (isNumber) // ES_NUMBER
			edit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), edit));
		QObject::connect(edit, &QLineEdit::textChanged, form, [this, id]()
		{
			// EN_CHANGE
			if (!lock)
				setValueFrom(id);
		});
		return edit;
	};
	const auto addButton = [form, grid](int id, const char* text, int row, int column, const std::function<void()>& onClick)
	{
		auto* button = new QPushButton(QString::fromLatin1(text), form);
		button->setProperty("nppId", id);
		button->setAutoDefault(false);
		QObject::connect(button, &QPushButton::clicked, form, onClick);
		grid->addWidget(button, row, column);
	};

	// ASCII: one character, with its name for the control characters
	addLabel(ID_ASCII_STATIC, "ASCII:", 0);
	QLineEdit* asciiEdit = addEdit(ID_ASCII_EDIT, false);
	asciiEdit->setMaxLength(1); // EM_LIMITTEXT
	asciiEdit->setFixedWidth(asciiEdit->fontMetrics().averageCharWidth() * 4 + 8);
	auto* asciiInfo = new QLabel(form);
	asciiInfo->setProperty("nppId", ID_ASCII_INFO_STATIC);
	auto* asciiRow = new QHBoxLayout;
	asciiRow->addWidget(asciiEdit);
	asciiRow->addWidget(asciiInfo, 1);
	grid->addLayout(asciiRow, 0, 1);
	addButton(ID_ASCII_BUTTON, "Copy", 0, 2, [this]() { copyToClipboardFrom(ID_ASCII_EDIT); });
	addButton(ID_ASCII_INSERT_BUTTON, "Insert", 0, 3, [this]()
	{
		int v = getAsciiUcharFromDec();
		if (v != -1)
		{
			char charStr[2];
			charStr[0] = static_cast<char>(v);
			charStr[1] = '\0';
			HWND hCurrScintilla = getCurrentScintillaHandle();
			::SendMessage(hCurrScintilla, SCI_REPLACESEL, 0, (LPARAM)"");
			::SendMessage(hCurrScintilla, SCI_ADDTEXT, 1, (LPARAM)charStr);
		}
	});

	struct Row
	{
		int labelId;
		const char* label;
		int editId;
		bool isNumber;
		int copyId;
		int insertId;
	};
	const Row rows[] = {
		{ ID_DEC_STATIC, "Decimal:", ID_DEC_EDIT, true, ID_DEC_BUTTON, ID_DEC_INSERT_BUTTON },
		{ ID_HEX_STATIC, "Hexadecimal:", ID_HEX_EDIT, false, ID_HEX_BUTTON, ID_HEX_INSERT_BUTTON },
		{ ID_BIN_STATIC, "Binary:", ID_BIN_EDIT, true, ID_BIN_BUTTON, ID_BIN_INSERT_BUTTON },
		{ ID_OCT_STATIC, "Octal:", ID_OCT_EDIT, true, ID_OCT_BUTTON, ID_OCT_INSERT_BUTTON },
	};
	int rowIndex = 1;
	for (const Row& row : rows)
	{
		addLabel(row.labelId, row.label, rowIndex);
		QLineEdit* edit = addEdit(row.editId, row.isNumber);
		edit->setMinimumWidth(edit->fontMetrics().averageCharWidth() * 150 / 4); // 150 dialog units, as in the .rc
		grid->addWidget(edit, rowIndex, 1);
		const int editId = row.editId;
		addButton(row.copyId, "Copy", rowIndex, 2, [this, editId]() { copyToClipboardFrom(editId); });
		addButton(row.insertId, "Insert", rowIndex, 3, [this, editId]() { insertToNppFrom(editId); });
		++rowIndex;
	}
	grid->setColumnStretch(1, 1);
	grid->setRowStretch(rowIndex, 1);
}

QWidget* ConversionPanel::item(int id) const
{
	const QList<QWidget*> controls = _hSelf->findChildren<QWidget*>();
	for (QWidget* control : controls)
	{
		if (control->property("nppId").toInt() == id)
			return control;
	}
	return nullptr;
}

generic_string ConversionPanel::itemText(int id) const
{
	const QWidget* control = item(id);
	if (const auto* edit = qobject_cast<const QLineEdit*>(control))
		return edit->text().toStdWString();
	if (const auto* label = qobject_cast<const QLabel*>(control))
		return label->text().toStdWString();
	return generic_string();
}

// Like WM_SETTEXT, which also sends EN_CHANGE for an edit
void ConversionPanel::setItemText(int id, const generic_string& text)
{
	QWidget* control = item(id);
	if (auto* edit = qobject_cast<QLineEdit*>(control))
		edit->setText(QString::fromStdWString(text));
	else if (auto* label = qobject_cast<QLabel*>(control))
		label->setText(QString::fromStdWString(text));
}

void ConversionPanel::resetExcept(int exceptID)
{
	if (exceptID != ID_ASCII_EDIT)
		setItemText(ID_ASCII_EDIT, TEXT("")); // Qt port: in place of WM_SETTEXT
	if (exceptID != ID_DEC_EDIT)
		setItemText(ID_DEC_EDIT, TEXT("")); // Qt port: in place of WM_SETTEXT
	if (exceptID != ID_HEX_EDIT)
		setItemText(ID_HEX_EDIT, TEXT("")); // Qt port: in place of WM_SETTEXT
	if (exceptID != ID_OCT_EDIT)
		setItemText(ID_OCT_EDIT, TEXT("")); // Qt port: in place of WM_SETTEXT
	if (exceptID != ID_BIN_EDIT)
		setItemText(ID_BIN_EDIT, TEXT("")); // Qt port: in place of WM_SETTEXT
}

bool ConversionPanel::qualified(TCHAR *str, int id)
{
	for (int i = 0 ; i < lstrlen(str) ; i++)
	{
		if (id == ID_ASCII_EDIT)
		{
			//Only one character, Accept all
		}
		else if (id == ID_HEX_EDIT)
		{
			if (!((str[i] >= '0' && str[i] <= '9') ||
				  (str[i] >= 'A' && str[i] <= 'F') ||
				  (str[i] >= 'a' && str[i] <= 'f')))
			{

				return false;
			}
		}
		else if (id == ID_DEC_EDIT)
		{
			if (!(str[i] >= '0' && str[i] <= '9'))
			{
				
				return false;
			}
		}
		else if (id == ID_OCT_EDIT)
		{
			if (!(str[i] >= '0' && str[i] <= '7'))
			{	
				
				return false;
			}
		}
		else if (id == ID_BIN_EDIT)
		{
			if (!(str[i] == '0' || str[i] == '1'))
			{	
				
				return false;
			}
		}
	}
	return true;
}

generic_string ConversionPanel::getAsciiInfo(unsigned char value)
{
	switch (value)
	{
		case 0:
			return TEXT("NULL");
		case 1:
			return TEXT("SOH");
		case 2:
			return TEXT("STX");
		case 3:
			return TEXT("ETX");
		case 4:
			return TEXT("EOT");
		case 5:
			return TEXT("ENQ");
		case 6:
			return TEXT("ACK");
		case 7:
			return TEXT("BEL");
		case 8:
			return TEXT("BS");
		case 9:
			return TEXT("TAB");
		case 10:
			return TEXT("LF");
		case 11:
			return TEXT("VT");
		case 12:
			return TEXT("FF");
		case 13:
			return TEXT("CR");
		case 14:
			return TEXT("SO");
		case 15:
			return TEXT("SI");
		case 16:
			return TEXT("DLE");
		case 17:
			return TEXT("DC1");
		case 18:
			return TEXT("DC2");
		case 19:
			return TEXT("DC3");
		case 20:
			return TEXT("DC4");
		case 21:
			return TEXT("NAK");
		case 22:
			return TEXT("SYN");
		case 23:
			return TEXT("ETB");
		case 24:
			return TEXT("CAN");
		case 25:
			return TEXT("EM");
		case 26:
			return TEXT("SUB");
		case 27:
			return TEXT("ESC");
		case 28:
			return TEXT("FS");
		case 29:
			return TEXT("GS");
		case 30:
			return TEXT("RS");
		case 31:
			return TEXT("US");
		case 32:
			return TEXT("Space");
		case 127:
			return TEXT("DEL");
		/*
		case 0:
			return TEXT("NULL");
		case 0:
			return TEXT("NULL");
		case 0:
			return TEXT("NULL");
		case 0:
			return TEXT("NULL");
		case 0:
			return TEXT("NULL");
		*/
	}
	return TEXT("");	
}

void ConversionPanel::setValueExcept(int exceptID, size_t value)
{
	const int strLen = 1024;
	TCHAR str2Display[strLen];
	if (exceptID != ID_ASCII_EDIT)
	{
		if (value <= 255)
		{
			TCHAR ascii2Display[2];
			ascii2Display[0] = (TCHAR)value;
			ascii2Display[1] = '\0';
			setItemText(ID_ASCII_EDIT, ascii2Display); // Qt port: in place of WM_SETTEXT
			setItemText(ID_ASCII_INFO_STATIC, getAsciiInfo((unsigned char)value)); // Qt port: in place of WM_SETTEXT
		}
		else
			setItemText(ID_ASCII_INFO_STATIC, TEXT("")); // Qt port: in place of WM_SETTEXT
	}
	else
	{
		setItemText(ID_ASCII_INFO_STATIC, getAsciiInfo((unsigned char)value)); // Qt port: in place of WM_SETTEXT
	}

	if (exceptID != ID_DEC_EDIT)
	{
		generic_sprintf(str2Display, strLen, TEXT("%zd"), (size_t)value);
		setItemText(ID_DEC_EDIT, str2Display); // Qt port: in place of WM_SETTEXT
	}
	if (exceptID != ID_HEX_EDIT)
	{
		generic_sprintf(str2Display, strLen, TEXT("%zX"), (size_t)value);
		setItemText(ID_HEX_EDIT, str2Display); // Qt port: in place of WM_SETTEXT
	}
	if (exceptID != ID_OCT_EDIT)
	{
		generic_sprintf(str2Display, strLen, TEXT("%zo"), (size_t)value);
		setItemText(ID_OCT_EDIT, str2Display); // Qt port: in place of WM_SETTEXT
	}
	if (exceptID != ID_BIN_EDIT)
	{
		char str2DisplayA[1234];
		itoa(int(value), str2DisplayA, 2);
		setItemText(ID_BIN_EDIT, generic_string(str2DisplayA, str2DisplayA + strlen(str2DisplayA))); // Qt port: in place of WM_SETTEXT
	}
}


void ConversionPanel::setValueFrom(int id)
{
	lock = true;
	const int inStrSize = 256;
	TCHAR intStr[inStrSize];
	wcsncpy_s(intStr, inStrSize, itemText(id).c_str(), _TRUNCATE); // Qt port: in place of WM_GETTEXT

	resetExcept(id);

	if (!intStr[0])
	{
		lock = false;
		return;
	}

	if (!qualified(intStr, id))
	{
		lock = false;
		int len = lstrlen(intStr);
		intStr[len-1] = '\0';
		setItemText(id, intStr); // Qt port: in place of WM_SETTEXT
		if (auto* edit = qobject_cast<QLineEdit*>(item(id))) // Qt port: in place of EM_SETSEL
			edit->setCursorPosition(len-1);
		return;
	}

	int base = 10;
	switch (id)
	{
		case ID_ASCII_EDIT :
		{
			base = 0;
			
		}
		break;
		case ID_DEC_EDIT :
		{
			base = 10;	
			
		}
		break;
		case ID_HEX_EDIT :
		{
			base = 16;
			
		}
		break;
		case ID_OCT_EDIT :
		{
			base = 8;
			
		}
		break;
		case ID_BIN_EDIT :
		{
			base = 2;
			
		}
		break;
	}

	unsigned long v = 0;
	if (!base)
		v = intStr[0];
	else
		v = generic_strtoul(intStr, NULL, base);

	setValueExcept(id, v);
	lock = false;
}

void ConversionPanel::insertToNppFrom(int id)
{
	const int inStrSize = 256;
	char intStr[inStrSize];
	strncpy_s(intStr, inStrSize, QString::fromStdWString(itemText(id)).toUtf8().constData(), _TRUNCATE); // Qt port: in place of WM_GETTEXT
	HWND hCurrScintilla = getCurrentScintillaHandle();
	::SendMessage(hCurrScintilla, SCI_REPLACESEL, 0, (LPARAM)intStr);	
}

void ConversionPanel::copyToClipboardFrom(int id)
{
	const int intStrMaxSize = 256;
	char intStr[intStrMaxSize];
	size_t intStrLen = 0;
	if (id == ID_ASCII_EDIT)
	{
		int v = getAsciiUcharFromDec();
		if (v == -1) return;
		intStr[0] = static_cast<char>(v);
		intStr[1] = '\0';
		intStrLen = 1;
	}
	else
	{
		strncpy_s(intStr, intStrMaxSize, QString::fromStdWString(itemText(id)).toUtf8().constData(), _TRUNCATE); // Qt port: in place of WM_GETTEXT
		intStrLen = strlen(intStr);
	}
	// Qt port: the bytes in the format of Notepad++ for Paste Binary Content, in place of CF_TEXT with the length in
	// a format of its own, whose name Notepad++ does not read. Other applications get the text, the one character
	// of the ASCII field as the field shows it.
	auto* mimeData = new QMimeData;
	mimeData->setData(QString::fromWCharArray(CF_NPPBINARY), QByteArray(intStr, static_cast<qsizetype>(intStrLen)));
	if (id == ID_ASCII_EDIT)
	{
		if (intStr[0])
			mimeData->setText(QString(QChar(static_cast<unsigned char>(intStr[0]))));
	}
	else
		mimeData->setText(QString::fromLatin1(intStr, static_cast<qsizetype>(intStrLen)));
	QApplication::clipboard()->setMimeData(mimeData);
}

int ConversionPanel::getAsciiUcharFromDec()
{
	const int inStrSize = 256;
	TCHAR intStr[inStrSize];
	wcsncpy_s(intStr, inStrSize, itemText(ID_DEC_EDIT).c_str(), _TRUNCATE); // Qt port: in place of WM_GETTEXT
	int v = generic_strtoul(intStr, NULL, 10);
	if (v > 255)
		return -1;
	return v;
}