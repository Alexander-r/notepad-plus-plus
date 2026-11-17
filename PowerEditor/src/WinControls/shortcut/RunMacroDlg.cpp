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

// created by Daniel Volk mordorpost@volkarts.com


#include "RunMacroDlg.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QIntValidator>
#include <QLineEdit>

#include <algorithm>

#include "Notepad_plus_msgs.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "resource.h"
#include "ui_RunMacroDlg.h"

void RunMacroDlg::initMacroList()
{
	if (!isCreated()) return;

	NppParameters& nppParam = NppParameters::getInstance();
	const std::vector<MacroShortcut>& macroList = nppParam.getMacroList();

	QComboBox* macroCombo = combo(IDC_MACRO_COMBO);
	macroCombo->clear();

	if (static_cast<MacroStatus>(::SendMessage(_hParent, NPPM_GETCURRENTMACROSTATUS, 0, 0)) == MacroStatus::RecordingStopped)
		macroCombo->addItem(QStringLiteral("Current recorded macro"));

	for (size_t i = 0, len = macroList.size(); i < len ; ++i)
		macroCombo->addItem(QString::fromStdWString(string2wstring(macroList[i].getName(), CP_UTF8)));

	macroCombo->setCurrentIndex(0);
	_macroIndex = 0;
}

void RunMacroDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::RunMacroDlg ui;
		ui.setupUi(dialog);

		initMacroList();
		ui.mRunTimes->setValidator(new QIntValidator(0, 9999, ui.mRunTimes));
		ui.mRunTimes->setText(QString::number(_times));
		ui.mRunMulti->setChecked(true);
		goToCenter();

		QObject::connect(ui.mRunMulti, &QAbstractButton::toggled, ui.mRunTimes, &QWidget::setEnabled);
		QObject::connect(ui.mRunMulti, &QAbstractButton::toggled, ui.timesStatic, &QWidget::setEnabled);
		QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { display(false); });
		QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]()
		{
			if (combo(IDC_MACRO_COMBO)->count() > 0)
				::SendMessage(_hParent, WM_MACRODLGRUNMACRO, 0, 0);
		});
		QObject::connect(ui.macroCombo, &QComboBox::currentIndexChanged, dialog, [this](int index) { _macroIndex = index; });
		QObject::connect(ui.mRunTimes, &QLineEdit::textChanged, dialog, [this](const QString& text)
		{
			_times = std::max(text.toInt(), 1);
		});
		QObject::connect(ui.mRunTimes, &QLineEdit::editingFinished, dialog, [edit = ui.mRunTimes]()
		{
			if (edit->text().toInt() < 1)
				edit->setText(QStringLiteral("1"));
		});
	}
	else
	{
		// Shortcut might have been updated for current session
		// So reload the macro list (issue #4526)
		initMacroList();
	}
	display();
}

int RunMacroDlg::getMacro2Exec() const
{
	bool isCurMacroPresent = static_cast<MacroStatus>(::SendMessage(_hParent, NPPM_GETCURRENTMACROSTATUS, 0, 0)) == MacroStatus::RecordingStopped;
	return isCurMacroPresent?(_macroIndex - 1):_macroIndex;
}
