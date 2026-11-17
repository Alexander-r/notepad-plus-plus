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


#include "columnEditor.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Scintilla.h>

#include "Common.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "Window.h"
#include "columnEditor_rc.h"
#include "resource.h"
#include "ui_ColumneditDlg.h"

void ColumnEditorDlg::init(HINSTANCE hInst, HWND hPere, ScintillaEditView **ppEditView)
{
	Window::init(hInst, hPere);
	if (!ppEditView)
		throw std::runtime_error("StaticDialog::init : ppEditView is null.");
	_ppEditView = ppEditView;
}

void ColumnEditorDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::ColumneditDlg ui;
		ui.setupUi(dialog);
		setupControls();

		const auto onClick = [dialog](QAbstractButton* button, auto handler)
		{
			QObject::connect(button, &QAbstractButton::clicked, dialog, handler);
		};
		onClick(ui.cancel, [this]() { display(false); });
		onClick(ui.ok, [this]() { onOk(); });
		onClick(ui.colTextRadio, [this]()
		{
			NppParameters::getInstance()._columnEditParam._mainChoice = activeText;
			switchTo(activeText);
		});
		onClick(ui.colNumRadio, [this]()
		{
			NppParameters::getInstance()._columnEditParam._mainChoice = activeNumeric;
			switchTo(activeNumeric);
		});
		onClick(ui.colDecRadio, [this]() { onFormatChosen(IDC_COL_DEC_RADIO); });
		onClick(ui.colHexRadio, [this]() { onFormatChosen(IDC_COL_HEX_RADIO); });
		onClick(ui.colOctRadio, [this]() { onFormatChosen(IDC_COL_OCT_RADIO); });
		onClick(ui.colBinRadio, [this]() { onFormatChosen(IDC_COL_BIN_RADIO); });

		QObject::connect(ui.colTextEdit, &QLineEdit::textChanged, dialog, [this](const QString& text)
		{
			NppParameters::getInstance()._columnEditParam._insertedTextContent = text.toStdWString();
			enableItem(IDOK, !text.isEmpty());
		});
		QObject::connect(ui.colInitnumEdit, &QLineEdit::textEdited, dialog, [this]() { onNumericFieldEdited(IDC_COL_INITNUM_EDIT); });
		QObject::connect(ui.colIncreasenumEdit, &QLineEdit::textEdited, dialog, [this]() { onNumericFieldEdited(IDC_COL_INCREASENUM_EDIT); });
		QObject::connect(ui.colRepeatnumEdit, &QLineEdit::textEdited, dialog, [this]() { onNumericFieldEdited(IDC_COL_REPEATNUM_EDIT); });

		QObject::connect(ui.colLeadingCombo, &QComboBox::activated, dialog, [this]()
		{
			NppParameters::getInstance()._columnEditParam._leadingChoice = getLeading();
		});
		QObject::connect(ui.colHexucCombo, &QComboBox::activated, dialog, [this]()
		{
			using enum NumBase;
			ColumnEditorParam& colEditParam = NppParameters::getInstance()._columnEditParam;
			if (colEditParam._formatChoice == BASE_16 || colEditParam._formatChoice == BASE_16_UPPERCASE)
				colEditParam._formatChoice = getHexCase();

			setNumericFields(colEditParam);	// want the GUI fields to update case when combobox changes
		});

		goToCenter();
	}

	const bool isTextMode = isCheckedOrNot(IDC_COL_TEXT_RADIO);
	display();
	dlgItem(isTextMode ? IDC_COL_TEXT_EDIT : IDC_COL_INITNUM_EDIT)->setFocus();
}

