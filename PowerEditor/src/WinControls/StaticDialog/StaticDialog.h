// This file is part of Notepad++ project
// Copyright (C)2024 Don HO <don.h@free.fr>

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


#pragma once

#include <string>

#include <QPointer>

#include "Window.h"
#include "dpiManagerV2.h"

class QAbstractButton;
class QComboBox;
class QDialog;
class QLineEdit;

enum class PosAlign { left, right, top, bottom };

// A dialog that lives as long as its owner and is shown on demand. A subclass calls createDialogWidget(),
// sets up its Ui form on the result, and connects the controls with lambdas, using the widget as the
// connection context so nothing fires after destroy().
class StaticDialog : public Window
{
public :
	~StaticDialog() override;

	virtual bool isCreated() const {
		return (_hSelf != nullptr);
	}

	void goToCenter();

	void display(bool toShow = true) const override;
	void displayEnhanced(bool toShow) const { display(toShow); }

	// A rectangle moved onto the working area of its nearest screen
	RECT getViewablePositionRect(RECT testPositionRc) const;

	void destroy() override;

	// Access to the controls by their numeric ID, for dialogs whose logic is written around those IDs
	QWidget* dlgItem(int id) const;
	QAbstractButton* button(int id) const;
	QComboBox* combo(int id) const;
	std::wstring getItemText(int id) const;
	void setItemText(int id, const std::wstring& text) const;
	void enableItem(int id, bool enable = true) const;
	void showItem(int id, bool show = true) const;
	bool isCheckedOrNot(int checkControlID) const;
	void setChecked(int checkControlID, bool checkOrNot = true) const;

	DPIManagerV2& dpiManager() { return _dpiManager; }

protected:
	DPIManagerV2 _dpiManager;

	QDialog* createDialogWidget(bool isRTL = false);
	// Runs a dialog from createDialogWidget() modally, destroys it, and returns the endDialog() result.
	int runModal();
	void endDialog(int result) const;

	// The Escape key or closing the window. By default the dialog is hidden.
	virtual void onCancel();
	virtual void onActivationChanged(bool /*isActive*/) {}
	// Shown, or hidden however it is, also with its owner: where Windows sends WM_SHOWWINDOW
	virtual void onVisibilityChanged(bool /*isShown*/) {}

public:
	// The font of the preferences for every dialog made from now on. An empty family or a size of 0 is the system's.
	static void setDialogFont(const QString& family, int pointSize);
	static void applyDialogFont(QWidget* dialog);

protected:

	friend class NppDialogWidget;

private:
	// The widget is also destroyed with its parent, and then _hSelf is no longer valid
	QPointer<QWidget> _dialogWidget;
};
