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


#include "documentMap.h"

#include <QDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "NppConstants.h"
#include "ScintillaEditView.h"
#include "menuCmdID.h"

static constexpr bool moveDown = true;
static constexpr bool moveUp = false;

namespace
{
	QColor colorOf(COLORREF colour, int alpha)
	{
		return QColor(GetRValue(colour), GetGValue(colour), GetBValue(colour), alpha);
	}

	class ViewZoneWidget : public QWidget
	{
	public:
		ViewZoneWidget(ViewZoneDlg* zone, DocumentMap* map, QWidget* parent) : QWidget(parent), _zone(zone), _map(map)
		{
			setFocusPolicy(Qt::ClickFocus);
		}

	protected:
		void paintEvent(QPaintEvent*) override
		{
			constexpr int transparency = 50; // 0 <= transparency < 256
			QPainter painter(this);
			painter.fillRect(rect(), colorOf(ViewZoneDlg::colour(ViewZoneDlg::ViewZoneColorIndex::frost), transparency));
			painter.fillRect(0, _zone->higherY(), width(), _zone->getViewerHeight(), colorOf(ViewZoneDlg::colour(ViewZoneDlg::ViewZoneColorIndex::focus), transparency));
		}

		void mousePressEvent(QMouseEvent* event) override
		{
			_map->onMouseAt(event->position().toPoint().y());
		}

		void mouseMoveEvent(QMouseEvent* event) override
		{
			if (event->buttons() & Qt::LeftButton)
				_map->onMouseAt(event->position().toPoint().y());
		}

		void wheelEvent(QWheelEvent* event) override
		{
			_map->onWheel(event->angleDelta().y());
		}

		void keyPressEvent(QKeyEvent* event) override
		{
			switch (event->key())
			{
				case Qt::Key_Up: _map->scrollMap(moveUp, perLine); break;
				case Qt::Key_Down: _map->scrollMap(moveDown, perLine); break;
				case Qt::Key_PageUp: _map->scrollMap(moveUp, perPage); break;
				case Qt::Key_PageDown: _map->scrollMap(moveDown, perPage); break;
				default: QWidget::keyPressEvent(event); break;
			}
		}

	private:
		ViewZoneDlg* _zone;
		DocumentMap* _map;
	};

	class MapResizeEvents : public QObject
	{
	public:
		MapResizeEvents(DocumentMap* map, QObject* parent) : QObject(parent), _map(map) {}

		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::Resize)
				_map->onResized();
			return QObject::eventFilter(watched, event);
		}

	private:
		DocumentMap* _map;
	};
}

void DocumentMap::reloadMap()
{
	if (_pMapView && _ppEditView)
	{
		Document currentDoc = (*_ppEditView)->execute(SCI_GETDOCPOINTER);
		_pMapView->execute(SCI_SETDOCPOINTER, 0, static_cast<LPARAM>(currentDoc));

		//
		// sync with the current document
		//

		Buffer *editBuf = (*_ppEditView)->getCurrentBuffer();
		_pMapView->setCurrentBuffer(editBuf);

		// folding
		std::vector<size_t> lineStateVector;
		(*_ppEditView)->getCurrentFoldStates(lineStateVector);
		_pMapView->syncFoldStateWith(lineStateVector);

		// Wrapping
		if ((*_ppEditView)->isWrap() && needToRecomputeWith())
		{
			wrapMap();
		}

		scrollMap();
	}
}

void DocumentMap::showInMapTemporarily(Buffer *buf2show, ScintillaEditView *fromEditView)
{
	if (_pMapView && fromEditView)
	{
		_pMapView->execute(SCI_SETDOCPOINTER, 0, static_cast<LPARAM>(buf2show->getDocument()));
		_pMapView->setCurrentBuffer(buf2show);

		// folding
		const std::vector<size_t> & lineStateVector = buf2show->getHeaderLineState(fromEditView);
		_pMapView->syncFoldStateWith(lineStateVector);

		// Wrapping
		if (fromEditView->isWrap() && needToRecomputeWith(fromEditView))
		{
			wrapMap(fromEditView);
		}

		MapPosition mp = buf2show->getMapPosition();
		if (mp.isValid())
			scrollMapWith(mp);
	}
}

void DocumentMap::setSyntaxHiliting()
{
	Buffer *buf = _pMapView->getCurrentBuffer();
	_pMapView->defineDocType(buf->getLangType());
	_pMapView->showMargin(ScintillaEditView::_SC_MARGE_FOLDER, false);
}

