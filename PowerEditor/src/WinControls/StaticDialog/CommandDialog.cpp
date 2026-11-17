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

#include "CommandDialog.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSlider>

#include <algorithm>
#include <functional>
#include <utility>

#include "ColourPicker.h"

namespace
{
	// Calls back when one of the page's edit fields gets the focus
	class EditFocus : public QObject
	{
	public:
		EditFocus(QObject* parent, std::function<void(QObject*)> onFocus) : QObject(parent), _onFocus(std::move(onFocus)) {}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::FocusIn)
				_onFocus(watched);
			return QObject::eventFilter(watched, event);
		}

	private:
		std::function<void(QObject*)> _onFocus;
	};
}

void CommandDialog::connectControls()
{
	const auto onChange = [this](QWidget* control, UINT code)
	{
		const int id = control->property("nppId").toInt();
		onCommand(MAKEWPARAM(id, code), reinterpret_cast<LPARAM>(control));
	};

	const QList<QWidget*> controls = _hSelf->findChildren<QWidget*>();
	for (QWidget* control : controls)
	{
		if (control->property("nppId").toInt() <= 0)
			continue;

		if (auto* button = qobject_cast<QAbstractButton*>(control))
		{
			QObject::connect(button, &QAbstractButton::clicked, _hSelf, [onChange, button]() { onChange(button, BN_CLICKED); });
		}
		else if (auto* combo = qobject_cast<QComboBox*>(control))
		{
			QObject::connect(combo, &QComboBox::activated, _hSelf, [onChange, combo]() { onChange(combo, CBN_SELCHANGE); });
		}
		else if (auto* edit = qobject_cast<QLineEdit*>(control))
		{
			QObject::connect(edit, &QLineEdit::textChanged, _hSelf, [onChange, edit]() { onChange(edit, EN_CHANGE); });
			QObject::connect(edit, &QLineEdit::editingFinished, _hSelf, [onChange, edit]() { onChange(edit, EN_KILLFOCUS); });
			edit->installEventFilter(new EditFocus(edit, [onChange, edit](QObject*) { onChange(edit, EN_SETFOCUS); }));
		}
		else if (auto* text = qobject_cast<QPlainTextEdit*>(control))
		{
			QObject::connect(text, &QPlainTextEdit::textChanged, _hSelf, [onChange, text]() { onChange(text, EN_CHANGE); });
		}
		else if (auto* listBox = qobject_cast<QListWidget*>(control))
		{
			QObject::connect(listBox, &QListWidget::currentRowChanged, _hSelf, [onChange, listBox]() { onChange(listBox, LBN_SELCHANGE); });
			QObject::connect(listBox, &QListWidget::itemDoubleClicked, _hSelf, [onChange, listBox]() { onChange(listBox, LBN_DBLCLK); });
		}
		else if (auto* scale = qobject_cast<QSlider*>(control))
		{
			QObject::connect(scale, &QSlider::valueChanged, _hSelf, [this, scale]() { onScroll(scale); });
		}
	}
}

QListWidget* CommandDialog::list(int id) const
{
	return qobject_cast<QListWidget*>(dlgItem(id));
}

QSlider* CommandDialog::slider(int id) const
{
	return qobject_cast<QSlider*>(dlgItem(id));
}

int CommandDialog::getItemInt(int id) const
{
	return QString::fromStdWString(getItemText(id)).trimmed().toInt();
}

void CommandDialog::setItemInt(int id, long long value) const
{
	setItemText(id, std::to_wstring(value));
}

void CommandDialog::copyItemText(int id, wchar_t* buffer, size_t bufferSize) const
{
	const std::wstring text = getItemText(id);
	const size_t length = std::min(text.length(), bufferSize - 1);
	text.copy(buffer, length);
	buffer[length] = L'\0';
}

std::wstring CommandDialog::comboText(int id, int index) const
{
	return combo(id)->itemText(index).toStdWString();
}

int CommandDialog::addComboItem(int id, const std::wstring& text) const
{
	combo(id)->addItem(QString::fromStdWString(text));
	return combo(id)->count() - 1;
}

int CommandDialog::findComboItem(int id, const std::wstring& text) const
{
	return combo(id)->findText(QString::fromStdWString(text), Qt::MatchFixedString);
}

int CommandDialog::addListItem(int id, const std::wstring& text) const
{
	list(id)->addItem(QString::fromStdWString(text));
	return list(id)->count() - 1;
}

void CommandDialog::setListRow(int id, int row) const
{
	const QSignalBlocker blocker(list(id));
	list(id)->setCurrentRow(row);
}

void CommandDialog::setSliderPos(int id, int position) const
{
	const QSignalBlocker blocker(slider(id));
	slider(id)->setValue(position);
}

void CommandDialog::limitText(int id, int maxLength) const
{
	if (auto* edit = qobject_cast<QLineEdit*>(dlgItem(id)))
	{
		edit->setMaxLength(maxLength);
		if (maxLength <= 2)
			edit->setMaximumWidth(40);
	}
}

void CommandDialog::setTip(int id, const std::wstring& tip) const
{
	// As rich text, for a long tip to wrap
	if (QWidget* control = dlgItem(id))
		control->setToolTip(QStringLiteral("<p>%1</p>").arg(QString::fromStdWString(tip).toHtmlEscaped()));
}

void CommandDialog::redrawDlgItem(int id) const
{
	if (QWidget* control = dlgItem(id))
		control->update();
}

void CommandDialog::setItemTextQuietly(int id, const std::wstring& text) const
{
	QWidget* control = dlgItem(id);
	const QSignalBlocker blocker(control);
	setItemText(id, text);
}

void CommandDialog::placePicker(int id, ColourPicker& picker, bool isOnLeft)
{
	QWidget* control = dlgItem(id);
	QWidget* parent = control->parentWidget();

	auto* holder = new QWidget(parent);
	auto* row = new QHBoxLayout(holder);
	row->setContentsMargins(0, 0, 0, 0);

	parent->layout()->replaceWidget(control, holder, Qt::FindChildrenRecursively);
	picker.getHSelf()->setParent(holder);
	row->addWidget(isOnLeft ? picker.getHSelf() : control);
	row->addWidget(isOnLeft ? control : picker.getHSelf());
	row->addStretch(1);

	picker.setOnPicked([this, &picker]() { onCommand(MAKEWPARAM(0, CPN_COLOURPICKED), reinterpret_cast<LPARAM>(picker.getHSelf())); });
}

