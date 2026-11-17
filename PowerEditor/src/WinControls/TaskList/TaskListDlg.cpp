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


#include "TaskListDlg.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>

#include "IconList.h"
#include "Notepad_plus_msgs.h"
#include "NppConstants.h"
#include "NppDarkMode.h"
#include "Parameters.h"

int TaskListDlg::_instanceCount = 0;

namespace
{
	class SwitcherEvents : public QObject
	{
	public:
		SwitcherEvents(TaskListDlg* owner, QObject* parent) : QObject(parent), _owner(owner) {}

		bool eventFilter(QObject* watched, QEvent* event) override
		{
			switch (event->type())
			{
				case QEvent::ShortcutOverride:
					event->accept(); // the keys belong to the switcher, not to the menu commands
					return true;

				case QEvent::KeyPress:
				{
					const auto* key = static_cast<QKeyEvent*>(event);
					if (key->modifiers() & Qt::ControlModifier)
					{
						switch (key->key())
						{
							// Ctrl+F4 will dismiss document switcher
							case Qt::Key_F4:
								_owner->dismiss();
								break;

							// Shift+Tab is cool but I think VK_UP and VK_LEFT are also cool :-)
							case Qt::Key_Backtab:
							case Qt::Key_Up:
							case Qt::Key_Left:
								_owner->moveSelection(-1);
								break;

							// VK_DOWN and VK_RIGHT do the same as VK_TAB does
							case Qt::Key_Tab:
								_owner->moveSelection((key->modifiers() & Qt::ShiftModifier) ? -1 : 1);
								break;

							case Qt::Key_Down:
							case Qt::Key_Right:
								_owner->moveSelection(1);
								break;

							default:
								break;
						}
					}
					else if (key->key() == Qt::Key_Escape)
					{
						_owner->dismiss();
					}
					return true;
				}

				case QEvent::KeyRelease:
				{
					const auto* key = static_cast<QKeyEvent*>(event);
					if (key->key() == Qt::Key_Control)
						_owner->pickCurrent();
					// Ctrl+W will dismiss document switcher on releasing 'W' key to avoid accidently closing newly activated document
					else if (key->key() == Qt::Key_W && (key->modifiers() & Qt::ControlModifier))
						_owner->dismiss();
					return true;
				}

				// Holding the right button is the other way to keep the switcher open
				case QEvent::MouseButtonRelease:
				{
					if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
					{
						_owner->pickCurrent();
						return true;
					}
					break;
				}

				case QEvent::Wheel:
				{
					_owner->moveSelection(static_cast<QWheelEvent*>(event)->angleDelta().y() > 0 ? -1 : 1);
					return true;
				}

				default:
					break;
			}
			return QObject::eventFilter(watched, event);
		}

	private:
		TaskListDlg* _owner;
	};
}

int TaskListDlg::doDialog(bool isRTL)
{
	::SendMessage(_hParent, WM_GETTASKLISTINFO, reinterpret_cast<WPARAM>(&_taskListInfo), 0);
	const int nbTotal = static_cast<int>(_taskListInfo._tlfsLst.size());
	if (nbTotal == 0)
		return -1;

	int i2set = _taskListInfo._currentIndex + (_initDir == dirDown?1:-1);

	if (i2set < 0)
		i2set = nbTotal - 1;

	if (i2set > (nbTotal - 1))
		i2set = 0;

	QDialog* dialog = createDialogWidget(isRTL);
	dialog->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);

	_list = new QListWidget(dialog);
	_list->setSelectionMode(QAbstractItemView::SingleSelection);
	_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_list->setUniformItemSizes(true);
	_list->setTextElideMode(Qt::ElideMiddle);

	QFont listFont = _list->font();
	listFont.setPointSize(listFont.pointSize() + 2);
	_list->setFont(listFont);

	auto* layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_list);

	const QFontMetrics metrics(listFont);
	int widest = 150;
	for (const TaskLstFnStatus& status : _taskListInfo._tlfsLst)
	{
		const QString name = QString::fromStdWString(status._fn);
		auto* item = new QListWidgetItem(name, _list);
		if (_icons && static_cast<size_t>(status._status) < _icons->size())
			item->setIcon(_icons->icon(status._status));
		widest = std::max(widest, metrics.horizontalAdvance(name));
	}

	QObject::connect(_list, &QListWidget::itemClicked, dialog, [this]() { pickCurrent(); });
	_list->installEventFilter(new SwitcherEvents(this, dialog));

	selectRow(i2set);

	const QRect available = _hParent->screen()->availableGeometry();
	const int rowHeight = std::max(_list->sizeHintForRow(0), metrics.height());
	const int width = std::min(widest + 64, available.width() * 4 / 5);
	const int height = std::min(rowHeight * nbTotal + 8, available.height() * 4 / 5);
	dialog->resize(width, height);
	goToCenter();
	_list->setFocus();

	// Ctrl may be up already, when the switcher was slower to come than the user's fingers
	QTimer::singleShot(0, dialog, [this]()
	{
		if (!(QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier) && !(QGuiApplication::mouseButtons() & Qt::RightButton))
			pickCurrent();
	});

	return runModal();
}

void TaskListDlg::selectRow(int row)
{
	const int current = _list->currentRow();
	if (auto* item = _list->item(current))
	{
		QFont font = item->font();
		font.setBold(false);
		item->setFont(font);
	}
	_list->setCurrentRow(row);
	if (auto* item = _list->item(row))
	{
		QFont font = _list->font();
		font.setBold(true);
		item->setFont(font);
		_list->scrollToItem(item);
	}
}

void TaskListDlg::moveSelection(int direction)
{
	const int count = _list->count();
	selectRow((_list->currentRow() + direction + count) % count);
}

void TaskListDlg::pickCurrent()
{
	const int listIndex = _list->currentRow();
	if (listIndex >= 0 && static_cast<size_t>(listIndex) < _taskListInfo._tlfsLst.size())
	{
		const int view2set = _taskListInfo._tlfsLst[listIndex]._iView;
		const int index2Switch = _taskListInfo._tlfsLst[listIndex]._docIndex;
		::SendMessage(_hParent, NPPM_ACTIVATEDOC, view2set, index2Switch);
	}
	dismiss();
}

void TaskListDlg::dismiss()
{
	endDialog(-1);
}