void ColumnEditorDlg::setupControls()
{
	ColumnEditorParam colEditParam = NppParameters::getInstance()._columnEditParam;

	setItemText(IDC_COL_TEXT_EDIT, colEditParam._insertedTextContent);

	setNumericFields(colEditParam);

	QComboBox* leadingCombo = combo(IDC_COL_LEADING_COMBO);
	leadingCombo->addItem(QStringLiteral("None"));
	leadingCombo->addItem(QStringLiteral("Zeros"));
	leadingCombo->addItem(QStringLiteral("Spaces"));
	int curSel = 0;
	switch (colEditParam._leadingChoice)
	{
		using enum ColumnEditorParam::leadingChoice;
		case noneLeading: { curSel = 0; break; }
		case zeroLeading: { curSel = 1; break; }
		case spaceLeading: { curSel = 2; break; }
		default : { curSel = 0; break; }
	}
	leadingCombo->setCurrentIndex(curSel);

	int format = IDC_COL_DEC_RADIO;
	using enum NumBase;
	if ((colEditParam._formatChoice == BASE_16) || (colEditParam._formatChoice == BASE_16_UPPERCASE))	// either BASE_16 or BASE_16_UPPERCASE
		format = IDC_COL_HEX_RADIO;
	else if (colEditParam._formatChoice == BASE_08)
		format = IDC_COL_OCT_RADIO;
	else if (colEditParam._formatChoice == BASE_02)
		format = IDC_COL_BIN_RADIO;

	setChecked(format);

	// populate the Hex-Case dropdown and activate correct case
	QComboBox* hexCombo = combo(IDC_COL_HEXUC_COMBO);
	hexCombo->addItem(QStringLiteral("a-f"));
	hexCombo->addItem(QStringLiteral("A-F"));
	hexCombo->setCurrentIndex((colEditParam._formatChoice == BASE_16_UPPERCASE) ? 1 : 0);	// activate correct case

	switchTo(colEditParam._mainChoice);
}

