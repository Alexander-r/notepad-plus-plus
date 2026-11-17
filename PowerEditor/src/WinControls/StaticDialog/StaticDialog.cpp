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


#include "StaticDialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>

#include "NppControls.h"

// The Qt side of a dialog: tells the owner about the window events it may want to act on.
class NppDialogWidget : public QDialog
{
public:
	NppDialogWidget(StaticDialog* owner, QWidget* parent) : QDialog(parent), _owner(owner) {}

	void detach() { _owner = nullptr; }

protected:
	void closeEvent(QCloseEvent* event) override
	{
		event->ignore();
		if (_owner)
			_owner->onCancel();
	}

	void reject() override
	{
		if (_owner)
			_owner->onCancel();
	}

	void changeEvent(QEvent* event) override
	{
		QDialog::changeEvent(event);
		if (_owner && event->type() == QEvent::ActivationChange)
			_owner->onActivationChanged(isActiveWindow());
	}

	void showEvent(QShowEvent* event) override
	{
		QDialog::showEvent(event);
		if (_owner)
			_owner->onVisibilityChanged(true);
	}

	void hideEvent(QHideEvent* event) override
	{
		QDialog::hideEvent(event);
		if (_owner)
			_owner->onVisibilityChanged(false);
	}

private:
	StaticDialog* _owner;
};

StaticDialog::~StaticDialog()
{
	if (StaticDialog::isCreated())
		StaticDialog::destroy();
}

void StaticDialog::destroy()
{
	if (!_hSelf)
		return;

	if (_dialogWidget)
	{
		// onCancel and onActivationChanged are virtual, and this may be running from a destructor
		static_cast<NppDialogWidget*>(_hSelf)->detach();
		_hSelf->deleteLater();
	}
	_dialogWidget = nullptr;
	_hSelf = nullptr;
}

namespace
{
	QString dialogFontFamily;
	int dialogFontSize = 0;
}

void StaticDialog::setDialogFont(const QString& family, int pointSize)
{
	dialogFontFamily = family;
	dialogFontSize = pointSize;
}

void StaticDialog::applyDialogFont(QWidget* dialog)
{
	if (dialogFontFamily.isEmpty() && dialogFontSize <= 0)
		return;

	QFont font = QApplication::font();
	if (!dialogFontFamily.isEmpty())
		font.setFamily(dialogFontFamily);
	if (dialogFontSize > 0)
		font.setPointSize(dialogFontSize);
	dialog->setFont(font);
}

QDialog* StaticDialog::createDialogWidget(bool isRTL)
{
	auto* dialog = new NppDialogWidget(this, _hParent);
	applyDialogFont(dialog);
	if (isRTL)
		dialog->setLayoutDirection(Qt::RightToLeft);
	_hSelf = dialog;
	_dialogWidget = dialog;
	return dialog;
}

int StaticDialog::runModal()
{
	auto* dialog = static_cast<NppDialogWidget*>(_hSelf);
	dialog->hide(); // a dialog that is already showing would not become modal
	const int result = dialog->exec();
	destroy();
	return result;
}

void StaticDialog::endDialog(int result) const
{
	if (auto* dialog = static_cast<NppDialogWidget*>(_hSelf))
		dialog->done(result);
}

void StaticDialog::onCancel()
{
	display(false);
}

void StaticDialog::goToCenter()
{
	if (!_hSelf)
		return;

	QRect area;
	if (_hParent)
		area = QRect(_hParent->mapToGlobal(QPoint(0, 0)), _hParent->size());
	else if (const QScreen* screen = QGuiApplication::primaryScreen())
		area = screen->availableGeometry();

	QRect frame = _hSelf->frameGeometry();
	frame.moveCenter(area.center());
	_hSelf->move(frame.topLeft());
	_hSelf->show();
}

void StaticDialog::display(bool toShow) const
{
	if (!_hSelf)
		return;

	_hSelf->setVisible(toShow);
	if (toShow)
	{
		_hSelf->raise();
		_hSelf->activateWindow();
	}
}

RECT StaticDialog::getViewablePositionRect(RECT testPositionRc) const
{
	const QRect wanted(QPoint(testPositionRc.left, testPositionRc.top), QPoint(testPositionRc.right - 1, testPositionRc.bottom - 1));
	const QScreen* screen = QGuiApplication::screenAt(wanted.center());
	if (!screen)
		screen = QGuiApplication::primaryScreen();
	if (!screen)
		return testPositionRc;

	const QRect available = screen->availableGeometry();
	QRect moved = wanted;
	if (moved.right() > available.right())
		moved.moveRight(available.right());
	if (moved.bottom() > available.bottom())
		moved.moveBottom(available.bottom());
	if (moved.left() < available.left())
		moved.moveLeft(available.left());
	if (moved.top() < available.top())
		moved.moveTop(available.top());

	RECT rc;
	rc.left = moved.left();
	rc.top = moved.top();
	rc.right = moved.right() + 1;
	rc.bottom = moved.bottom() + 1;
	return rc;
}

QWidget* StaticDialog::dlgItem(int id) const
{
	return nppFindControl(_hSelf, id);
}

QAbstractButton* StaticDialog::button(int id) const
{
	return qobject_cast<QAbstractButton*>(dlgItem(id));
}

QComboBox* StaticDialog::combo(int id) const
{
	return qobject_cast<QComboBox*>(dlgItem(id));
}

std::wstring StaticDialog::getItemText(int id) const
{
	return nppControlText(dlgItem(id)).toStdWString();
}

void StaticDialog::setItemText(int id, const std::wstring& text) const
{
	nppSetControlText(dlgItem(id), QString::fromStdWString(text));
}

void StaticDialog::enableItem(int id, bool enable) const
{
	if (QWidget* item = dlgItem(id))
		item->setEnabled(enable);
}

void StaticDialog::showItem(int id, bool show) const
{
	if (QWidget* item = dlgItem(id))
		item->setVisible(show);
}

bool StaticDialog::isCheckedOrNot(int checkControlID) const
{
	const QAbstractButton* b = button(checkControlID);
	return b && b->isChecked();
}

void StaticDialog::setChecked(int checkControlID, bool checkOrNot) const
{
	if (QAbstractButton* b = button(checkControlID))
		b->setChecked(checkOrNot);
}
