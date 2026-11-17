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


#include "WindowsDlg.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Buffer.h"
#include "Common.h"
#include "DocTabView.h"
#include "NppMenu.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "WindowsDlgRc.h"
#include "localization.h"
#include "menuCmdID.h"
#include "resource.h"
#include "ui_WindowsDlg.h"

using namespace std;

#define WD_ROOTNODE					"WindowsDlg"
#define WD_CLMNNAME					"ColumnName"
#define WD_CLMNPATH					"ColumnPath"
#define WD_CLMNTYPE					"ColumnType"
#define WD_CLMNSIZE					"ColumnSize"
#define WD_CLMNDT                   "ColumnDateTime"
#define WD_NBDOCSTOTAL				"NbDocsTotal"
#define WD_MENUCOPYNAME				"MenuCopyName"
#define WD_MENUCOPYPATH				"MenuCopyPath"

static const wchar_t *readonlyString = L" [Read Only]";

namespace
{
	constexpr int nbColumns = 5;

	// The keys of the list that are not for the list itself
	class ListKeys : public QObject
	{
	public:
		ListKeys(QObject* parent, std::function<void(wchar_t)> onChar, std::function<void()> onCopy)
			: QObject(parent), _onChar(std::move(onChar)), _onCopy(std::move(onCopy)) {}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::KeyPress)
			{
				const auto* key = static_cast<QKeyEvent*>(event);
				const Qt::KeyboardModifiers modifiers = key->modifiers();
				if (key->key() == Qt::Key_C && (modifiers & Qt::ControlModifier))
				{
					_onCopy();
					return true;
				}

				const QString text = key->text();
				if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier)) && !text.isEmpty() && text.at(0).isPrint())
				{
					_onChar(text.toStdWString().front());
					return true;
				}
			}
			return QObject::eventFilter(watched, event);
		}

	private:
		std::function<void(wchar_t)> _onChar;
		std::function<void()> _onCopy;
	};
}

struct NumericStringEquivalence
{
	int operator()(const wchar_t* s1, const wchar_t* s2) const
	{
		return numstrcmp(s1, s2);
	}

	static inline int numstrcmp_get(const wchar_t **str, int *length)
	{
		const wchar_t *p = *str;
		int value = 0;
		for (*length = 0; std::iswdigit(*p); ++(*length))
			value = value * 10 + *p++ - L'0';
		*str = p;
		return (value);
	}

	static int numstrcmp(const wchar_t *str1, const wchar_t *str2)
	{
		wchar_t *p1 = nullptr, *p2 = nullptr;
		int c1 = 0, c2 = 0, lcmp = 0;
		for (;;)
		{
			if (*str1 == 0 || *str2 == 0)
			{
				if (*str1 != *str2)
					lcmp = *str1 - *str2;
				break;
			}

			if (std::iswdigit(*str1) && std::iswdigit(*str2))
			{
				lcmp = wcstol(str1, &p1, 10) - wcstol(str2, &p2, 10);
				if ( lcmp == 0 )
					lcmp = static_cast<int>((p2 - str2) - (p1 - str1));
				if ( lcmp != 0 )
					break;
				str1 = p1, str2 = p2;
			}
			else
			{
				if (*str1 < 128 && std::iswupper(*str1))
					c1 = towlower(*str1);
				else
					c1 = *str1;
				if (*str2 < 128 && std::iswupper(*str2))
					c2 = towlower(*str2);
				else
					c2 = *str2;
				lcmp = (c1 - c2);
				if (lcmp != 0)
					break;
				++str1, ++str2;
			}
		}
		return ( lcmp < 0 ) ? -1 : (lcmp > 0 ? 1 : 0);
	}
};

struct BufferEquivalent
{
	NumericStringEquivalence _strequiv{};
	DocTabView* _pTab = nullptr;
	int _iColumn = 0;
	bool _reverse = false;
	BufferEquivalent(DocTabView* pTab, int iColumn, bool reverse)
		: _pTab(pTab), _iColumn(iColumn), _reverse(reverse)
	{}

