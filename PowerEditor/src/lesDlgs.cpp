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


#include "lesDlgs.h"

#include <QAbstractButton>

#include "menuCmdID.h"
#include "NppMessages.h"
#include "resource.h"
#include "ui_ButtonDlg.h"

void ButtonDlg::doDialog(bool isRTL)
{
	if (!isCreated())
	{
		QDialog* dialog = createDialogWidget(isRTL);
		Ui::ButtonDlg ui;
		ui.setupUi(dialog);

		// Part of the main window, in its corner. A window of its own would get a frame from the desktop.
		dialog->setWindowFlags(Qt::Widget);
		const int side = dialog->fontMetrics().height() + 4;
		ui.restoreButton->setFixedSize(side, side);
		dialog->adjustSize();

		QObject::connect(button(IDC_RESTORE_BUTTON), &QAbstractButton::clicked, dialog, [this]()
		{
			int bs = getButtonStatus();

			bool isDistractionFree = (bs & buttonStatus_distractionFree) != 0;
			bool isFullScreen = (bs & buttonStatus_fullscreen) != 0;
			bool isPostIt = (bs & buttonStatus_postit) != 0;
			int cmd = 0;
			if (isDistractionFree)
			{
				cmd = IDM_VIEW_DISTRACTIONFREE;
			}
			else if (isFullScreen && isPostIt)
			{
				// remove postit firstly
				cmd = IDM_VIEW_POSTIT;
			}
			else if (isFullScreen)
			{
				cmd = IDM_VIEW_FULLSCREENTOGGLE;
			}
			else if (isPostIt)
			{
				cmd = IDM_VIEW_POSTIT;
			}
			::SendMessage(_hParent, WM_COMMAND, cmd, 0);
			display(false);
		});
	}
	display();
}
