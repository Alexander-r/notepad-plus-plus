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


#include "DocTabView.h"

#include <QSignalBlocker>
#include <QTabBar>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <cwchar>

#include "Buffer.h"
#include "NppConstants.h"
#include "Parameters.h"
#include "ScintillaEditView.h"
#include "TabBar.h"
#include "resource.h"

enum ImgIdx
{
	SAVED_IMG_INDEX,
	UNSAVED_IMG_INDEX,
	REDONLY_IMG_INDEX,
	REDONLYSYS_IMG_INDEX,
	MONITORING_IMG_INDEX,
};

static constexpr int docTabIconIDs[] = { IDI_SAVED_ICON, IDI_UNSAVED_ICON, IDI_READONLY_ICON, IDI_READONLYSYS_ICON, IDI_MONITORING_ICON };
static constexpr int docTabIconIDs_darkMode[] = { IDI_SAVED_DM_ICON, IDI_UNSAVED_DM_ICON, IDI_READONLY_DM_ICON, IDI_READONLYSYS_DM_ICON, IDI_MONITORING_DM_ICON };
static constexpr int docTabIconIDs_alt[] = { IDI_SAVED_ALT_ICON, IDI_UNSAVED_ALT_ICON, IDI_READONLY_ALT_ICON, IDI_READONLYSYS_ALT_ICON, IDI_MONITORING_ICON };

void DocTabView::init(HINSTANCE hInst, HWND parent, ScintillaEditView* pView, unsigned char indexChoice, unsigned char buttonsStatus)
{
	TabBarPlus::init(hInst, parent, false, false, buttonsStatus);
	_pView = pView;

	_pane = new QWidget(parent);
	auto* layout = new QVBoxLayout(_pane);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(_hSelf);
	layout->addWidget(pView->getHSelf(), 1);
	_pane->hide(); // until display()

	_iconSets.resize(3);
	_iconSets[0].create(docTabIconIDs, static_cast<int>(std::size(docTabIconIDs)));
	_iconSets[1].create(docTabIconIDs_alt, static_cast<int>(std::size(docTabIconIDs_alt)));
	_iconSets[2].create(docTabIconIDs_darkMode, static_cast<int>(std::size(docTabIconIDs_darkMode)));

	_iconSetChoice = (indexChoice < _iconSets.size()) ? indexChoice : 0;
}

void DocTabView::display(bool toShow) const
{
	if (_pane)
		_pane->setVisible(toShow);
	syncTabBarVisibility();
}

void DocTabView::syncTabBarVisibility() const
{
	if (_hSelf)
		_hSelf->setVisible((NppParameters::getInstance().getNppGUI()._tabStatus & TAB_HIDE) == 0);
}

void DocTabView::changeIconSet(unsigned char choice)
{
	if (choice >= _iconSets.size())
		return;
	_iconSetChoice = choice;
	for (size_t i = 0; i < nbItem(); ++i)
		bufferUpdated(getBufferByIndex(i), BufferChangeReadonly);
}

void DocTabView::addBuffer(BufferID buffer)
{
	if (buffer == BUFFER_INVALID)	//valid only
		return;
	if (getIndexByBuffer(buffer) != -1)	//no duplicates
		return;
	const Buffer* buf = MainFileManager.getBufferByID(buffer);
	const QSignalBlocker silent(tabs());
	const int index = tabs()->addTab(QString::fromWCharArray(buf->getCompactFileName()));
	tabs()->setTabData(index, QVariant::fromValue(static_cast<void*>(buffer)));
	bufferUpdated(buf, BufferChangeMask);
}

void DocTabView::closeBuffer(BufferID buffer)
{
	const int indexToClose = getIndexByBuffer(buffer);
	if (indexToClose != -1)
		deletItemAt(static_cast<size_t>(indexToClose));
}

void DocTabView::setIndividualTabColour(BufferID bufferId, int colorId)
{
	bufferId->setDocColorId(colorId);
}

bool DocTabView::isTabPinned(int tabIndex)
{
	BufferID bufferId = getBufferByIndex(tabIndex);
	return bufferId && bufferId->isPinned();
}

int DocTabView::getIndividualTabColourId(int tabIndex)
{
	BufferID bufferId = getBufferByIndex(tabIndex);
	if (!bufferId)
		return -1;

	return bufferId->getDocColorId();
}

bool DocTabView::activateBuffer(BufferID buffer)
{
	int indexToActivate = getIndexByBuffer(buffer);
	if (indexToActivate == -1)
		return false;	//cannot activate

	activateAt(indexToActivate);
	return true;
}


BufferID DocTabView::activeBuffer()
{
	int index = getCurrentTabIndex();
	return getBufferByIndex(index);
}


BufferID DocTabView::findBufferByName(const wchar_t * fullfilename) //-1 if not found, something else otherwise
{
	const size_t count = nbItem();
	for (size_t i = 0; i < count; ++i)
	{
		BufferID id = getBufferByIndex(i);
		const Buffer* buf = MainFileManager.getBufferByID(id);
		if (std::wcscmp(fullfilename, buf->getFullPathName()) == 0) // file names are case-sensitive here
			return id;
	}
	return BUFFER_INVALID;
}


int DocTabView::getIndexByBuffer(BufferID id)
{
	const int count = tabs()->count();
	for (int i = 0; i < count; ++i)
	{
		if (getBufferByIndex(i) == id)
			return i;
	}
	return -1;
}


BufferID DocTabView::getBufferByIndex(size_t index)
{
	return static_cast<BufferID>(tabs()->tabData(static_cast<int>(index)).value<void*>());
}


void DocTabView::bufferUpdated(const Buffer* buffer, int mask)
{
	int index = getIndexByBuffer(buffer->getID());
	if (index == -1)
		return;

	if (mask & BufferChangeReadonly || mask & BufferChangeDirty)
	{
		int image = buffer->isDirty() ? UNSAVED_IMG_INDEX : SAVED_IMG_INDEX;
		if (buffer->isMonitoringOn())
			image = MONITORING_IMG_INDEX;
		else if (buffer->getFileReadOnly())
			image = REDONLYSYS_IMG_INDEX;
		else if (buffer->getUserReadOnly())
			image = REDONLY_IMG_INDEX;

		if (_iconSetChoice < _iconSets.size())
			tabs()->setTabIcon(index, _iconSets[_iconSetChoice].icon(image));
	}

	if (mask & BufferChangeFilename)
	{
		// A literal ampersand must be doubled, or it would become a mnemonic
		QString label = QString::fromWCharArray(buffer->getCompactFileName());
		label.replace(u'&', QStringLiteral("&&"));
		tabs()->setTabText(index, label);
		tabs()->setTabToolTip(index, QString::fromWCharArray(buffer->getFullPathName()));
	}
}


void DocTabView::setBuffer(size_t index, BufferID id)
{
	if (index >= nbItem())
		return;

	tabs()->setTabData(static_cast<int>(index), QVariant::fromValue(static_cast<void*>(id)));

	bufferUpdated(MainFileManager.getBufferByID(id), BufferChangeMask);	//update tab, everything has changed
}