void ColumnEditorDlg::onOk()
{
	(*_ppEditView)->execute(SCI_BEGINUNDOACTION);

	static constexpr int stringSize = 1024;

	bool isTextMode = isCheckedOrNot(IDC_COL_TEXT_RADIO);

	if (isTextMode)
	{
		const std::string str = wstring2string(getItemText(IDC_COL_TEXT_EDIT));

		display(false);

		if ((*_ppEditView)->execute(SCI_SELECTIONISRECTANGLE) || (*_ppEditView)->execute(SCI_GETSELECTIONS) > 1)
		{
			ColumnModeInfos colInfos = (*_ppEditView)->getColumnModeSelectInfo();
			std::sort(colInfos.begin(), colInfos.end(), SortInPositionOrder());
			(*_ppEditView)->columnReplace(colInfos, str.c_str());
			std::sort(colInfos.begin(), colInfos.end(), SortInSelectOrder());
			(*_ppEditView)->setMultiSelections(colInfos);
		}
		else
		{
			auto cursorPos = (*_ppEditView)->execute(SCI_GETCURRENTPOS);
			auto cursorCol = (*_ppEditView)->execute(SCI_GETCOLUMN, cursorPos);
			auto cursorLine = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, cursorPos);
			auto endPos = (*_ppEditView)->execute(SCI_GETLENGTH);
			auto endLine = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, endPos);

			static constexpr int lineAllocatedLen = 1024;
			auto line = std::make_unique<char[]>(lineAllocatedLen);

			for (size_t i = cursorLine; i <= static_cast<size_t>(endLine); ++i)
			{
				auto lineBegin = (*_ppEditView)->execute(SCI_POSITIONFROMLINE, i);
				auto lineEnd = (*_ppEditView)->execute(SCI_GETLINEENDPOSITION, i);

				auto lineEndCol = (*_ppEditView)->execute(SCI_GETCOLUMN, lineEnd);
				auto lineLen = lineEnd - lineBegin + 1;

				if (lineLen > lineAllocatedLen)
				{
					line.reset(new char[lineLen]);
				}
				(*_ppEditView)->getGenericText(line.get(), lineLen, lineBegin, lineEnd);
				std::string s2r(line.get());

				if (lineEndCol < cursorCol)
				{
					std::string s_space(cursorCol - lineEndCol, ' ');
					s2r.append(s_space);
					s2r.append(str);
				}
				else
				{
					auto posAbs2Start = (*_ppEditView)->execute(SCI_FINDCOLUMN, i, cursorCol);
					auto posRelative2Start = posAbs2Start - lineBegin;
					if (posRelative2Start > static_cast<long long>(s2r.length()))
						posRelative2Start = s2r.length();

					s2r.insert(posRelative2Start, str);
				}
				(*_ppEditView)->replaceTarget(s2r, lineBegin, lineEnd);
			}
		}
	}
	else
	{
		ColumnEditorParam colEditParam = NppParameters::getInstance()._columnEditParam;

		wchar_t str[stringSize]{};
		const auto readField = [this, &str](int id)
		{
			wcsncpy(str, getItemText(id).c_str(), stringSize - 1);
		};
		readField(IDC_COL_INITNUM_EDIT);

		const int initialNumber = getNumericFieldValueFromText(colEditParam._formatChoice, str);
		if (initialNumber == -1)
		{
			showValidationError(IDC_COL_INITNUM_EDIT, colEditParam._formatChoice, str);
			(*_ppEditView)->execute(SCI_ENDUNDOACTION);
			return;
		}

		readField(IDC_COL_INCREASENUM_EDIT);
		const int increaseNumber = getNumericFieldValueFromText(colEditParam._formatChoice, str);
		if (increaseNumber == -1)
		{
			showValidationError(IDC_COL_INCREASENUM_EDIT, colEditParam._formatChoice, str);
			(*_ppEditView)->execute(SCI_ENDUNDOACTION);
			return;
		}

		readField(IDC_COL_REPEATNUM_EDIT);
		int repeat = getNumericFieldValueFromText(colEditParam._formatChoice, str);
		if (repeat == -1)
		{
			showValidationError(IDC_COL_REPEATNUM_EDIT, colEditParam._formatChoice, str);
			(*_ppEditView)->execute(SCI_ENDUNDOACTION);
			return;
		}

		if (repeat == 0)
		{
			repeat = 1; // Without this we might get an infinite loop while calculating the set "numbers" below.
		}

		const NumBase format = getFormat();
		display(false);

		if ((*_ppEditView)->execute(SCI_SELECTIONISRECTANGLE) || (*_ppEditView)->execute(SCI_GETSELECTIONS) > 1)
		{
			ColumnModeInfos colInfos = (*_ppEditView)->getColumnModeSelectInfo();

			// If there is no column mode info available, no need to do anything
			// If required a message can be shown to user, that select column properly or something similar
			if (colInfos.size() > 0)
			{
				std::sort(colInfos.begin(), colInfos.end(), SortInPositionOrder());
				(*_ppEditView)->columnReplace(colInfos, initialNumber, increaseNumber, repeat, format, getLeading());
				std::sort(colInfos.begin(), colInfos.end(), SortInSelectOrder());
				(*_ppEditView)->setMultiSelections(colInfos);
			}
		}
		else
		{
			auto cursorPos = (*_ppEditView)->execute(SCI_GETCURRENTPOS);
			auto cursorCol = (*_ppEditView)->execute(SCI_GETCOLUMN, cursorPos);
			auto cursorLine = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, cursorPos);
			auto endPos = (*_ppEditView)->execute(SCI_GETLENGTH);
			auto endLine = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, endPos);

			// Compute the numbers to be placed at each column.
			std::vector<size_t> numbers;

			size_t curNumber = initialNumber;
			const size_t kiMaxSize = 1 + static_cast<size_t>(endLine) - static_cast<size_t>(cursorLine);
			while (numbers.size() < kiMaxSize)
			{
				for (int i = 0; i < repeat; i++)
				{
					numbers.push_back(curNumber);

					if (numbers.size() >= kiMaxSize)
						break;
				}
				curNumber += increaseNumber;
			}

			static constexpr int lineAllocatedLen = 1024;
			auto line = std::make_unique<wchar_t[]>(lineAllocatedLen);

			size_t base = 10;
			bool useUppercase = false;
			using enum NumBase;
			if (format == BASE_16)
				base = 16;
			else if (format == BASE_08)
				base = 8;
			else if (format == BASE_02)
				base = 2;
			else if (format == BASE_16_UPPERCASE)
			{
				base = 16;
				useUppercase = true;
			}

			size_t endNumber = *numbers.rbegin();
			size_t nbEnd = getNbDigits(endNumber, base);
			size_t nbInit = getNbDigits(initialNumber, base);
			size_t nb = std::max<size_t>(nbInit, nbEnd);

			for (size_t i = cursorLine; i <= static_cast<size_t>(endLine); ++i)
			{
				auto lineBegin = (*_ppEditView)->execute(SCI_POSITIONFROMLINE, i);
				auto lineEnd = (*_ppEditView)->execute(SCI_GETLINEENDPOSITION, i);

				auto lineEndCol = (*_ppEditView)->execute(SCI_GETCOLUMN, lineEnd);
				auto lineLen = lineEnd - lineBegin + 1;

				if (lineLen > lineAllocatedLen)
				{
					line.reset(new wchar_t[lineLen]);
				}
				(*_ppEditView)->getGenericText(line.get(), lineLen, lineBegin, lineEnd);

				std::wstring s2r(line.get());

				//
				// Calcule wstring
				//
				variedFormatNumber2String<wchar_t>(str, stringSize, numbers.at(i - cursorLine), base, useUppercase, nb, getLeading());

				if (lineEndCol < cursorCol)
				{
					std::wstring s_space(cursorCol - lineEndCol, ' ');
					s2r.append(s_space);
					s2r.append(str);
				}
				else
				{
					auto posAbs2Start = (*_ppEditView)->execute(SCI_FINDCOLUMN, i, cursorCol);
					auto posRelative2Start = posAbs2Start - lineBegin;
					if (posRelative2Start > static_cast<long long>(s2r.length()))
						posRelative2Start = s2r.length();

					s2r.insert(posRelative2Start, str);
				}

				(*_ppEditView)->replaceTarget(s2r.c_str(), static_cast<int>(lineBegin), static_cast<int>(lineEnd));
			}
		}
	}
	(*_ppEditView)->execute(SCI_ENDUNDOACTION);
	(*_ppEditView)->grabFocus();
	(*_ppEditView)->execute(SCI_ENDUNDOACTION) ;
	(*_ppEditView)->grabFocus();
}

