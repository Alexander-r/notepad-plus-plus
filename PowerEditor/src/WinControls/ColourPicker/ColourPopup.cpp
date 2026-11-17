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


#include "ColourPopup.h"

#include <QAbstractButton>
#include <QColorDialog>
#include <QDialog>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>

#include <array>
#include <utility>

#include "ColourPopupResource.h"
#include "Parameters.h"
#include "StaticDialog.h"
#include "localization.h"
#include "ui_ColourPopupDlg.h"

static constexpr std::array<COLORREF, 48> colorItems{ {
	RGB(  0,   0,   0), RGB( 64,   0,   0), RGB(128,   0,   0), RGB(128,  64,  64), RGB(255,   0,   0), RGB(255, 128, 128),
	RGB(255, 255, 128), RGB(255, 255,   0), RGB(255, 128,  64), RGB(255, 128,   0), RGB(128,  64,   0), RGB(128, 128,   0),
	RGB(128, 128,  64), RGB(  0,  64,   0), RGB(  0, 128,   0), RGB(  0, 255,   0), RGB(128, 255,   0), RGB(128, 255, 128),
	RGB(  0, 255, 128), RGB(  0, 255,  64), RGB(  0, 128, 128), RGB(  0, 128,  64), RGB(  0,  64,  64), RGB(128, 128, 128),
	RGB( 64, 128, 128), RGB(  0,   0, 128), RGB(  0,   0, 255), RGB(  0,  64, 128), RGB(  0, 255, 255), RGB(128, 255, 255),
	RGB(  0, 128, 255), RGB(  0, 128, 192), RGB(128, 128, 255), RGB(  0,   0, 160), RGB(  0,   0,  64), RGB(192, 192, 192),
	RGB( 64,   0,  64), RGB( 64,   0,  64), RGB(128,   0, 128), RGB(128,   0,  64), RGB(128, 128, 192), RGB(255, 128, 192),
	RGB(255, 128, 255), RGB(255,   0, 255), RGB(255,   0, 128), RGB(128,   0, 255), RGB( 64,   0, 128), RGB(255, 255, 255),
} };

namespace
{
	QColor toQColor(COLORREF colour)
	{
		return QColor(GetRValue(colour), GetGValue(colour), GetBValue(colour));
	}

	COLORREF toColorRef(const QColor& colour)
	{
		return RGB(colour.red(), colour.green(), colour.blue());
	}

	constexpr int swatchSize = 20;
	constexpr int cellSize = swatchSize + 6;
	constexpr int nbColumns = 8;
	constexpr int nbRows = 6;
}

void ColourPopup::doDialog(const QPoint& position, std::function<void(COLORREF)> onChosen)
{
	_onChosen = std::move(onChosen);

	QDialog* dialog = createDialogWidget(false);
	Ui::ColourPopupDlg ui;
	ui.setupUi(dialog);
	dialog->setWindowFlags(Qt::Popup);

	QListWidget* list = ui.colourList;
	list->setViewMode(QListView::IconMode);
	list->setMovement(QListView::Static);
	list->setResizeMode(QListView::Adjust);
	list->setIconSize(QSize(swatchSize, swatchSize));
	list->setGridSize(QSize(cellSize, cellSize));
	list->setUniformItemSizes(true);
	list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list->setFixedSize(nbColumns * cellSize + 2 * list->frameWidth() + 2, nbRows * cellSize + 2 * list->frameWidth() + 2);

	for (const COLORREF colour : colorItems)
	{
		QPixmap swatch(swatchSize, swatchSize);
		swatch.fill(toQColor(colour));
		QPainter painter(&swatch);
		painter.setPen(Qt::gray);
		painter.drawRect(0, 0, swatchSize - 1, swatchSize - 1);
		painter.end();

		auto* item = new QListWidgetItem(QIcon(swatch), QString(), list);
		item->setData(Qt::UserRole, static_cast<uint>(colour));
	}

	const NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
	pNativeSpeaker->changeDlgLang(dialog, "ColorPopup");

	// The chooser may destroy this object, so nothing here can use it afterwards
	QObject::connect(list, &QListWidget::itemClicked, dialog, [this](QListWidgetItem* item)
	{
		const std::function<void(COLORREF)> onChosen = _onChosen;
		onChosen(static_cast<COLORREF>(item->data(Qt::UserRole).toUInt()));
	});
	QObject::connect(ui.ok, &QAbstractButton::clicked, dialog, [this]() { chooseFromDialog(); });

	dialog->move(position);
	dialog->show();
}

void ColourPopup::chooseFromDialog()
{
	_hSelf->hide();

	// These are kept with the settings
	auto& customColors = NppParameters::getInstance().getNppGUI()._colorPickerCustomColors;
	for (int i = 0; i < static_cast<int>(customColors.size()); ++i)
		QColorDialog::setCustomColor(i, toQColor(customColors[i]));

	const QColor chosen = QColorDialog::getColor(toQColor(_colour), _hParent);

	for (int i = 0; i < static_cast<int>(customColors.size()); ++i)
		customColors[i] = toColorRef(QColorDialog::customColor(i));

	const std::function<void(COLORREF)> onChosen = _onChosen;
	onChosen(chosen.isValid() ? toColorRef(chosen) : _colour);
}
