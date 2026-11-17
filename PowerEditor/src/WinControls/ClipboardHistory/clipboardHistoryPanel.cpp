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


#include "clipboardHistoryPanel.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QHeaderView>
#include <QMimeData>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <Scintilla.h>

#include "Common.h"
#include "DockingDlgInterface.h"
#include "NppDarkMode.h"
#include "ScintillaEditView.h"
#include "clipboardFormats.h"
#include "clipboardHistoryPanel_rc.h"
#include "dockingResource.h"
#include "menuCmdID.h"
#include "resource.h"

ClipboardDataInfo ClipboardHistoryPanel::getClipboadData()
{
	ClipboardDataInfo clipboardData;
	const QMimeData* content = QApplication::clipboard()->mimeData();
	if (!content)
		return clipboardData;

	const QString binaryFormat = QString::fromWCharArray(CF_NPPBINARY);
	if (content->hasFormat(binaryFormat)) // Special copy-paste: Binary data
	{
		const QByteArray bytes = content->data(binaryFormat);
		clipboardData._data.assign(bytes.cbegin(), bytes.cend());
		clipboardData._isBinaryContained = true;
	}
	else if (content->hasText())
	{
		clipboardData._text = content->text().toStdWString();
	}
	return clipboardData;
}

// Search clipboard data in internal storage
// return -1 if not found, else return the index of internal array
int ClipboardHistoryPanel::getClipboardDataIndex(const ClipboardDataInfo& cbd) const
{
	const auto it = std::find(_clipboardDataInfos.begin(), _clipboardDataInfos.end(), cbd);
	return (it == _clipboardDataInfos.end()) ? -1 : static_cast<int>(it - _clipboardDataInfos.begin());
}

std::wstring ClipboardHistoryPanel::getDisplayStr(const ClipboardDataInfo& cbd)
{
	static constexpr size_t maxLen = 256 - 3; // as long as the label of the Windows list
	std::wstring displayStr;
	bool truncated = false;

	if (cbd._isBinaryContained)
	{
		const auto len = std::min<size_t>(cbd._data.size(), maxLen);
		truncated = cbd._data.size() > len;

		const auto raw = std::string(cbd._data.begin(), cbd._data.begin() + len);
		displayStr = string2wstring(raw);
	}
	else
	{
		const auto len = std::min<size_t>(cbd._text.size(), maxLen - 1);
		truncated = cbd._text.size() > len;
		displayStr.assign(cbd._text, 0, len);
	}

	if (truncated)
	{
		// this will be over maxLen, but it is already accounted
		displayStr += L"...";
	}

	return displayStr;
}

void ClipboardHistoryPanel::addToClipboadHistory(const ClipboardDataInfo& cbd)
{
	const int idx = getClipboardDataIndex(cbd);
	if (idx == 0)
		return;

	if (auto it = _clipboardDataInfos.begin();
		idx != -1)
	{
		std::rotate(it, it + idx, it + idx + 1);
		_listView.removeFromIndex(idx);
	}
	else
	{
		_clipboardDataInfos.insert(it, cbd);
	}

	_listView.addLine({ getDisplayStr(cbd) }, 0, 0);
}

void ClipboardHistoryPanel::onDoubleClick(int row)
{
	if (row < 0 || static_cast<size_t>(row) >= _clipboardDataInfos.size())
	{
		return;
	}

	int codepage = (*_ppEditView)->getCurrentBuffer()->getEncoding();
	if (codepage == -1)
	{
		const auto cp = (*_ppEditView)->execute(SCI_GETCODEPAGE);
		codepage = (cp == SC_CP_UTF8) ? SC_CP_UTF8 : 0;
	}
	else
	{
		codepage = SC_CP_UTF8;
	}

	try
	{
		const ClipboardDataInfo& info = _clipboardDataInfos[row];
		if (info._isBinaryContained)
		{
			(*_ppEditView)->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(""));
			(*_ppEditView)->execute(SCI_ADDTEXT, static_cast<WPARAM>(info._data.size()), reinterpret_cast<LPARAM>(info._data.data()));
			(*_ppEditView)->grabFocus();
		}
		else if (!info._text.empty())
		{
			const std::string strA = wstring2string(info._text, codepage);

			(*_ppEditView)->execute(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(""));
			(*_ppEditView)->execute(SCI_ADDTEXT, static_cast<WPARAM>(strA.length()), reinterpret_cast<LPARAM>(strA.c_str()));
			(*_ppEditView)->grabFocus();
		}
	}
	catch (...)
	{
		NppDarkMode::darkMessageBoxW(_hSelf, L"Cannot process this clipboard data in the history:\nThe data is too large to be treated.", L"Clipboard History panel", MB_OK | MB_APPLMODAL);
	}
}

void ClipboardHistoryPanel::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);

	_listView.addColumn(columnInfo(L"", 200));
	_listView.init(_hInst, _hSelf, IDC_LIST_CLIPBOARD);
	_listView.table()->horizontalHeader()->hide();

	auto* layout = new QVBoxLayout(_hSelf);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_listView.getHSelf());

	QObject::connect(_listView.table(), &QTableWidget::cellDoubleClicked, _hSelf, [this](int row) { onDoubleClick(row); });

	QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, _hSelf, [this]()
	{
		if (_isTrackingClipboardOps)
		{
			ClipboardDataInfo clipboardData = getClipboadData();
			if (!clipboardData.isEmpty())
				addToClipboadHistory(clipboardData);
		}
	});

	// What is on the clipboard already shows from the start
	const ClipboardDataInfo current = getClipboadData();
	if (!current.isEmpty())
		addToClipboadHistory(current);
}

void ClipboardHistoryPanel::onDockingNotify(const NMHDR& notification)
{
	if (LOWORD(notification.code) == DMN_CLOSE)
		::SendMessage(_hParent, WM_COMMAND, IDM_EDIT_CLIPBOARDHISTORY_PANEL, 0);
	else
		DockingDlgInterface::onDockingNotify(notification);
}
