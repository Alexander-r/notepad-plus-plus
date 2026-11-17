// This file is part of Notepad++ project

#include "NppMenu.h"

#include <QSignalBlocker>
#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>

#include "NppMessages.h"
#include "NppResources.h"

namespace
{
	void appendEntries(QMenu* menu, const NppMenuEntry* entries, int count, HWND target)
	{
		for (int i = 0; i < count; ++i)
		{
			const NppMenuEntry& e = entries[i];
			switch (e.kind)
			{
				case NppMenuEntry::Separator:
					menu->addSeparator();
					break;

				case NppMenuEntry::Popup:
				{
					QMenu* child = menu->addMenu(QString::fromWCharArray(e.text));
					appendEntries(child, e.children, e.childCount, target);
					break;
				}

				case NppMenuEntry::Item:
				{
					QAction* action = nppAddMenuItem(menu, e.id, e.text, target);
					action->setCheckable((e.flags & NppMenuEntry::Checked) != 0);
					action->setChecked((e.flags & NppMenuEntry::Checked) != 0);
					action->setEnabled(!(e.flags & NppMenuEntry::Disabled));
					break;
				}
			}
		}
	}
}

HMENU nppCreateMenuFromTemplate(int menuId, HWND commandTarget, QWidget* parent)
{
	int count = 0;
	const NppMenuEntry* entries = nppMenuTemplate(menuId, count);
	auto* menu = new QMenu(parent);
	if (entries)
		appendEntries(menu, entries, count, commandTarget);
	return menu;
}

namespace
{
	void connectMenuCommand(QAction* action, HWND commandTarget, int cmdId)
	{
		QObject::connect(action, &QAction::triggered, action, [action, commandTarget, cmdId]()
		{
			// Qt flips a checkable action before this, while the commands read the mark to decide what to do and set it
			// themselves, as with Win32 menus where only the program changes a mark
			if (action->isCheckable())
			{
				const QSignalBlocker silent(action);
				action->setChecked(!action->isChecked());
			}
			::SendMessage(commandTarget, WM_COMMAND, MAKEWPARAM(static_cast<WORD>(cmdId), 0), 0);
		});
	}
}

QAction* nppAddMenuItem(HMENU menu, int cmdId, const std::wstring& text, HWND commandTarget, QAction* before)
{
	auto* action = new QAction(QString::fromStdWString(text), menu);
	action->setData(cmdId);
	menu->insertAction(before, action);
	connectMenuCommand(action, commandTarget, cmdId);
	return action;
}

QAction* nppAddMenuSeparator(HMENU menu, QAction* before)
{
	auto* action = new QAction(menu);
	action->setSeparator(true);
	menu->insertAction(before, action);
	return action;
}

HMENU nppAddSubMenu(HMENU menu, const std::wstring& text, QAction* before)
{
	auto* child = new QMenu(QString::fromStdWString(text), menu);
	menu->insertMenu(before, child);
	return child;
}

int nppMenuItemCount(HMENU menu)
{
	return menu ? static_cast<int>(menu->actions().size()) : 0;
}

QAction* nppMenuActionAt(HMENU menu, int position)
{
	if (!menu)
		return nullptr;
	const auto actions = menu->actions();
	return (position >= 0 && position < actions.size()) ? actions[position] : nullptr;
}

HMENU nppSubMenuAt(HMENU menu, int position)
{
	const QAction* action = nppMenuActionAt(menu, position);
	return action ? action->menu() : nullptr;
}

QAction* nppInsertMenuItemAt(HMENU menu, int position, int cmdId, const std::wstring& text, HWND commandTarget)
{
	return nppAddMenuItem(menu, cmdId, text, commandTarget, nppMenuActionAt(menu, position));
}

QAction* nppInsertSeparatorAt(HMENU menu, int position)
{
	return nppAddMenuSeparator(menu, nppMenuActionAt(menu, position));
}

void nppInsertSubMenuAt(HMENU menu, int position, HMENU subMenu, const std::wstring& text)
{
	subMenu->setTitle(QString::fromStdWString(text));
	menu->insertMenu(nppMenuActionAt(menu, position), subMenu);
}

void nppRemoveMenuItemAt(HMENU menu, int position)
{
	if (QAction* action = nppMenuActionAt(menu, position))
		menu->removeAction(action);
}

QAction* nppFindMenuAction(HMENU menu, int cmdId)
{
	if (!menu)
		return nullptr;

	const auto actions = menu->actions();
	for (QAction* action : actions)
	{
		if (action->menu())
		{
			if (QAction* found = nppFindMenuAction(action->menu(), cmdId))
				return found;
		}
		else if (!action->isSeparator() && action->data().toInt() == cmdId)
		{
			return action;
		}
	}
	return nullptr;
}

void nppEnableMenuCommand(HMENU menu, int cmdId, bool enable)
{
	if (QAction* action = nppFindMenuAction(menu, cmdId))
		action->setEnabled(enable);
}

void nppCheckMenuCommand(HMENU menu, int cmdId, bool check)
{
	if (QAction* action = nppFindMenuAction(menu, cmdId))
	{
		// Any item can get a check mark in a Win32 menu. Keep the action user-triggered, not self-toggling.
		action->setCheckable(check || action->isCheckable());
		action->setChecked(check);
	}
}

bool nppIsMenuCommandChecked(HMENU menu, int cmdId)
{
	const QAction* action = nppFindMenuAction(menu, cmdId);
	return action && action->isChecked();
}

bool nppIsMenuCommandEnabled(HMENU menu, int cmdId)
{
	const QAction* action = nppFindMenuAction(menu, cmdId);
	return action && action->isEnabled();
}

std::wstring nppMenuCommandText(HMENU menu, int cmdId)
{
	const QAction* action = nppFindMenuAction(menu, cmdId);
	return action ? action->text().toStdWString() : std::wstring();
}

void nppSetMenuCommandText(HMENU menu, int cmdId, const std::wstring& text)
{
	if (QAction* action = nppFindMenuAction(menu, cmdId))
		action->setText(QString::fromStdWString(text));
}

void nppRemoveMenuCommand(HMENU menu, int cmdId)
{
	if (QAction* action = nppFindMenuAction(menu, cmdId))
	{
		action->setVisible(false);
		const auto widgets = action->associatedObjects();
		for (QObject* object : widgets)
		{
			if (auto* owner = qobject_cast<QMenu*>(object))
				owner->removeAction(action);
		}
		action->deleteLater();
	}
}

void nppSyncMenuBar(QMenuBar* bar, HMENU menu)
{
	bar->clear();
	if (!menu)
		return;
	const auto actions = menu->actions();
	for (QAction* action : actions)
		bar->addAction(action);
}

void nppSetMenuCommandIcon(HMENU menu, int cmdId, const QPixmap& icon)
{
	if (QAction* action = nppFindMenuAction(menu, cmdId))
		action->setIcon(QIcon(icon));
}

void nppCheckMenuRadioRange(HMENU menu, int firstCmdId, int lastCmdId, int cmdId)
{
	for (int id = firstCmdId; id <= lastCmdId; ++id)
	{
		if (QAction* action = nppFindMenuAction(menu, id))
		{
			action->setCheckable(true);
			action->setChecked(id == cmdId);
		}
	}
}