	bool operator()(int i1, int i2) const
	{
		if (i1 == i2) return false; // equivalence test not equality
		if (_reverse) std::swap(i1, i2);
		return compare(i1, i2);
	}

	bool compare(int i1, int i2) const
	{
		if (_iColumn >= 0 && _iColumn <= 4)
		{
			BufferID bid1 = _pTab->getBufferByIndex(i1);
			BufferID bid2 = _pTab->getBufferByIndex(i2);
			Buffer * b1 = MainFileManager.getBufferByID(bid1);
			Buffer * b2 = MainFileManager.getBufferByID(bid2);

			if (_iColumn == 0)
			{
				const wchar_t *s1 = b1->getFileName();
				const wchar_t *s2 = b2->getFileName();
				int result = _strequiv(s1, s2);

				if (result != 0) // default to filepath sorting when equivalent
					return result < 0;
			}
			else if (_iColumn == 2)
			{
				NppParameters & nppParameters = NppParameters::getInstance();
				const wchar_t *s1;
				const wchar_t *s2;
				//const wchar_t empty[] = ;
				Lang *lang1 = nppParameters.getLangFromID(b1->getLangType());

				if (lang1)
				{
					s1 = lang1->getLangName();
				}
				else
					s1 = L"";

				Lang *lang2 = nppParameters.getLangFromID(b2->getLangType());
				if (lang2)
				{
					s2 = lang2->getLangName();
				}
				else
					s2 = L"";

				int result = _strequiv(s1, s2);

				if (result != 0) // default to filepath sorting when equivalent
					return result < 0;

			}
			else if (_iColumn == 3)
			{
				auto t1 = b1->docLength();
				auto t2 = b2->docLength();

				if (t1 != t2) // default to filepath sorting when equivalent
					return (t1 < t2);
			}
			else if (_iColumn == 4)
			{
				const auto v1 = fileTimeTicks(b1->getLastModifiedTimestamp());
				const auto v2 = fileTimeTicks(b2->getLastModifiedTimestamp());

				if (v1 != v2)
					return (v1 < v2); //_isAscending ? (v1 < v2) : (v1 > v2);
			}

			// _iColumn == 1
			const wchar_t *s1 = b1->getFullPathName();
			const wchar_t *s2 = b2->getFullPathName();
			return _strequiv(s1, s2) < 0;	//we can compare the full path to sort on directory, since after sorting directories sorting files is the second thing to do (if directories are the same that is)
		}
		return false;
	}
};

QRect WindowsDlg::_lastKnownLocation;

void WindowsDlg::init(HINSTANCE hInst, HWND parent, DocTabView *pTab)
{
	StaticDialog::init(hInst, parent);
	_pTab = pTab;
}

void WindowsDlg::finish(int result)
{
	_lastKnownLocation = _hSelf->geometry();
	endDialog(result);
}

void WindowsDlg::onCancel()
{
	finish(IDCANCEL);
}

wstring WindowsDlg::cellText(int row, int column) const
{
	Buffer* buf = getBuffer(row);
	if (!buf)
		return {};

	wstring text;
	if (column == 0) // file name
	{
		text = buf->getFileName();
		if (buf->isDirty())
		{
			text += '*';
		}
		else if (buf->isReadOnly())
		{
			text += readonlyString;
		}
	}
	else if (column == 1) // directory
	{
		const wchar_t *fullName = buf->getFullPathName();
		const wchar_t *fileName = buf->getFileName();
		auto len = static_cast<intptr_t>(std::wcslen(fullName) - std::wcslen(fileName));
		if (len > 0)
			text.assign(fullName, static_cast<size_t>(len));
	}
	else if (column == 2) // Type
	{
		NppParameters& nppParameters = NppParameters::getInstance();
		Lang* lang = nppParameters.getLangFromID(buf->getLangType());
		if (lang)
		{
			text = lang->getLangName();
		}
	}
	else if (column == 3) // size
	{
		text = to_wstring(buf->docLength());
	}
	else if (column == 4) // Modified time
	{
		// handle unsaved or unknown timestamp
		static constexpr std::uint64_t unixEpochAsFileTime = 116444736000000000ULL;
		const std::uint64_t ticks = fileTimeTicks(buf->getLastModifiedTimestamp());
		if (ticks > unixEpochAsFileTime)
		{
			const QDateTime modified = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>((ticks - unixEpochAsFileTime) / 10000));
			text = modified.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toStdWString();
		}
	}
	return text;
}