void ColumnEditorDlg::onFormatChosen(int id)
{
	ColumnEditorParam& colEditParam = NppParameters::getInstance()._columnEditParam;
	using enum NumBase;
	colEditParam._formatChoice = BASE_10; // dec
	if (id == IDC_COL_HEX_RADIO)
		colEditParam._formatChoice = getHexCase();	// will pick appropriate UC or LC version of hex
	else if (id == IDC_COL_OCT_RADIO)
		colEditParam._formatChoice = BASE_08;
	else if (id == IDC_COL_BIN_RADIO)
		colEditParam._formatChoice = BASE_02;

	setNumericFields(colEditParam);	// reformat the field text to be based on the new radix
	enableItem(IDC_COL_HEXUC_COMBO, id == IDC_COL_HEX_RADIO);	// enable combobox only for hex
}

void ColumnEditorDlg::onNumericFieldEdited(int id)
{
	ColumnEditorParam& colEditParam = NppParameters::getInstance()._columnEditParam;
	int& value = (id == IDC_COL_INITNUM_EDIT) ? colEditParam._initialNum : (id == IDC_COL_INCREASENUM_EDIT) ? colEditParam._increaseNum : colEditParam._repeatNum;

	const std::wstring str = getItemText(id);
	if (str.empty())
	{
		value = -1;
		return;
	}

	const int num = getNumericFieldValueFromText(colEditParam._formatChoice, str);
	if (num == -1)
	{
		setNumericFields(colEditParam);	// reformat the strings to eliminate error
		showValidationError(id, colEditParam._formatChoice, str);
		return;
	}

	value = num;
}

