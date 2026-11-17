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


#include "Printer.h"

#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPicture>
#include <QPrintDialog>
#include <QPrinter>

#include <algorithm>
#include <string>

#include <Scintilla.h>

#include "Parameters.h"
#include "RunDlg.h"
#include "ScintillaEditView.h"
#include "localization.h"

using namespace std;

namespace
{
	void replaceStr(wstring & str, const wstring& str2BeReplaced, const wstring& replacement)
	{
		size_t pos = str.find(str2BeReplaced);

		if (pos != str.npos)
			str.replace(pos, str2BeReplaced.length(), replacement);
	}

	// A header or footer font. The painter scales from the page's logical units to the device, so the size is in those units.
	QFont makeFont(const wstring& name, int size, int style, int dpi)
	{
		QFont font(name.empty() ? QStringLiteral("Arial") : QString::fromStdWString(name));
		font.setPixelSize(std::max(1, (size ? size : 9) * dpi / 72));
		font.setBold((style & FONTSTYLE_BOLD) != 0);
		font.setItalic((style & FONTSTYLE_ITALIC) != 0);
		return font;
	}

	struct HeaderParts
	{
		wstring _left;
		wstring _middle;
		wstring _right;
	};

	// One line of text at the left, the middle and the right of the area, above the given baseline
	void drawParts(QPainter& painter, const QFont& font, const HeaderParts& parts, const wstring& pageVar, const wstring& pageString, const QRect& area, bool isRTL)
	{
		painter.setFont(font);
		painter.setPen(Qt::black);
		painter.setLayoutDirection(isRTL ? Qt::RightToLeft : Qt::LeftToRight);

		const auto text = [&](wstring part)
		{
			replaceStr(part, pageVar, pageString);
			return QString::fromStdWString(part);
		};

		QRect leftArea = area;
		leftArea.setLeft(area.left() + 5);
		painter.drawText(leftArea, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text(parts._left));
		painter.drawText(area, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine, text(parts._middle));
		painter.drawText(area, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, text(parts._right));
	}
}

void Printer::init(HWND hwnd, ScintillaEditView *pSEView, bool showDialog, size_t startPos, size_t endPos, bool isRTL)
{
	_hOwner = hwnd;
	_pSEView = pSEView;
	_showDialog = showDialog;
	_startPos = startPos;
	_endPos = endPos;
	_isRTL = isRTL;
}

