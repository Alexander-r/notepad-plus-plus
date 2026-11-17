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


#include "ListView.h"

#include <algorithm>
#include <QHeaderView>
#include <QPalette>
#include <QTableWidget>

namespace
{
	constexpr int lParamRole = Qt::UserRole + 1;
}

QTableWidget* ListView::table() const
{
	return static_cast<QTableWidget*>(_hSelf);
}

void ListView::init(HINSTANCE hInst, HWND parent, UINT_PTR listViewID)
{
	Window::init(hInst, parent);

	auto* list = new QTableWidget(parent);
	_hSelf = list;
	list->setProperty("nppId", static_cast<int>(listViewID));
	list->setColumnCount(static_cast<int>(_columnInfos.size()));
	list->setSelectionBehavior(QAbstractItemView::SelectRows);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setEditTriggers(QAbstractItemView::NoEditTriggers);
	list->setShowGrid(false);
	list->setWordWrap(false);
	list->verticalHeader()->hide();
	list->verticalHeader()->setDefaultSectionSize(list->fontMetrics().height() + 4);
	list->horizontalHeader()->setSectionsClickable(false);
	list->horizontalHeader()->setStretchLastSection(true);

	for (int i = 0; i < static_cast<int>(_columnInfos.size()); ++i)
	{
		list->setHorizontalHeaderItem(i, new QTableWidgetItem(QString::fromStdWString(_columnInfos[i]._label)));
		list->setColumnWidth(i, std::max(static_cast<int>(_columnInfos[i]._width), list->horizontalHeader()->sectionSizeHint(i))); // never cutting off the title
	}
}

void ListView::destroy()
{
	delete _hSelf;
	_hSelf = nullptr;
}

void ListView::setColumnText(size_t i, const std::wstring& txt2Set)
{
	if (auto* item = table()->horizontalHeaderItem(static_cast<int>(i)))
	{
		item->setText(QString::fromStdWString(txt2Set));
		QHeaderView* header = table()->horizontalHeader();
		header->resizeSection(static_cast<int>(i), std::max(header->sectionSize(static_cast<int>(i)), header->sectionSizeHint(static_cast<int>(i))));
	}
}

size_t ListView::findAlphabeticalOrderPos(const std::wstring& string2Cmp, SortDirection sortDir)
{
	const QString text = QString::fromStdWString(string2Cmp);
	const int count = table()->rowCount();
	for (int i = 0; i < count; ++i)
	{
		const int order = QString::compare(text, table()->item(i, 0)->text(), Qt::CaseInsensitive);
		if ((sortDir == sortEncrease && order < 0) || (sortDir == sortDecrease && order > 0))
			return i;
	}
	return count;
}

void ListView::addLine(const std::vector<std::wstring>& values2Add, LPARAM lParam, int pos2insert)
{
	if (values2Add.empty())
		return;

	const int row = (pos2insert == -1) ? table()->rowCount() : pos2insert;
	table()->insertRow(row);

	const int columns = std::min(table()->columnCount(), static_cast<int>(values2Add.size()));
	for (int column = 0; column < columns; ++column)
		table()->setItem(row, column, new QTableWidgetItem(QString::fromStdWString(values2Add[column])));

	if (table()->item(row, 0))
		table()->item(row, 0)->setData(lParamRole, QVariant::fromValue<qlonglong>(lParam));
}

size_t ListView::nbItem() const
{
	return table()->rowCount();
}

long ListView::getSelectedIndex() const
{
	return table()->currentRow();
}

void ListView::setSelection(int itemIndex) const
{
	table()->selectRow(itemIndex);
	if (auto* item = table()->item(itemIndex, 0))
		table()->scrollToItem(item);
}

LPARAM ListView::getLParamFromIndex(int itemIndex) const
{
	const auto* item = table()->item(itemIndex, 0);
	return item ? static_cast<LPARAM>(item->data(lParamRole).toLongLong()) : 0;
}

bool ListView::removeFromIndex(size_t i)
{
	if (i >= nbItem())
		return false;

	table()->removeRow(static_cast<int>(i));
	return true;
}

void ListView::removeAll()
{
	table()->setRowCount(0);
}

std::wstring ListView::getText(int row, int column) const
{
	const auto* item = table()->item(row, column);
	return item ? item->text().toStdWString() : std::wstring{};
}

void ListView::setColours(COLORREF background, COLORREF text)
{
	QPalette palette = table()->palette();
	palette.setColor(QPalette::Base, QColor(GetRValue(background), GetGValue(background), GetBValue(background)));
	palette.setColor(QPalette::Text, QColor(GetRValue(text), GetGValue(text), GetBValue(text)));
	table()->setPalette(palette);
}