void ColumnEditorDlg::switchTo(bool toText)
{
	enableItem(IDC_COL_TEXT_EDIT, toText);
	setChecked(IDC_COL_TEXT_RADIO, toText);

	setChecked(IDC_COL_NUM_RADIO, !toText);
	enableItem(IDC_COL_INITNUM_EDIT, !toText);
	enableItem(IDC_COL_INCREASENUM_EDIT, !toText);
	enableItem(IDC_COL_REPEATNUM_EDIT, !toText);
	enableItem(IDC_COL_DEC_RADIO, !toText);
	enableItem(IDC_COL_HEX_RADIO, !toText);
	enableItem(IDC_COL_OCT_RADIO, !toText);
	enableItem(IDC_COL_BIN_RADIO, !toText);
	enableItem(IDC_COL_LEADING_COMBO, !toText);
	enableItem(IDOK, !toText || !NppParameters::getInstance()._columnEditParam._insertedTextContent.empty());

	enableItem(IDC_COL_HEXUC_COMBO, !toText && isCheckedOrNot(IDC_COL_HEX_RADIO));

	enableItem(IDC_COL_INITNUM_STATIC, !toText);
	enableItem(IDC_COL_INCRNUM_STATIC, !toText);
	enableItem(IDC_COL_REPEATNUM_STATIC, !toText);
	enableItem(IDC_COL_LEADING_STATIC, !toText);
	enableItem(IDC_COL_FORMAT_GRP_STATIC, !toText);

	dlgItem(toText ? IDC_COL_TEXT_EDIT : IDC_COL_INITNUM_EDIT)->setFocus();
}

NumBase ColumnEditorDlg::getFormat()
{
	using enum NumBase;
	NumBase f = BASE_10; // Dec by default
	if (isCheckedOrNot(IDC_COL_HEX_RADIO))
		f = getHexCase();	// will give BASE_16 or BASE_16_UC, depending on case selector
	else if (isCheckedOrNot(IDC_COL_OCT_RADIO))
		f = BASE_08;
	else if (isCheckedOrNot(IDC_COL_BIN_RADIO))
		f = BASE_02;
	return f;
}


NumBase ColumnEditorDlg::getHexCase()
{
	using enum NumBase;
	const int curSel = combo(IDC_COL_HEXUC_COMBO)->currentIndex();
	return (curSel == 1) ? BASE_16_UPPERCASE : BASE_16;
}

ColumnEditorParam::leadingChoice ColumnEditorDlg::getLeading()
{
	using enum ColumnEditorParam::leadingChoice;
	ColumnEditorParam::leadingChoice leading = noneLeading;
	const int curSel = combo(IDC_COL_LEADING_COMBO)->currentIndex();
	switch (curSel)
	{
		case 0:
		default:
		{
			leading = noneLeading;
			break;
		}
		case 1:
		{
			leading = zeroLeading;
			break;
		}
		case 2:
		{
			leading = spaceLeading;
			break;
		}
	}
	return leading;
}

