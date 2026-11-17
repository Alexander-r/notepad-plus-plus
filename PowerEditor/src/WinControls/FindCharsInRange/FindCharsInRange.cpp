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


#include "FindCharsInRange.h"

#include <QAbstractButton>
#include <QDialog>
#include <QLineEdit>

#include "Parameters.h"
#include "findCharsInRange_rc.h"
#include "localization.h"
#include "ui_FindcharactersDlg.h"

void FindCharsInRangeDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::FindcharactersDlg ui;
		ui.setupUi(dialog);

		ui.rangestartEdit->setMaxLength(3);
		ui.rangeendEdit->setMaxLength(3);
		ui.nonascciRadio->setChecked(true);
		ui.findcharDirdown->setChecked(true);
		ui.rangestartEdit->setText(QStringLiteral("0"));
		ui.rangeendEdit->setText(QStringLiteral("255"));
		ui.rangestartEdit->setEnabled(false);
		ui.rangeendEdit->setEnabled(false);
		goToCenter();

		// The "my range" fields only apply when that choice is on
		QObject::connect(ui.myrangeRadio, &QAbstractButton::toggled, ui.rangestartEdit, &QWidget::setEnabled);
		QObject::connect(ui.myrangeRadio, &QAbstractButton::toggled, ui.rangeendEdit, &QWidget::setEnabled);
		QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.findcharNext, &QAbstractButton::clicked, dialog, [this]() { onFindNext(); });
	}
	display();
}

void FindCharsInRangeDlg::onFindNext()
{
	intptr_t currentPos = (*_ppEditView)->execute(SCI_GETCURRENTPOS);
	unsigned char startRange = 0;
	unsigned char endRange = 255;
	bool direction = dirDown;
	bool isWrap = true;
	if (!getRangeFromUI(startRange, endRange))
	{
		//STOP!
		NppParameters::getInstance().getNativeLangSpeaker()->messageBox("FindCharRangeValueError",
			_hSelf,
			L"You should type between 0 and 255.",
			L"Range Value problem",
			MB_OK);
		return;
	}
	getDirectionFromUI(direction, isWrap);
	findCharInRange(startRange, endRange, currentPos, direction, isWrap);
}

bool FindCharsInRangeDlg::findCharInRange(unsigned char beginRange, unsigned char endRange, intptr_t startPos, bool direction, bool wrap)
{
	size_t totalSize = (*_ppEditView)->getCurrentDocLen();
	if (startPos == -1)
		startPos = direction == dirDown ? 0 : totalSize - 1;
	if (startPos > static_cast<intptr_t>(totalSize))
		return false;

	char *content = new char[totalSize + 1];
	(*_ppEditView)->getText(content, 0, totalSize);

	bool isFound = false;
	size_t found = 0;

	for (int64_t i = startPos - (direction == dirDown ? 0 : 1);
		(direction == dirDown) ? i < static_cast<int64_t>(totalSize) : i >= 0 ;
		(direction == dirDown) ? (++i) : (--i))
	{
		if (static_cast<unsigned char>(content[i]) >= beginRange && static_cast<unsigned char>(content[i]) <= endRange)
		{
			found = static_cast<size_t>(i);
			isFound = true;
			break;
		}
	}
	
	if (!isFound)
	{
		if (wrap)
		{
			for (int64_t i = (direction == dirDown ? 0: totalSize - 1); 
				(direction == dirDown) ? i < static_cast<int64_t>(totalSize) : i >= 0 ;
				(direction == dirDown) ? (++i) : (--i))
			{
				if (static_cast<unsigned char>(content[i]) >= beginRange && static_cast<unsigned char>(content[i]) <= endRange)
				{
					found = static_cast<size_t>(i);
					isFound = true;
					break;
				}
			}
		}
	}

	if (isFound)
	{
		//printInt(found);
		auto sci_line = (*_ppEditView)->execute(SCI_LINEFROMPOSITION, found);
		(*_ppEditView)->execute(SCI_ENSUREVISIBLE, sci_line);
		(*_ppEditView)->execute(SCI_GOTOPOS, found);
		(*_ppEditView)->execute(SCI_SETSEL, (direction == dirDown)? found : found+1, (direction == dirDown) ? found + 1 : found);
	}
	delete [] content;
	return isFound;
}

void FindCharsInRangeDlg::getDirectionFromUI(bool & whichDirection, bool & isWrap)
{
	whichDirection = isCheckedOrNot(ID_FINDCHAR_DIRUP);
	isWrap = isCheckedOrNot(ID_FINDCHAR_WRAP);
}

bool FindCharsInRangeDlg::getRangeFromUI(unsigned char & startRange, unsigned char & endRange)
{
	if (isCheckedOrNot(IDC_NONASCCI_RADIO))
	{
		startRange = 128;
		endRange = 255;
		return true;
	}

	if (isCheckedOrNot(IDC_ASCCI_RADIO))
	{
		startRange = 0;
		endRange = 127;
		return true;
	}
	
	if (isCheckedOrNot(IDC_MYRANGE_RADIO))
	{
		bool startBool = false;
		bool endBool = false;
		const int start = QString::fromStdWString(getItemText(IDC_RANGESTART_EDIT)).toUInt(&startBool);
		const int end = QString::fromStdWString(getItemText(IDC_RANGEEND_EDIT)).toUInt(&endBool);

		if (!startBool || !endBool)
			return false;
		if (start > 255 || end > 255)
			return false;
		if (start > end)
			return false;
		startRange = static_cast<unsigned char>(start);
		endRange = static_cast<unsigned char>(end);
		return true;
	}
	
	return false;
}