// The list has a row for each entry of _idxMap, showing what the document holds now
void WindowsDlg::refreshList() const
{
	if (!_hList)
		return;

	const int rows = static_cast<int>(_idxMap.size());
	_hList->setRowCount(rows);
	for (int row = 0; row < rows; ++row)
	{
		for (int column = 0; column < nbColumns; ++column)
		{
			QTableWidgetItem* item = _hList->item(row, column);
			if (!item)
			{
				item = new QTableWidgetItem;
				if (column == 2 || column == 3)
					item->setTextAlignment(Qt::AlignCenter);
				_hList->setItem(row, column, item);
			}
			item->setText(QString::fromStdWString(cellText(row, column)));
		}
	}
}

vector<int> WindowsDlg::selectedRows() const
{
	vector<int> rows;
	const QModelIndexList selected = _hList->selectionModel()->selectedRows();
	rows.reserve(selected.size());
	for (const QModelIndex& index : selected)
		rows.push_back(index.row());
	std::sort(rows.begin(), rows.end());
	return rows;
}

void WindowsDlg::setRowSelected(int row, bool isSelected, bool isFocused) const
{
	const QModelIndex index = _hList->model()->index(row, 0);
	QItemSelectionModel* model = _hList->selectionModel();
	model->select(index, (isSelected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows);
	if (isFocused)
		model->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
}

void WindowsDlg::onColumnClicked(int column)
{
	_currentColumn = column;

	if (_lastSort == _currentColumn)
	{
		_reverseSort = true;
		_lastSort = -1;
	}
	else
	{
		_reverseSort = false;
		_lastSort = _currentColumn;
	}

	updateColumnNames();
	doColumnSort();
}

void WindowsDlg::onSortTabs()
{
	// they never set a column to sort by, so assume they wanted filename
	if (_currentColumn == -1)
	{
		_currentColumn = 0;
		_reverseSort = false;
		_lastSort = _currentColumn;

		updateColumnNames();
		doColumnSort();
	}

	doSortToTabs();

	// must re-sort because tab indexes changed
	doColumnSort();
}

// Selecting by the first character of the name, which cycles through the names that match
void WindowsDlg::onCharTyped(wchar_t ch)
{
	const int itemCount = _hList->rowCount();

	// backup current state
	vector<bool> stateMap(itemCount, false);
	for (int row : selectedRows())
		stateMap[row] = true;

	int firstMatchFound = -1;
	int lastMatchSel = -1;
	int targetedIndex = -1;

	// find items whose first character matches the typed character
	for (int index = 0; index < itemCount; ++index)
	{
		const wstring name = cellText(index, 0);
		if (!name.empty() && towlower(name[0]) == towlower(ch))
		{
			// mark the first matching item index found
			if (firstMatchFound == -1)
				firstMatchFound = index;

			// if the item is currenly selected, skip and find the next item
			if (stateMap[index])
			{
				lastMatchSel = index;
				continue;
			}

			// stop at the next matching item
			// after the last selected matching one
			if (lastMatchSel != -1)
			{
				targetedIndex = index;
				break;
			}
		}
	}

	// otherwise, that means either of these cases:
	//  1. no matching item is currently selected
	//	2. the last matching item is currently selected
	//	3. all matching items are currently selected
	if (targetedIndex == -1)
		targetedIndex = firstMatchFound;

	if (targetedIndex != -1)
	{
		_hList->clearSelection();
		setRowSelected(targetedIndex, true, true);
		_hList->scrollToItem(_hList->item(targetedIndex, 0));
	}
	else
	{
		// no matching item is found: the state stays, with a beep
		const NppGUI& nppGUI = NppParameters::getInstance().getNppGUI();
		if (!nppGUI._muteSounds)
			QApplication::beep();
	}
}

void WindowsDlg::showListMenu(const QPoint& position)
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	const bool enableMenu = !selectedRows().empty();

	QMenu menu;
	QAction* copyName = menu.addAction(QString::fromStdWString(pNativeSpeaker->getAttrNameStr(L"Copy Name(s)", WD_ROOTNODE, WD_MENUCOPYNAME)));
	QAction* copyPath = menu.addAction(QString::fromStdWString(pNativeSpeaker->getAttrNameStr(L"Copy Pathname(s)", WD_ROOTNODE, WD_MENUCOPYPATH)));
	copyName->setEnabled(enableMenu);
	copyPath->setEnabled(enableMenu);

	const QAction* chosen = menu.exec(_hList->viewport()->mapToGlobal(position));
	if (chosen == copyName)
		putItemsToClipboard(false);
	else if (chosen == copyPath)
		putItemsToClipboard(true);
}