bool DocumentMap::needToRecomputeWith(const ScintillaEditView *editView)
{
	const ScintillaEditView *pEditView = editView ? editView : *_ppEditView;

	auto currentZoom = pEditView->execute(SCI_GETZOOM);
	if (_displayZoom != currentZoom)
		return true;

	int currentTextZoneWidth = pEditView->getTextZoneWidth();
	if (_displayWidth != currentTextZoneWidth)
		return true;

	return false;
}

void DocumentMap::initWrapMap()
{
	if (_pMapView && _ppEditView)
	{
		RECT rect{};
		getClientRect(rect);
		_pMapView->getHSelf()->setGeometry(0, 0, rect.right - rect.left, rect.bottom-rect.top);
		_pMapView->wrap(false);
		_pMapView->redraw(true);

		bool isRTL = (*_ppEditView)->isTextDirectionRTL();
		if (_pMapView->isTextDirectionRTL() != isRTL)
			_pMapView->changeTextDirection(isRTL);
	}
}

void DocumentMap::changeTextDirection(bool isRTL)
{
	_pMapView->changeTextDirection(isRTL);
}

/*
double ddd = (double)Xlength1/(double)Xlength2;
char dchar[256];
sprintf(dchar, "%f", ddd);
::MessageBoxA(NULL, dchar, "", MB_OK);
		
		// -10    => 1
		// -9     => 1
		// -8     => 1
		// -7     => 1
		// -6     => 1.5
		// -5     => 2
		// -4     => 2.5
		// -3     => 2.5
		// -2     => 3.5
		// -1     => 3.5
		// 0: -10 => 4
		// 1      => 4.5
		// 2      => 5
		// 3      => 5
		// 4      => 5.5
		// 5      => 6
		// 6      => 6.5
		// 7      => 7
		// 8      => 7
		// 9      => 7.5
		// 10     => 8
		// 11     => 8.5
		// 12     => 8.5
		// 13     => 9.5
		// 14     => 9.5
		// 15     => 10
		// 16     => 10.5
		// 17     => 11
		// 18     => 11
		// 19     => 11.5
		// 20     => 12
*/

static constexpr double zoomRatio[]{ 1, 1, 1, 1, 1.5, 2, 2.5, 2.5, 3.5, 3.5,
4, 4.5, 5, 5, 5.5, 6, 6.5, 7, 7, 7.5, 8, 8.5, 8.5, 9.5, 9.5, 10, 10.5, 11, 11, 11.5, 12 };

void DocumentMap::wrapMap(const ScintillaEditView *editView)
{
	const ScintillaEditView *pEditView = editView ? editView : *_ppEditView;
	RECT rect{};
	getClientRect(rect);
	if (pEditView->isWrap())
	{
		// get current scintilla width W1
		int editZoneWidth = pEditView->getTextZoneWidth();

		// update the wrap needed data
		_displayWidth = editZoneWidth;
		_displayZoom = pEditView->execute(SCI_GETZOOM);
		double zr = zoomRatio[_displayZoom + 10];

		// compute doc map width: dzw/ezw = 1/zoomRatio
		double docMapWidth = editZoneWidth / zr;

		_pMapView->getHSelf()->setGeometry(0, 0, int(docMapWidth), rect.bottom-rect.top);
		_pMapView->wrap(true);

		// sync wrapping indent mode
		_pMapView->execute(SCI_SETWRAPINDENTMODE, pEditView->execute(SCI_GETWRAPINDENTMODE));

		const ScintillaViewParams& svp = NppParameters::getInstance().getSVP();

		if (svp._paddingLeft || svp._paddingRight)
		{
			intptr_t paddingMapLeft = static_cast<intptr_t>(svp._paddingLeft / (editZoneWidth / docMapWidth));
			intptr_t paddingMapRight = static_cast<intptr_t>(svp._paddingRight / (editZoneWidth / docMapWidth));
			_pMapView->execute(SCI_SETMARGINLEFT, 0, paddingMapLeft);
			_pMapView->execute(SCI_SETMARGINRIGHT, 0, paddingMapRight);
		}
	}

	doMove();
}

