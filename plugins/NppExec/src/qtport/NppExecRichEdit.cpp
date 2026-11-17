// Qt port: see NppExecRichEdit.h

#include "NppExecRichEdit.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>

NppExecRichEditWidget::NppExecRichEditWidget(QWidget* parent) : QPlainTextEdit(parent)
{
	// As RichEdit, which has no menu of its own
	setContextMenuPolicy(Qt::NoContextMenu);
}

NppExecRichEditWidget::Filtered NppExecRichEditWidget::filter(UINT message, WPARAM& wParam, LPARAM lParam)
{
	if (!msgFilter)
		return Filtered::unchanged;
	UINT msg = message;
	const WPARAM original = wParam;
	LPARAM l = lParam;
	if (msgFilter(msg, wParam, l) != 0 || msg == 0 || wParam == 0)
		return Filtered::blocked;
	return (wParam != original) ? Filtered::changed : Filtered::unchanged;
}

void NppExecRichEditWidget::keyPressEvent(QKeyEvent* event)
{
	// Windows sends WM_KEYDOWN and then WM_CHAR, each of which the handler may block or change before RichEdit sees it
	const bool alt = (event->modifiers() & Qt::AltModifier) != 0;
	WPARAM vk = nppExecVirtualKey(event->key());
	if (vk != 0)
	{
		switch (filter(alt ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, 1))
		{
			case Filtered::blocked:
				return;
			case Filtered::changed:
			{
				// Another key, as Ctrl+C for Ctrl+X over the output, which RichEdit handles in its place
				const int key = (vk >= 'A' && vk <= 'Z') ? Qt::Key_A + static_cast<int>(vk - 'A')
					: (vk >= '0' && vk <= '9') ? Qt::Key_0 + static_cast<int>(vk - '0') : 0;
				if (key != 0)
				{
					QKeyEvent changed(QEvent::KeyPress, key, event->modifiers());
					QPlainTextEdit::keyPressEvent(&changed);
				}
				return;
			}
			case Filtered::unchanged:
				break;
		}
	}
	const QString text = event->text();
	for (const QChar ch : text)
	{
		WPARAM code = ch.unicode();
		switch (filter(alt ? WM_SYSCHAR : WM_CHAR, code, 1))
		{
			case Filtered::blocked:
				return;
			case Filtered::changed:
				textCursor().insertText(QString(QChar(static_cast<char16_t>(code))));
				return;
			case Filtered::unchanged:
				break;
		}
	}
	QPlainTextEdit::keyPressEvent(event);
}

void NppExecRichEditWidget::keyReleaseEvent(QKeyEvent* event)
{
	const bool alt = (event->modifiers() & Qt::AltModifier) != 0;
	WPARAM vk = nppExecVirtualKey(event->key());
	if (vk != 0 && filter(alt ? WM_SYSKEYUP : WM_KEYUP, vk, static_cast<LPARAM>(0xC0000001)) != Filtered::unchanged)
		return;
	QPlainTextEdit::keyReleaseEvent(event);
}

namespace
{
	WPARAM mouseKeys(const QMouseEvent* event)
	{
		WPARAM keys = 0;
		if (event->buttons() & Qt::LeftButton)
			keys |= MK_LBUTTON;
		if (event->modifiers() & Qt::ShiftModifier)
			keys |= MK_SHIFT;
		if (event->modifiers() & Qt::ControlModifier)
			keys |= MK_CONTROL;
		return keys;
	}

	LPARAM mousePoint(const QMouseEvent* event)
	{
		const QPoint pos = event->position().toPoint();
		return MAKELPARAM(static_cast<WORD>(pos.x()), static_cast<WORD>(pos.y()));
	}
}

void NppExecRichEditWidget::mousePressEvent(QMouseEvent* event)
{
	WPARAM keys = mouseKeys(event);
	if (event->button() == Qt::LeftButton && filter(WM_LBUTTONDOWN, keys, mousePoint(event)) == Filtered::blocked)
		return;
	QPlainTextEdit::mousePressEvent(event);
}

void NppExecRichEditWidget::mouseReleaseEvent(QMouseEvent* event)
{
	WPARAM keys = mouseKeys(event);
	if (event->button() == Qt::LeftButton && filter(WM_LBUTTONUP, keys, mousePoint(event)) == Filtered::blocked)
		return;
	QPlainTextEdit::mouseReleaseEvent(event);
}

void NppExecRichEditWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	WPARAM keys = mouseKeys(event);
	if (event->button() == Qt::LeftButton && filter(WM_LBUTTONDBLCLK, keys, mousePoint(event)) == Filtered::blocked)
		return;
	QPlainTextEdit::mouseDoubleClickEvent(event);
}

bool NppExecRichEditWidget::canInsertFromMimeData(const QMimeData* source) const
{
	return source->hasText();
}

void NppExecRichEditWidget::insertFromMimeData(const QMimeData* source)
{
	// Plain text only, in the format of the insertion point, as RichEdit pastes into NppExec's console. A paste of
	// the selection with the middle button, which Windows has not, goes to the end of the input as typing does.
	if (!source->hasText())
		return;
	WPARAM none = 1;
	if (filter(WM_PASTE, none, 0) == Filtered::blocked)
		return;
	textCursor().insertText(source->text());
}

UINT nppExecVirtualKey(int qtKey)
{
	if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
		return static_cast<UINT>('A' + (qtKey - Qt::Key_A));
	if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
		return static_cast<UINT>('0' + (qtKey - Qt::Key_0));
	if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
		return static_cast<UINT>(VK_F1 + (qtKey - Qt::Key_F1));
	switch (qtKey)
	{
		case Qt::Key_Backspace: return VK_BACK;
		case Qt::Key_Tab: return VK_TAB;
		case Qt::Key_Backtab: return VK_TAB;
		case Qt::Key_Return: return VK_RETURN;
		case Qt::Key_Enter: return VK_RETURN;
		case Qt::Key_Shift: return VK_SHIFT;
		case Qt::Key_Control: return VK_CONTROL;
		case Qt::Key_Alt: return VK_MENU;
		case Qt::Key_CapsLock: return VK_CAPITAL;
		case Qt::Key_Escape: return VK_ESCAPE;
		case Qt::Key_Space: return VK_SPACE;
		case Qt::Key_PageUp: return VK_PRIOR;
		case Qt::Key_PageDown: return VK_NEXT;
		case Qt::Key_End: return VK_END;
		case Qt::Key_Home: return VK_HOME;
		case Qt::Key_Left: return VK_LEFT;
		case Qt::Key_Up: return VK_UP;
		case Qt::Key_Right: return VK_RIGHT;
		case Qt::Key_Down: return VK_DOWN;
		case Qt::Key_Insert: return VK_INSERT;
		case Qt::Key_Delete: return VK_DELETE;
		default: return 0;
	}
}

SHORT GetKeyState(int virtualKey)
{
	const Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
	bool down = false;
	switch (virtualKey)
	{
		case VK_SHIFT: down = (modifiers & Qt::ShiftModifier) != 0; break;
		case VK_CONTROL: down = (modifiers & Qt::ControlModifier) != 0; break;
		case VK_MENU: down = (modifiers & Qt::AltModifier) != 0; break;
		case VK_LBUTTON: down = (QGuiApplication::mouseButtons() & Qt::LeftButton) != 0; break;
		case VK_RBUTTON: down = (QGuiApplication::mouseButtons() & Qt::RightButton) != 0; break;
		default: break;
	}
	// As Windows gives it: negative, with the high bit and the bits below it set, which code tests in either way
	return down ? static_cast<SHORT>(0xFF80) : 0;
}