void WindowsDlg::doColumnSort()
{
	if (_currentColumn == -1)
		return;

	size_t n = _idxMap.size();
	vector<bool> isSelected(n, false);
	for (int row : selectedRows())
		isSelected[_idxMap[row]] = true;

	stable_sort(_idxMap.begin(), _idxMap.end(), BufferEquivalent(_pTab, _currentColumn, _reverseSort));
	for (size_t i = 0; i < n; ++i)
		setRowSelected(static_cast<int>(i), isSelected[_idxMap[i]]);

	refreshList();
	updateButtonState();
}

void WindowsDlg::updateButtonState()
{
	const size_t nSelection = selectedRows().size();
	enableItem(IDOK, nSelection == 1);
	enableItem(IDC_WINDOWS_SAVE, nSelection != 0);
	enableItem(IDC_WINDOWS_CLOSE, nSelection != 0);
	enableItem(IDC_WINDOWS_SORT, true);
}

int WindowsDlg::doDialog()
{
	QDialog* dialog = createDialogWidget(false);
	Ui::WindowsDlg ui;
	ui.setupUi(dialog);

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
	pNativeSpeaker->changeDlgLang(dialog, "Window");

	_hList = ui.windowsList;
	_hList->setColumnCount(nbColumns);
	_hList->setSelectionBehavior(QAbstractItemView::SelectRows);
	_hList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	_hList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_hList->setShowGrid(false);
	_hList->setWordWrap(false);
	_hList->setContextMenuPolicy(Qt::CustomContextMenu);
	_hList->verticalHeader()->hide();
	_hList->verticalHeader()->setDefaultSectionSize(_hList->fontMetrics().height() + 6);

	const auto toColor = [](COLORREF colour) { return QColor(colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF); };
	QPalette colours = _hList->palette();
	colours.setColor(QPalette::Base, toColor(NppParameters::getInstance().getCurrentDefaultBgColor()));
	colours.setColor(QPalette::Text, toColor(NppParameters::getInstance().getCurrentDefaultFgColor()));
	_hList->setPalette(colours);

	updateColumnNames();
	QHeaderView* header = _hList->horizontalHeader();
	header->setSectionsMovable(true);
	header->setSectionsClickable(true);
	header->setSectionResizeMode(QHeaderView::Interactive);
	// The path takes what is left. When nothing is, as with a large font, the header leaves it the size it has, so that
	// is its title's, and the list scrolls.
	header->resizeSection(1, header->sectionSizeHint(1));
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->resizeSection(0, dialog->width() / 4);
	header->resizeSection(2, std::max(90, header->sectionSizeHint(2)));
	header->resizeSection(3, std::max(90, header->sectionSizeHint(3)));
	header->resizeSection(4, _hList->fontMetrics().horizontalAdvance(QStringLiteral("0000-00-00 00:00:00")) + 16);

	_currentColumn = -1;
	_lastSort = -1;
	_reverseSort = false;

	_hList->installEventFilter(new ListKeys(_hList,
		[this](wchar_t ch) { onCharTyped(ch); },
		[this]()
		{
			if (!selectedRows().empty())
				putItemsToClipboard(true);
		}));

	QObject::connect(header, &QHeaderView::sectionClicked, dialog, [this](int column) { onColumnClicked(column); });
	QObject::connect(_hList, &QTableWidget::itemSelectionChanged, dialog, [this]() { updateButtonState(); });
	QObject::connect(_hList, &QTableWidget::cellDoubleClicked, dialog, [this]() { activateCurrent(); });
	QObject::connect(_hList, &QWidget::customContextMenuRequested, dialog, [this](const QPoint& position) { showListMenu(position); });
	QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { activateCurrent(); });
	QObject::connect(ui.cancel, &QAbstractButton::clicked, dialog, [this]() { finish(IDCANCEL); });
	QObject::connect(ui.windowsSave, &QAbstractButton::clicked, dialog, [this]() { doSave(); });
	QObject::connect(ui.windowsClose, &QAbstractButton::clicked, dialog, [this]() { doClose(); });
	QObject::connect(ui.windowsSort, &QAbstractButton::clicked, dialog, [this]() { onSortTabs(); });

	if (_lastKnownLocation.isValid())
	{
		dialog->setGeometry(_lastKnownLocation);
		dialog->show();
	}
	else
	{
		goToCenter();
	}

	doRefresh();
	_hList->setFocus();

	const int result = runModal();
	_hList = nullptr;
	return result;
}