void DocumentMap::scrollMap()
{
	if (_pMapView && _ppEditView)
	{
		// Get the position of the 1st and last showing chars from the original edit view
		RECT rcEditView;
		(*_ppEditView)->getClientRect(rcEditView);
		LRESULT higherPos = (*_ppEditView)->execute(SCI_POSITIONFROMPOINT, 0, 0);
		LRESULT lowerPos = (*_ppEditView)->execute(SCI_POSITIONFROMPOINT, rcEditView.right - rcEditView.left, rcEditView.bottom - rcEditView.top);

		// Let Scintilla scroll the map
		_pMapView->execute(SCI_GOTOPOS, higherPos);
		_pMapView->execute(SCI_GOTOPOS, lowerPos);

		// Get top position of orange marker window
		RECT rcMapView;
		_pMapView->getClientRect(rcMapView);
		LRESULT higherY = _pMapView->execute(SCI_POINTYFROMPOSITION, 0, higherPos);

		// Get bottom position of orange marker window
		LRESULT lowerY = 0;
		LRESULT lineHeightMapView  = _pMapView->execute(SCI_TEXTHEIGHT, 0);
		if (!(*_ppEditView)->isWrap())
		{ // not wrapped: mimic height of edit view
			LRESULT lineHeightEditView = (*_ppEditView)->execute(SCI_TEXTHEIGHT, 0);
			lowerY = higherY + lineHeightMapView * (rcEditView.bottom - rcEditView.top) / lineHeightEditView;
		}
		else
		{ // wrapped: ask Scintilla, since in the map view the current range of edit view might be wrapped differently
			lowerY = _pMapView->execute(SCI_POINTYFROMPOSITION, 0, lowerPos) + lineHeightMapView;
		}

		//
		// Mark view zone in map
		//
		_vzDlg.drawZone(static_cast<int32_t>(higherY), static_cast<int32_t>(lowerY));
	}
}

void DocumentMap::scrollMapWith(const MapPosition & mapPos)
{
	if (_pMapView)
	{
		// Visible document line for the map view
		auto firstVisibleDisplayLineMap = _pMapView->execute(SCI_GETFIRSTVISIBLELINE);
		auto firstVisibleDocLineMap = _pMapView->execute(SCI_DOCLINEFROMVISIBLE, firstVisibleDisplayLineMap);
		auto nbLineMap = _pMapView->execute(SCI_LINESONSCREEN, firstVisibleDocLineMap);
		auto lastVisibleDocLineMap = _pMapView->execute(SCI_DOCLINEFROMVISIBLE, firstVisibleDisplayLineMap + nbLineMap);

		// If part of editor view is out of map, then scroll map
		LRESULT mapLineToScroll = 0;
		if (lastVisibleDocLineMap < mapPos._lastVisibleDocLine)
			mapLineToScroll = mapPos._lastVisibleDocLine;
		else
			mapLineToScroll = mapPos._firstVisibleDocLine;
		//
		// Scroll to make whole view zone visible
		//
		_pMapView->execute(SCI_GOTOLINE, mapLineToScroll);

		// Get the editor's higher/lower Y, then compute the map's higher/lower Y
		LRESULT higherY = 0;
		LRESULT lowerY = 0;
		if (!mapPos._isWrap)
		{
			auto higherPos = _pMapView->execute(SCI_POSITIONFROMLINE, mapPos._firstVisibleDocLine);
			auto lowerPos = _pMapView->execute(SCI_POSITIONFROMLINE, mapPos._lastVisibleDocLine);
			higherY = _pMapView->execute(SCI_POINTYFROMPOSITION, 0, higherPos);
			lowerY = _pMapView->execute(SCI_POINTYFROMPOSITION, 0, lowerPos);
			if (lowerY == 0)
			{
				auto lineHeight = _pMapView->execute(SCI_TEXTHEIGHT, mapPos._firstVisibleDocLine);
				lowerY = mapPos._nbLine * lineHeight + mapPos._firstVisibleDocLine;
			}
		}
		else
		{
			higherY = _pMapView->execute(SCI_POINTYFROMPOSITION, 0, mapPos._higherPos);
			auto lineHeight = _pMapView->execute(SCI_TEXTHEIGHT, mapPos._firstVisibleDocLine);
			lowerY = mapPos._nbLine * lineHeight + higherY;
		}

		//
		// Mark view zone in map
		//
		_vzDlg.drawZone(static_cast<int32_t>(higherY), static_cast<int32_t>(lowerY));
	}
}

void DocumentMap::doMove()
{
	if (_hwndScintilla && _vzDlg.getHSelf())
	{
		_vzDlg.getHSelf()->setGeometry(_hwndScintilla->geometry());
		_vzDlg.getHSelf()->raise();
	}
}

void DocumentMap::fold(size_t line, bool foldOrNot)
{
	_pMapView->fold(line, foldOrNot, false);
}

void DocumentMap::foldAll(bool mode)
{
	_pMapView->foldAll(mode);
}