void ColumnEditorDlg::setNumericFields(const ColumnEditorParam& colEditParam)
{
	using enum NumBase;
	if (colEditParam._formatChoice == BASE_10)
	{
		if (colEditParam._initialNum != -1)
			setItemText(IDC_COL_INITNUM_EDIT, std::to_wstring(colEditParam._initialNum));
		else
			setItemText(IDC_COL_INITNUM_EDIT, L"");

		if (colEditParam._increaseNum != -1)
			setItemText(IDC_COL_INCREASENUM_EDIT, std::to_wstring(colEditParam._increaseNum));
		else
			setItemText(IDC_COL_INCREASENUM_EDIT, L"");

		if (colEditParam._repeatNum != -1)
			setItemText(IDC_COL_REPEATNUM_EDIT, std::to_wstring(colEditParam._repeatNum));
		else
			setItemText(IDC_COL_REPEATNUM_EDIT, L"");
	}
	else
	{
		size_t base = 10;
		switch (colEditParam._formatChoice)
		{
			case BASE_16:		// hex
			case BASE_16_UPPERCASE:	// or hex w/ uppercase A-F
				base = 16;
				break;
			case BASE_08:		// oct
				base = 8;
				break;
			case BASE_02:		// bin
				base = 2;
				break;
			default:
				base = 10;
				break;
		}
		bool useUpper = (colEditParam._formatChoice == BASE_16_UPPERCASE);

		static constexpr int stringSize = 1024;
		wchar_t str[stringSize]{};

		if (colEditParam._initialNum != -1)
		{
			variedFormatNumber2String<wchar_t>(str, stringSize, colEditParam._initialNum, base, useUpper, getNbDigits(colEditParam._initialNum, base), getLeading());
			setItemText(IDC_COL_INITNUM_EDIT, str);
		}
		else
			setItemText(IDC_COL_INITNUM_EDIT, L"");

		if (colEditParam._increaseNum != -1)
		{
			variedFormatNumber2String<wchar_t>(str, stringSize, colEditParam._increaseNum, base, useUpper, getNbDigits(colEditParam._increaseNum, base), getLeading());
			setItemText(IDC_COL_INCREASENUM_EDIT, str);
		}
		else
			setItemText(IDC_COL_INCREASENUM_EDIT, L"");

		if (colEditParam._repeatNum != -1)
		{
			variedFormatNumber2String<wchar_t>(str, stringSize, colEditParam._repeatNum, base, useUpper, getNbDigits(colEditParam._repeatNum, base), getLeading());
			setItemText(IDC_COL_REPEATNUM_EDIT, str);
		}
		else
			setItemText(IDC_COL_REPEATNUM_EDIT, L"");

	}
	return;
}

// Convert the string to an integer, depending on base
int ColumnEditorDlg::getNumericFieldValueFromText(NumBase formatChoice, const std::wstring& str)
{
	if (str.empty())
		return 0;

	int base = 0;

	switch (formatChoice)
	{
		using enum NumBase;
		case BASE_16:
		case BASE_16_UPPERCASE:
			base = 16;
			break;
		case BASE_08:
			base = 8;
			break;
		case BASE_02:
			base = 2;
			break;
		default:
			base = 10;
			break;
	}

	size_t end = 0;
	// convert string in base to int value; on error, return -1
	try {
		const int num = std::stoi(str, &end, base);
		if (end != str.length())
			return -1;
		return num;
	}
	catch (std::invalid_argument const&)
	{
		return -1;
	}
	catch (std::out_of_range const&)
	{
		return -1;
	}
}

void ColumnEditorDlg::showValidationError(int field, NumBase formatChoice, const std::wstring& str)
{
	const wchar_t* wcRadixNote = nullptr;
	switch (formatChoice)
	{
		using enum NumBase;
		case BASE_16:
		case BASE_16_UPPERCASE:
			wcRadixNote = L"Hex numbers use 0-9, A-F!";
			break;
		case BASE_08:
			wcRadixNote = L"Oct numbers only use 0-7!";
			break;
		case BASE_02:
			wcRadixNote = L"Bin numbers only use 0-1!";
			break;
		default:
			wcRadixNote = L"Decimal numbers only use 0-9!";
			break;
	}

	QString message = QStringLiteral("Invalid Numeric Entry\n");
	if (!str.empty())
		message += QStringLiteral("Entered string \"%1\":\n").arg(QString::fromStdWString(str));
	message += QString::fromWCharArray(wcRadixNote);

	// A red flash of the field, and the reason next to it
	QWidget* control = dlgItem(field);
	QToolTip::showText(control->mapToGlobal(QPoint(0, control->height())), message, control, QRect(), 3500);
	control->setStyleSheet(QStringLiteral("background-color: red;"));
	QTimer::singleShot(250, control, [control]() { control->setStyleSheet(QString{}); });
}