void WindowsDlg::updateColumnNames() const
{
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	const wstring names[nbColumns] = {
		pNativeSpeaker->getAttrNameStr(L"Name", WD_ROOTNODE, WD_CLMNNAME),
		pNativeSpeaker->getAttrNameStr(L"Path", WD_ROOTNODE, WD_CLMNPATH),
		pNativeSpeaker->getAttrNameStr(L"Type", WD_ROOTNODE, WD_CLMNTYPE),
		pNativeSpeaker->getAttrNameStr(L"Size", WD_ROOTNODE, WD_CLMNSIZE),
		pNativeSpeaker->getAttrNameStr(L"Modified time", WD_ROOTNODE, WD_CLMNDT)
	};

	for (int column = 0; column < nbColumns; ++column)
	{
		wstring columnText;
		if (_currentColumn != column)
			columnText = L"⇵ " + names[column];
		else if (_reverseSort)
			columnText = L"△ " + names[column];
		else
			columnText = L"▽ " + names[column];

		QTableWidgetItem* header = _hList->horizontalHeaderItem(column);
		if (!header)
		{
			header = new QTableWidgetItem;
			header->setTextAlignment((column == 2 || column == 3) ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter));
			_hList->setHorizontalHeaderItem(column, header);
		}
		header->setText(QString::fromStdWString(columnText));
	}
}

void WindowsDlg::doRefresh()
{
	if (_hSelf != nullptr && isVisible())
	{
		if (_hList != nullptr)
		{
			size_t count = (_pTab != nullptr) ? _pTab->nbItem() : 0;
			size_t oldSize = _idxMap.size();
			if (count == oldSize)
				return;

			size_t lo = 0;
			_idxMap.resize(count);
			if (oldSize < count)
				lo = oldSize;
			for (size_t i = lo; i < count; ++i)
				_idxMap[i] = static_cast<int>(i);

			refreshList();

			resetSelection();
			updateButtonState();
			doCount();
		}
	}
}

