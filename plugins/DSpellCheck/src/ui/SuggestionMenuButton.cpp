// This file is part of DSpellCheck Plug-in for Notepad++
// Copyright (C)2019 Sergey Semushin <Predelnik@gmail.com>
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Qt port: written with Qt, see SuggestionMenuButton.h. The handling of each message is upstream's.

#include "SuggestionMenuButton.h"

#include "ContextMenuHandler.h"
#include "MenuItem.h"
#include "common/WindowsDefs.h"
#include "npp/NppInterface.h"
#include "plugin/Plugin.h"
#include "plugin/resource.h"
#include "plugin/Settings.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>

namespace {
class ButtonWidget : public QWidget {
public:
  ButtonWidget(SuggestionMenuButton &button) : m_button(button) {
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::ArrowCursor);
  }

protected:
  void enterEvent(QEnterEvent *) override { m_button.on_mouse_enter(); }
  void leaveEvent(QEvent *) override { m_button.on_mouse_leave(); }
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton)
      m_button.on_mouse_press();
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton)
      m_button.on_mouse_release();
  }
  void wheelEvent(QWheelEvent *event) override { m_button.on_wheel(event); }
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    m_button.paint(painter);
  }

private:
  SuggestionMenuButton &m_button;
};
} // namespace

void SuggestionMenuButton::do_dialog() {
  create(IDD_SUGGESTIONS);
}

// The widget, hidden until there is a word to show it at, as WM_INITDIALOG left it
void SuggestionMenuButton::build_form(QWidget *) {}

bool SuggestionMenuButton::is_pressed() const {
  return m_state_pressed;
}

SuggestionMenuButton::SuggestionMenuButton(HINSTANCE h_inst, HWND parent,
                                     NppInterface &npp, ContextMenuHandler &context_menu_handler,
                                     const Settings &settings)
  : m_npp(npp),
    m_settings(settings), m_context_menu_handler(context_menu_handler) {
  StaticDialog::init(h_inst, parent);
  m_state_pressed = false;
  m_state_hovered = false;
  m_state_menu = false;
}

void SuggestionMenuButton::move_to(HWND editor, int x, int y, int size) {
  auto *scroll_area = qobject_cast<QAbstractScrollArea *>(editor);
  QWidget *text_area = scroll_area != nullptr ? scroll_area->viewport() : editor;
  if (!_hSelf) {
    auto *widget = new ButtonWidget(*this);
    widget->setProperty("nppId", IDD_SUGGESTIONS);
    _hSelf = widget;
    set_transparency();
  }
  if (_hSelf->parentWidget() != text_area)
    _hSelf->setParent(text_area);
  _hSelf->setGeometry(x, y, size, size);
}

void SuggestionMenuButton::show_suggestion_menu() {
  // As WM_SHOWANDRECREATEMENU
  ACTIVE_VIEW_BLOCK(m_npp);
  m_state_menu = true;
  _hSelf->update();
  QMenu popup_menu(_hSelf);
  MenuItem::append_to_menu(&popup_menu, m_context_menu_handler.get_suggestion_menu_items());
  const QPoint below = _hSelf->mapToGlobal(QPoint(_hSelf->width(), _hSelf->height()));
  popup_menu.exec(QPoint(below.x() - popup_menu.sizeHint().width(), below.y())); // TPM_RIGHTALIGN
  m_npp.get_view_hwnd()->setFocus();
  m_state_menu = false;
  m_state_pressed = false;
  if (_hSelf)
    _hSelf->update();
}

void SuggestionMenuButton::on_settings_changed() {
  display(false);
  set_transparency();
}

void SuggestionMenuButton::set_transparency() {
  if (!_hSelf)
    return;
  auto *effect = qobject_cast<QGraphicsOpacityEffect *>(_hSelf->graphicsEffect());
  if (effect == nullptr) {
    effect = new QGraphicsOpacityEffect(_hSelf);
    _hSelf->setGraphicsEffect(effect);
  }
  effect->setOpacity(m_settings.data.suggestion_button_opacity / 100.0);
}

void SuggestionMenuButton::on_mouse_enter() {
  m_state_hovered = true;
  _hSelf->update();
}

void SuggestionMenuButton::on_mouse_leave() {
  m_state_hovered = false;
  m_state_pressed = false;
  _hSelf->update();
}

void SuggestionMenuButton::on_mouse_press() {
  m_state_pressed = true;
  _hSelf->update();
}

void SuggestionMenuButton::on_mouse_release() {
  if (!m_state_pressed)
    return;
  show_suggestion_menu();
}

void SuggestionMenuButton::on_wheel(QWheelEvent *event) {
  _hSelf->hide();
  // The wheel scrolls the editor, which is where the button is
  if (QWidget *text_area = _hSelf->parentWidget()) {
    QWheelEvent forwarded(text_area->mapFromGlobal(event->globalPosition()), event->globalPosition(), event->pixelDelta(), event->angleDelta(),
                          event->buttons(), event->modifiers(), event->phase(), event->inverted());
    QApplication::sendEvent(text_area, &forwarded);
  }
}

void SuggestionMenuButton::paint(QPainter &painter) {
  // As WM_PAINT: the arrow of the state, stretched over the button
  QString image = QStringLiteral(":/DSpellCheck/downArrowButton.bmp");
  if (m_state_pressed || m_state_menu)
    image = QStringLiteral(":/DSpellCheck/downArrowButton_push.bmp");
  else if (m_state_hovered)
    image = QStringLiteral(":/DSpellCheck/downArrowButton_hover.bmp");
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.drawPixmap(_hSelf->rect(), QPixmap(image));
}