size_t Printer::doPrint()
{
	const NppGUI & nppGUI = (NppParameters::getInstance()).getNppGUI();

	QPrinter printer(QPrinter::HighResolution);
	printer.setDocName(QString::fromStdWString(_pSEView->getCurrentBuffer()->getFullPathName()));

	bool isSelectionOnly = false;
	int fromPage = 0;
	int toPage = 0;
	if (_showDialog)
	{
		QPrintDialog dialog(&printer, _hOwner);
		dialog.setOption(QAbstractPrintDialog::PrintSelection, _startPos != _endPos);
		dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
		dialog.setOption(QAbstractPrintDialog::PrintCurrentPage, false);
		if (dialog.exec() != QDialog::Accepted)
			return 0;

		isSelectionOnly = printer.printRange() == QPrinter::Selection;
		if (printer.printRange() == QPrinter::PageRange)
		{
			fromPage = printer.fromPage();
			toPage = printer.toPage();
		}
	}

	QPainter output;
	if (!output.begin(&printer))
	{
		NativeLangSpeaker* pNativeSpeaker = NppParameters::getInstance().getNativeLangSpeaker();
		pNativeSpeaker->messageBox("PrintError",
			nullptr,
			L"Cannot start printer document.",
			L"",
			MB_OK);
		return 0;
	}

	// Scintilla opens its own painter on the device it draws to, and a printer takes only one for the whole job.
	// So each page's text is recorded to a picture and replayed on the printer, scaled up from the picture's resolution.
	QPicture measure;
	const int dpiX = measure.logicalDpiX();
	const int dpiY = measure.logicalDpiY();
	output.scale(static_cast<double>(printer.logicalDpiX()) / dpiX, static_cast<double>(printer.logicalDpiY()) / dpiY);

	// The painter's origin is the top left of the printable area
	const QRectF printableInches = printer.pageRect(QPrinter::Inch);
	const QRectF paperInches = printer.paperRect(QPrinter::Inch);
	const int pageWidth = static_cast<int>(printableInches.width() * dpiX);
	const int pageHeight = static_cast<int>(printableInches.height() * dpiY);
	const int physLeft = static_cast<int>((printableInches.left() - paperInches.left()) * dpiX);
	const int physTop = static_cast<int>((printableInches.top() - paperInches.top()) * dpiY);
	const int physRight = static_cast<int>((paperInches.right() - printableInches.right()) * dpiX);
	const int physBottom = static_cast<int>((paperInches.bottom() - printableInches.bottom()) * dpiY);

	// Space between the printable area and the text, from the user's margins in millimetres
	int marginLeft = 0;
	int marginTop = 0;
	int marginRight = 0;
	int marginBottom = 0;
	if (nppGUI._printSettings.isUserMargePresent())
	{
		const auto& marge = nppGUI._printSettings._marge;
		marginLeft = std::max(0, marge.left * dpiX * 100 / 2540 - physLeft);
		marginTop = std::max(0, marge.top * dpiY * 100 / 2540 - physTop);
		marginRight = std::max(0, marge.right * dpiX * 100 / 2540 - physRight);
		marginBottom = std::max(0, marge.bottom * dpiY * 100 / 2540 - physBottom);
	}

	const QFont fontHeader = makeFont(nppGUI._printSettings._headerFontName, nppGUI._printSettings._headerFontSize, nppGUI._printSettings._headerFontStyle, dpiY);
	const QFont fontFooter = makeFont(nppGUI._printSettings._footerFontName, nppGUI._printSettings._footerFontSize, nppGUI._printSettings._footerFontStyle, dpiY);
	const int headerLineHeight = QFontMetrics(fontHeader, &measure).lineSpacing();
	const int footerLineHeight = QFontMetrics(fontFooter, &measure).lineSpacing();
	const int printMarge = footerLineHeight + footerLineHeight / 2;

	// By default, we will print all the document
	size_t lengthPrinted = 0;
	size_t lengthDoc = _pSEView->getCurrentDocLen();
	const size_t lengthDocMax = lengthDoc;

	// In the case that the print dialog was launched and that there's a range of selection
	// We print the range of selection
	if (isSelectionOnly)
	{
		lengthPrinted = std::min(_startPos, _endPos);
		lengthDoc = std::min(std::max(_startPos, _endPos), lengthDocMax);
	}

	Sci_RangeToFormatFull frPrint{};
	frPrint.rc.left = marginLeft + printMarge;
	frPrint.rc.top = marginTop + printMarge;
	frPrint.rc.right = pageWidth - marginRight - printMarge;
	frPrint.rc.bottom = pageHeight - marginBottom - printMarge;
	frPrint.rcPage.left = 0;
	frPrint.rcPage.top = 0;
	frPrint.rcPage.right = pageWidth - 1;
	frPrint.rcPage.bottom = pageHeight - 1;
	frPrint.hdcTarget = static_cast<QPaintDevice*>(&measure);

	const QDateTime now = QDateTime::currentDateTime();
	const QLocale locale;
	const wstring shortDate = locale.toString(now.date(), QLocale::ShortFormat).toStdWString();
	const wstring longDate = locale.toString(now.date(), QLocale::LongFormat).toStdWString();
	const wstring timeNow = locale.toString(now.time(), QLocale::ShortFormat).toStdWString();

	const auto expand = [&](wstring part)
	{
		if (part.empty())
			return part;
		replaceStr(part, L"$(SHORT_DATE)", shortDate);
		replaceStr(part, L"$(LONG_DATE)", longDate);
		replaceStr(part, L"$(TIME)", timeNow);

		return expandNppEnvironmentStrs(part, _hOwner);
	};

	HeaderParts header;
	if (nppGUI._printSettings.isHeaderPresent())
	{
		frPrint.rc.top += headerLineHeight + headerLineHeight / 2;
		header = { expand(nppGUI._printSettings._headerLeft), expand(nppGUI._printSettings._headerMiddle), expand(nppGUI._printSettings._headerRight) };
	}

	HeaderParts footer;
	if (nppGUI._printSettings.isFooterPresent())
	{
		frPrint.rc.bottom -= footerLineHeight + footerLineHeight / 2;
		footer = { expand(nppGUI._printSettings._footerLeft), expand(nppGUI._printSettings._footerMiddle), expand(nppGUI._printSettings._footerRight) };
	}

	bool isShown = _pSEView->hasMarginShown(ScintillaEditView::_SC_MARGE_LINENUMBER);
	if (!nppGUI._printSettings._printLineNumber)
		_pSEView->showMargin(ScintillaEditView::_SC_MARGE_LINENUMBER, false);

	int pageNum = 1;
	bool isFirstPrinted = true;
	const wstring pageVar = L"$(CURRENT_PRINTING_PAGE)";

	_pSEView->execute(SCI_SETPRINTCOLOURMODE, nppGUI._printSettings._printOption); // setting mode once is enough
	while (lengthPrinted < lengthDoc)
	{
		const bool printPage = (fromPage == 0) || (pageNum >= fromPage && pageNum <= toPage);
		const wstring pageString = to_wstring(pageNum);

		if (printPage)
		{
			if (!isFirstPrinted)
				printer.newPage();
			isFirstPrinted = false;

			if (nppGUI._printSettings.isHeaderPresent())
			{
				const QRect area(frPrint.rc.left, frPrint.rc.top - headerLineHeight - headerLineHeight / 2, frPrint.rc.right - frPrint.rc.left, headerLineHeight);
				drawParts(output, fontHeader, header, pageVar, pageString, area, _isRTL);
				output.setPen(Qt::black);
				output.drawLine(frPrint.rc.left, frPrint.rc.top - headerLineHeight / 4, frPrint.rc.right, frPrint.rc.top - headerLineHeight / 4);
			}
		}

		frPrint.chrg.cpMin = static_cast<Sci_Position>(lengthPrinted);
		frPrint.chrg.cpMax = static_cast<Sci_Position>(lengthDoc);
		Sci_Position foundFormFeed = -1;
		if (nppGUI._printSettings._printFormFeedPageBreak)
		{
			// look for form feed: if found, stop current page just before the FF (though scintilla will continue through the EOL if one comes right before the FF)
			Sci_TextToFindFull ttf;
			ttf.chrg = frPrint.chrg;
			ttf.lpstrText = "\f";	// null terminated formfeed
			foundFormFeed = static_cast<Sci_Position>(_pSEView->execute(SCI_FINDTEXTFULL, static_cast<WPARAM>(SCFIND_NONE), reinterpret_cast<LPARAM>(&ttf)));
			// if FF is found, then stop the current page just before the FF by changing cpMax
			if (foundFormFeed > 0)
				frPrint.chrg.cpMax = foundFormFeed - 1;	// stop the current page just before the FF
			else if (foundFormFeed == 0)
				frPrint.chrg.cpMax = 0;	// but if FF was the first character, then just stop at that character (cannot stop before 0)
		}

		// actually send this page to Scintilla's print-page fomatter, whether or not FF is found
		QPicture pageContent;
		frPrint.hdc = static_cast<QPaintDevice*>(&pageContent);
		lengthPrinted = _pSEView->execute(SCI_FORMATRANGEFULL, printPage, reinterpret_cast<LPARAM>(&frPrint));
		if (printPage)
			output.drawPicture(0, 0, pageContent);

		// if FF was found, _after_ we've done the print-formatting for the current page, make sure that the start of the _next_ page is after the FF
		//		(without this increment by 1, it will just print the same FF inifintitely)
		if ((foundFormFeed != -1) && (static_cast<Sci_Position>(lengthPrinted) <= foundFormFeed))
			lengthPrinted = foundFormFeed + 1;

		if (printPage && nppGUI._printSettings.isFooterPresent())
		{
			const QRect area(frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 2, frPrint.rc.right - frPrint.rc.left, footerLineHeight);
			drawParts(output, fontFooter, footer, pageVar, pageString, area, _isRTL);
			output.setPen(Qt::black);
			output.drawLine(frPrint.rc.left, frPrint.rc.bottom + footerLineHeight / 4, frPrint.rc.right, frPrint.rc.bottom + footerLineHeight / 4);
		}

		++pageNum;

		if (fromPage != 0 && pageNum > toPage)
			break;
	}

	if (!nppGUI._printSettings._printLineNumber)
		_pSEView->showMargin(ScintillaEditView::_SC_MARGE_LINENUMBER, isShown);

	_pSEView->execute(SCI_FORMATRANGEFULL, FALSE, 0);
	output.end();

	return (static_cast<size_t>(pageNum) - 1);
}