void WindowsDlg::resetSelection()
{
	if (!_pTab)
		return;

	auto curSel = _pTab->getCurrentTabIndex();
	int pos = 0;
	for (vector<int>::iterator itr = _idxMap.begin(), end = _idxMap.end(); itr != end; ++itr, ++pos)
	{
		if (*itr == curSel)
		{
			setRowSelected(pos, true, true);
			_hList->scrollToItem(_hList->item(pos, 0));
		}
		else
		{
			setRowSelected(pos, false);
		}
	}
}

void WindowsDlg::doSave()
{
	const vector<int> rows = selectedRows();
	if (rows.empty())
		return;

	NMWINDLG nmdlg{};
	nmdlg.type = WDT_SAVE;
	nmdlg.curSel = rows.front();
	nmdlg.hwndFrom = _hSelf;
	nmdlg.code = WDN_NOTIFY;
	nmdlg.nItems = static_cast<UINT>(rows.size());
	std::vector<UINT> items(nmdlg.nItems);
	nmdlg.Items = items.data();

	for (UINT j = 0; j < nmdlg.nItems; ++j)
		nmdlg.Items[j] = _idxMap[rows[j]];

	::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));
	refreshList();
}

void WindowsDlg::activateCurrent()
{
	const vector<int> rows = selectedRows();
	if (rows.size() == 1)
	{
		NMWINDLG nmdlg{};
		nmdlg.type = WDT_ACTIVATE;
		nmdlg.curSel = _idxMap[rows.front()];
		nmdlg.hwndFrom = _hSelf;
		nmdlg.code = WDN_NOTIFY;
		::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));

		finish(IDOK);
	}
}

void WindowsDlg::doClose()
{
	const vector<int> rows = selectedRows();
	if (rows.empty())
		return;

	int index = rows.front();

	NMWINDLG nmdlg{};
	nmdlg.type = WDT_CLOSE;
	nmdlg.curSel = _idxMap[index];
	nmdlg.hwndFrom = _hSelf;
	nmdlg.code = WDN_NOTIFY;
	UINT n = nmdlg.nItems = static_cast<UINT>(rows.size());
	std::vector<UINT> items(nmdlg.nItems);
	nmdlg.Items = items.data();
	vector<int> key;
	key.resize(n, 0x7fffffff);

	_hList->clearSelection();
	for (UINT j = 0; j < n; ++j)
	{
		nmdlg.Items[j] = _idxMap[rows[j]];
		key[j] = rows[j];
	}
	::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));
	if (nmdlg.processed)
	{
		// Trying to retain sort order. fairly sure there is a much better algorithm for this
		auto kitr = key.begin();
		for (UINT k = 0; k < n; ++k, ++kitr)
		{
			if (nmdlg.Items[k] == static_cast<UINT>(-1))
			{
				int oldVal = _idxMap[*kitr];
				_idxMap[*kitr] = -1;
				for (auto itr = _idxMap.begin(), end = _idxMap.end(); itr != end; ++itr)
					if (*itr > oldVal)
						--(*itr);
			}
		}
		_idxMap.erase(std::remove_if(_idxMap.begin(), _idxMap.end(), [](int x) { return x == -1; }), _idxMap.end());
	}

	if (_idxMap.size() < 1)
	{
		finish(IDCANCEL);
		return;
	}
	else if (_pTab->nbItem() != _idxMap.size())
	{
		doRefresh();
		dlgItem(IDC_WINDOWS_CLOSE)->setFocus();
	}
	else
	{
		refreshList();

		// select first previously selected item (or last one if only the last one was removed)
		if (index == static_cast<int>(_idxMap.size()))
			index -= 1;

		if (index >= 0)
			setRowSelected(index, true);

		dlgItem(IDC_WINDOWS_CLOSE)->setFocus();
	}
	doCount();
}

//this function will be called everytime when close is performed
//as well as each time refresh is performed to keep updated
void WindowsDlg::doCount()
{
	NativeLangSpeaker* pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();

	wstring msg = pNativeSpeaker->getAttrNameStr(L"Windows", "Dialog", "Window", "title");
	msg += L" - ";
	msg += pNativeSpeaker->getAttrNameStr(L"Total documents: ", WD_ROOTNODE, WD_NBDOCSTOTAL);
	msg += L" ";
	msg += to_wstring(_idxMap.size());
	_hSelf->setWindowTitle(QString::fromStdWString(msg));
}