void DocumentMap::scrollMap(bool direction, moveMode whichMode)
{
	// Visible line for the code view
	auto firstVisibleDisplayLine = (*_ppEditView)->execute(SCI_GETFIRSTVISIBLELINE);
	auto nbLine = (*_ppEditView)->execute(SCI_LINESONSCREEN, firstVisibleDisplayLine);
	auto nbLine2go = (whichMode == perLine ? 1 : nbLine);
	(*_ppEditView)->execute(SCI_LINESCROLL, 0, (direction == moveDown) ? nbLine2go : -nbLine2go);

	scrollMap();
}

void DocumentMap::redraw(bool) const
{
	_pMapView->execute(SCI_COLOURISE, 0, -1);
	DockingDlgInterface::redraw(true);
}

void DocumentMap::create(DockedWidgetData* data, bool isRTL)
{
	DockingDlgInterface::create(data, isRTL);
	data->pszAddInfo = id4dockingCont.c_str();

	_hwndScintilla = reinterpret_cast<HWND>(::SendMessage(_hParent, NPPM_CREATESCINTILLAHANDLE, 0, reinterpret_cast<LPARAM>(_hSelf)));
	_pMapView = reinterpret_cast<ScintillaEditView *>(::SendMessage(_hParent, NPPM_INTERNAL_GETSCINTEDTVIEW, 0, reinterpret_cast<LPARAM>(_hwndScintilla)));
	_pMapView->execute(SCI_SETZOOM, static_cast<WPARAM>(-10), 0);
	_pMapView->execute(SCI_SETVSCROLLBAR, FALSE, 0);
	_pMapView->execute(SCI_SETHSCROLLBAR, FALSE, 0);
	_pMapView->execute(SCI_SETMODEVENTMASK, MODEVENTMASK_OFF); // Turn off the modification event

	_pMapView->showIndentGuideLine(false);
	_pMapView->display();

	reloadMap();

	_vzDlg.init(_hInst, _hSelf, this);
	_vzDlg.display();

	setSyntaxHiliting();

	_pMapView->showMargin(0, false);
	_pMapView->showMargin(1, false);
	_pMapView->showMargin(2, false);
	_pMapView->showMargin(3, false);

	_hSelf->installEventFilter(new MapResizeEvents(this, _hSelf));
	onResized();
}

void DocumentMap::onResized()
{
	if (_pMapView && _vzDlg.getHSelf())
	{
		if (!_pMapView->isWrap())
			_pMapView->getHSelf()->setGeometry(_hSelf->rect());

		doMove();
	}
}

void DocumentMap::onMouseAt(int y)
{
	const int currentCenterPosY = _vzDlg.getCurrentCenterPosY();
	const intptr_t pixelPerLine = _pMapView->execute(SCI_TEXTHEIGHT, 0);
	const intptr_t jumpDistance = y - currentCenterPosY;
	const intptr_t nbLine2jump = jumpDistance/pixelPerLine;
	(*_ppEditView)->execute(SCI_LINESCROLL, 0, nbLine2jump);

	scrollMap();
}

void DocumentMap::onWheel(int delta)
{
	constexpr int linesPerNotch = 3;
	(*_ppEditView)->execute(SCI_LINESCROLL, 0, -delta * linesPerNotch / 120);

	scrollMap();
}

void DocumentMap::onDockingNotify(const NMHDR& notification)
{
	switch (notification.code)
	{
		case DMN_CLOSE:
			::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_DOC_MAP, 0);
			return;

		case DMN_SWITCHIN:
			_vzDlg.display();
			reloadMap();
			setSyntaxHiliting();
			return;

		case DMN_SWITCHOFF:
			_vzDlg.display(false);
			return;

		case DMN_FLOATDROPPED:
			doMove();
			scrollMap();
			return;

		default:
			DockingDlgInterface::onDockingNotify(notification);
			return;
	}
}

COLORREF ViewZoneDlg::_focus = RGB(0xFF, 0x80, 0x00);
COLORREF ViewZoneDlg::_frost = RGB(0xFF, 0xFF, 0xFF);

void ViewZoneDlg::setColour(COLORREF colour2Set, ViewZoneColorIndex i)
{
	switch (i)
	{
		case ViewZoneColorIndex::focus:
		{
			_focus = colour2Set;
			break;
		}

		case ViewZoneColorIndex::frost:
		{
			_frost = colour2Set;
			break;
		}

		default:
			return;
	}
}

COLORREF ViewZoneDlg::colour(ViewZoneColorIndex i)
{
	return (i == ViewZoneColorIndex::focus) ? _focus : _frost;
}

void ViewZoneDlg::init(HINSTANCE hInst, HWND parent, DocumentMap* map)
{
	Window::init(hInst, parent);
	_hSelf = new ViewZoneWidget(this, map, parent);
}
