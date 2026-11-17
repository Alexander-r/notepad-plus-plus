/*
This file is part of NppExec
Copyright (C) 2023 Anders Kjersem.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Qt port: a QPushButton that paints its colour, with the colour as its tooltip, and QColorDialog in place of
// ChooseColor, offering the same custom colours

#include "PickColorBtn.h"
#include "NppExec.h"

#include <QColorDialog>
#include <QPainter>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QStylePainter>

namespace
{
    class ColorButton : public QPushButton
    {
    public:
        explicit ColorButton(QWidget* parent) : QPushButton(parent)
        {
            setAutoDefault(false);
            setProperty("nppColorButton", true);
            const int h = fontMetrics().height() + 6;
            setFixedSize(h * 2, h);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QStylePainter painter(this);
            QStyleOptionButton option;
            initStyleOption(&option);
            option.text.clear();
            painter.drawControl(QStyle::CE_PushButtonBevel, option);
            const QRect area = rect().adjusted(5, 4, -5, -4);
            if (isEnabled())
            {
                const COLORREF c = static_cast<COLORREF>(property("nppColor").toUInt());
                painter.fillRect(area, QColor(GetRValue(c), GetGValue(c), GetBValue(c)));
                painter.setPen(palette().color(QPalette::WindowText));
                painter.drawRect(area.adjusted(0, 0, -1, -1));
            }
            else
            {
                painter.fillRect(area, QBrush(palette().color(QPalette::Disabled, QPalette::WindowText), Qt::DiagCrossPattern));
            }
        }
    };

    unsigned int PickColorBtn_addNewColor(COLORREF colors[], unsigned int nColors, const COLORREF color)
    {
        unsigned int n = 0;
        for ( ; n < nColors; ++n )
        {
            if ( colors[n] == color )
                break; // this color is already present in the colors
        }
        if ( n == nColors )
        {
            // this color is not present in the colors
            colors[nColors] = color;
            ++nColors;
        }
        return nColors;
    }
}

QWidget* nppExecCreateColorButton(QWidget* parent)
{
    return new ColorButton(parent);
}

COLORREF PickColorBtn_GetColor(HWND h)
{
    return h ? static_cast<COLORREF>(h->property("nppColor").toUInt()) : 0;
}

void PickColorBtn_SetColor(HWND h, COLORREF c)
{
    if ( !h )
        return;
    nppExecOnGuiThread( [h, c]() {
        h->setProperty("nppColor", static_cast<uint>(c));
        h->setToolTip(QString::asprintf("#%.2X%.2X%.2X", GetRValue(c), GetGValue(c), GetBValue(c)));
        h->update();
    } );
}

INT_PTR CALLBACK PickColorBtn_HandleMessage(HWND /*hDlg*/, UINT Msg, WPARAM WPar, LPARAM LPar, LRESULT *RetVal)
{
    if ( Msg == WM_COMMAND && LPar && HIWORD(WPar) == BN_CLICKED )
    {
        HWND hCtl = reinterpret_cast<HWND>(LPar);
        if ( hCtl->property("nppColorButton").toBool() )
        {
            COLORREF orgclr = PickColorBtn_GetColor(hCtl) & 0xffffff;
            COLORREF colors[16];
            for (unsigned int i = 0; i < 16; ++i) colors[i] = RGB(i * 16, i * 16, i * 16);
            colors[0] = orgclr;
            colors[1] = COLOR_CON_TEXTERR;               // error
            colors[2] = COLOR_CON_TEXTMSG;               // message
            colors[3] = COLOR_CON_TEXTINF1;              // info 1
            colors[4] = COLOR_CON_TEXTINF2;              // info 2
            colors[5] = ::GetSysColor(COLOR_WINDOWTEXT); // normal text
            colors[6] = ::GetSysColor(COLOR_WINDOW);     // background
            unsigned int nColors = 7;
            const auto& nppExecConsole = Runtime::GetNppExec().GetConsole();
            nColors = PickColorBtn_addNewColor(colors, nColors, nppExecConsole.GetCurrentColorTextErr());
            nColors = PickColorBtn_addNewColor(colors, nColors, nppExecConsole.GetCurrentColorTextMsg());
            nColors = PickColorBtn_addNewColor(colors, nColors, nppExecConsole.GetCurrentColorTextNorm());
            COLORREF bkColor = nppExecConsole.GetCurrentColorBkgnd();
            if ( bkColor == COLOR_CON_BKGND )
            {
                bkColor = ::GetSysColor(COLOR_WINDOW);
            }
            nColors = PickColorBtn_addNewColor(colors, nColors, bkColor);

            for ( unsigned int i = 0; i < 16 && static_cast<int>(i) < QColorDialog::customCount(); ++i )
            {
                QColorDialog::setCustomColor(static_cast<int>(i), QColor(GetRValue(colors[i]), GetGValue(colors[i]), GetBValue(colors[i])));
            }
            const QColor chosen = QColorDialog::getColor(QColor(GetRValue(orgclr), GetGValue(orgclr), GetBValue(orgclr)), hCtl->window());
            if ( chosen.isValid() )
            {
                PickColorBtn_SetColor(hCtl, RGB(chosen.red(), chosen.green(), chosen.blue()));
            }
            if ( RetVal )
                *RetVal = 0;
            return TRUE;
        }
    }
    return FALSE;
}

INT_PTR CALLBACK PickColorBtn_HandleMessageForDialog(HWND hDlg, UINT Msg, WPARAM WPar, LPARAM LPar)
{
    LRESULT r = 0;
    return PickColorBtn_HandleMessage(hDlg, Msg, WPar, LPar, &r);
}

HWND PickColorBtn_InitializeTooltips(HWND /*hDlg*/, UINT /*first*/, UINT /*last*/)
{
    // Qt port: each button's tooltip is its colour, set with the colour
    return NULL;
}

void PickColorBtn_HandleTooltipsNotify(HWND, WPARAM, LPARAM)
{
}