void WindowsDlg::doSort()
{
	if (!_pTab)
		return;

	size_t count =  _pTab->nbItem();
	std::vector<UINT> items(count);
	auto currrentTabIndex = _pTab->getCurrentTabIndex();
	NMWINDLG nmdlg{};
	nmdlg.type = WDT_SORT;
	nmdlg.hwndFrom = _hSelf;
	nmdlg.curSel = currrentTabIndex;
	nmdlg.code = WDN_NOTIFY;
	nmdlg.nItems = static_cast<UINT>(count);
	nmdlg.Items = items.data();
	for (size_t i=0; i < count; ++i)
	{
		nmdlg.Items[i] = _idxMap[i];
	}
	::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));
	if (nmdlg.processed)
	{
		_idxMap.clear();
		refreshMap();
	}

	//After sorting, need to open the active tab before sorting
	//This will be helpful when large number of documents are opened
	long long newPosition = -1;
	std::vector<int>::iterator it = std::find(_idxMap.begin(), _idxMap.end(), currrentTabIndex);
	if (it != _idxMap.end())
	{
		newPosition = it - _idxMap.begin();
	}
	nmdlg.type = WDT_ACTIVATE;
	nmdlg.curSel = static_cast<UINT>(newPosition);
	nmdlg.hwndFrom = _hSelf;
	nmdlg.code = WDN_NOTIFY;
	::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));
}

void WindowsDlg::sort(int columnID, bool reverseSort)
{
	refreshMap();
	_currentColumn = columnID;
	_reverseSort = reverseSort;
	stable_sort(_idxMap.begin(), _idxMap.end(), BufferEquivalent(_pTab, _currentColumn, _reverseSort));
}

void WindowsDlg::sortFileNameASC()
{
	sort(0, false);
}

void WindowsDlg::sortFileNameDSC()
{
	sort(0, true);
}

void WindowsDlg::sortFilePathASC()
{
	sort(1, false);
}

void WindowsDlg::sortFilePathDSC()
{
	sort(1, true);
}

void WindowsDlg::sortFileTypeASC()
{
	sort(2, false);
}

void WindowsDlg::sortFileTypeDSC()
{
	sort(2, true);
}

void WindowsDlg::sortFileSizeASC()
{
	sort(3, false);
}

void WindowsDlg::sortFileSizeDSC()
{
	sort(3, true);
}

void WindowsDlg::sortDateTimeASC()
{
	sort(4, false);
}

void WindowsDlg::sortDateTimeDSC()
{
	sort(4, true);
}

void WindowsDlg::refreshMap()
{
	size_t count = (_pTab != NULL) ? _pTab->nbItem() : 0;
	size_t oldSize = _idxMap.size();
	if (count == oldSize)
		return;

	size_t lo = 0;
	_idxMap.resize(count);
	if (oldSize < count)
		lo = oldSize;

	for (size_t i = lo; i < count; ++i)
		_idxMap[i] = static_cast<int>(i);
}

void WindowsDlg::doSortToTabs()
{
	const vector<int> rows = selectedRows();
	int curSel = rows.empty() ? 0 : rows.front();

	NMWINDLG nmdlg{};
	nmdlg.type = WDT_SORT;
	nmdlg.hwndFrom = _hSelf;
	nmdlg.curSel = _idxMap[curSel];
	nmdlg.code = WDN_NOTIFY;
	nmdlg.nItems = static_cast<UINT>(_idxMap.size());
	std::vector<UINT> items(nmdlg.nItems);
	nmdlg.Items = items.data();

	for (UINT j = 0; j < nmdlg.nItems; ++j)
	{
		nmdlg.Items[j] = _idxMap[j];
		if (static_cast<int>(j) == curSel)
			nmdlg.curSel = j;
	}

	::SendMessage(_hParent, WDN_NOTIFY, 0, reinterpret_cast<LPARAM>(&nmdlg));
	if (nmdlg.processed)
	{
		_idxMap.clear();
		doRefresh();
	}
}

