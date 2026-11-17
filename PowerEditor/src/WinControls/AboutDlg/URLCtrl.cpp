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


#include "URLCtrl.h"

#include <QDesktopServices>
#include <QEvent>
#include <QLabel>
#include <QApplication>
#include <QPalette>
#include <QUrl>

namespace
{
	class LinkClicks : public QObject
	{
	public:
		LinkClicks(QObject* parent, std::function<void()> onClick) : QObject(parent), _onClick(std::move(onClick)) {}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::MouseButtonRelease && _onClick)
				_onClick();
			return QObject::eventFilter(watched, event);
		}

	private:
		std::function<void()> _onClick;
	};
}

namespace
{
	// A link takes its colour from the application's palette only when its text is set, or the label when its own
	// palette is, so that has to happen again when dark mode changes
	class FollowPalette : public QObject
	{
	public:
		FollowPalette(QLabel* label, std::function<void()> apply) : QObject(label), _apply(std::move(apply))
		{
			label->installEventFilter(this);
			_apply();
		}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::ApplicationPaletteChange)
				_apply();
			return QObject::eventFilter(watched, event);
		}

	private:
		std::function<void()> _apply;
	};
}

void URLCtrl::applyStyle(QLabel* label) const
{
	QFont font = label->font();
	font.setUnderline(true);
	label->setFont(font);
	label->setCursor(Qt::PointingHandCursor);
}

void URLCtrl::create(HWND itemHandle, const wchar_t* link)
{
	auto* label = qobject_cast<QLabel*>(itemHandle);
	if (!label)
		return;

	_URL = link ? link : L"";
	const QString url = QString::fromStdWString(_URL);
	const QString text = label->text().isEmpty() ? url : label->text().toHtmlEscaped();
	label->setTextFormat(Qt::RichText);
	label->setOpenExternalLinks(true);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	const QString html = QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), text);
	new FollowPalette(label, [label, html]()
	{
		label->clear();
		label->setText(html);
	});
	label->setToolTip(url);
	label->setCursor(Qt::PointingHandCursor);
}

void URLCtrl::create(HWND itemHandle, std::function<void()> onClick)
{
	auto* label = qobject_cast<QLabel*>(itemHandle);
	if (!label)
		return;

	_hSelf = label;
	applyStyle(label);
	new FollowPalette(label, [label]()
	{
		QPalette colours = QApplication::palette();
		colours.setColor(QPalette::WindowText, colours.color(QPalette::Link));
		label->setPalette(colours);
	});
	label->installEventFilter(new LinkClicks(label, std::move(onClick)));
}

void URLCtrl::create(HWND itemHandle, int cmd, HWND msgDest)
{
	auto* label = qobject_cast<QLabel*>(itemHandle);
	if (!label)
		return;

	_cmdID = cmd;
	_msgDest = msgDest ? msgDest : label->parentWidget();
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	const QString html = QStringLiteral("<a href=\"#\">%1</a>").arg(label->text().toHtmlEscaped());
	new FollowPalette(label, [label, html]()
	{
		label->clear();
		label->setText(html);
	});
	QObject::connect(label, &QLabel::linkActivated, label, [this]()
	{
		::SendMessage(_msgDest, WM_COMMAND, _cmdID, 0);
	});
	label->setCursor(Qt::PointingHandCursor);
}