void WindowsDlg::putItemsToClipboard(bool isFullPath)
{
	// Not the text of the list, because its first column may contain "*" or "[Read Only]".
	std::vector<Buffer*> buffers;
	for (int row : selectedRows())
		buffers.push_back(getBuffer(row));

	buf2Clipboard(buffers, isFullPath, _hList);
}

Buffer* WindowsDlg::getBuffer(int index) const
{
	if (index < 0 || index >= static_cast<int>(_idxMap.size()))
		return nullptr;

	index = _idxMap[index];
	if (index < 0 || !_pTab || index >= static_cast<int>(_pTab->nbItem()))
		return nullptr;

	BufferID bufID = _pTab->getBufferByIndex(index);
	return MainFileManager.getBufferByID(bufID);
}

void WindowsMenu::init(HMENU hMainMenu, HWND commandTarget)
{
	_hMenu = nppSubMenuAt(hMainMenu, MENUINDEX_WINDOW);
	_hMenuList = nppSubMenuAt(hMainMenu, MENUINDEX_LIST);
	_commandTarget = commandTarget;

	for (HMENU menu : { _hMenu, _hMenuList })
	{
		if (menu)
		{
			QObject::connect(menu, &QMenu::aboutToShow, menu, [commandTarget, menu]()
			{
				::SendMessage(commandTarget, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(menu), 0);
			});
		}
	}
}

void WindowsMenu::initPopupMenu(HMENU hMenu, DocTabView* pTab)
{
	bool isDropListMenu = false;

	UINT firstId = 0;
	UINT limitId = 0;

	if (hMenu == _hMenu)
	{
		firstId = IDM_WINDOW_MRU_FIRST;
		limitId = IDM_WINDOW_MRU_LIMIT;
	}
	else if (hMenu == _hMenuList)
	{
		isDropListMenu = true;

		if (_limitPrev < pTab->nbItem())
		{
			_limitPrev = static_cast<UINT>(pTab->nbItem());
		}

		firstId = IDM_DROPLIST_MRU_FIRST;
		limitId = IDM_DROPLIST_MRU_FIRST + _limitPrev - 1;
	}

	if (firstId > 0 && limitId > 0)
	{
		auto curDoc = pTab->getCurrentTabIndex();
		size_t nMaxDoc = static_cast<size_t>(limitId) - firstId + 1;
		size_t nDoc = pTab->nbItem();
		nDoc = std::min<size_t>(nDoc, nMaxDoc);
		UINT id = firstId;
		UINT guard = firstId + static_cast<UINT>(nDoc);
		size_t pos = 0;
		for (; id < guard; ++id, ++pos)
		{
			BufferID bufID = pTab->getBufferByIndex(pos);
			Buffer* buf = MainFileManager.getBufferByID(bufID);

			wstring strBuffer(BuildMenuFileName(60, static_cast<int>(pos), buf->getFileName(), !isDropListMenu));
			if (buf->isDirty())
			{
				// add a '*' after the modified tab name (like Visual Studio)
				strBuffer += L'*';
			}

			if (!nppFindMenuAction(hMenu, id))
			{
				nppAddMenuItem(hMenu, id, strBuffer, _commandTarget);
				if (isDropListMenu)
				{
					nppRemoveMenuCommand(hMenu, IDM_DROPLIST_LIST);
				}
			}
			else
			{
				nppSetMenuCommandText(hMenu, id, strBuffer);
			}
			nppCheckMenuCommand(hMenu, id, static_cast<int>(pos) == curDoc);
		}
		for (; id <= limitId; ++id)
		{
			nppRemoveMenuCommand(hMenu, id);
		}

		if (isDropListMenu)
		{
			_limitPrev = static_cast<UINT>(pTab->nbItem());
		}
	}
}
