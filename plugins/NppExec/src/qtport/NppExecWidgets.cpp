// Qt port: see NppExecWidgets.h

#include "NppExecWidgets.h"
#include "NppExecRichEdit.h"
#include "../resource.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFontInfo>
#include <QBrush>
#include <QIcon>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <map>
#include <mutex>

BOOL IsWindow(HWND window)
{
	if (!window)
		return FALSE;
	BOOL alive = FALSE;
	nppExecOnGuiThread([&]() { alive = QApplication::allWidgets().contains(window) ? TRUE : FALSE; });
	return alive;
}

BOOL IsWindowVisible(HWND window)
{
	BOOL visible = FALSE;
	if (IsWindow(window))
		nppExecOnGuiThread([&]() { visible = window->isVisible() ? TRUE : FALSE; });
	return visible;
}

BOOL IsWindowEnabled(HWND window)
{
	BOOL enabled = FALSE;
	if (IsWindow(window))
		nppExecOnGuiThread([&]() { enabled = window->isEnabled() ? TRUE : FALSE; });
	return enabled;
}

HWND SetFocus(HWND window)
{
	HWND previous = nullptr;
	nppExecOnGuiThread([&]() {
		previous = QApplication::focusWidget();
		if (window)
		{
			window->activateWindow();
			window->setFocus(Qt::OtherFocusReason);
		}
	});
	return previous;
}

HWND GetParent(HWND window)
{
	return window ? window->parentWidget() : nullptr;
}

BOOL EnableWindow(HWND window, BOOL enable)
{
	if (!window)
		return FALSE;
	BOOL wasDisabled = FALSE;
	nppExecOnGuiThread([&]() {
		wasDisabled = window->isEnabled() ? FALSE : TRUE;
		window->setEnabled(enable != FALSE);
	});
	return wasDisabled;
}

BOOL ShowWindow(HWND window, int command)
{
	if (!window)
		return FALSE;
	BOOL wasVisible = FALSE;
	nppExecOnGuiThread([&]() {
		wasVisible = window->isVisible() ? TRUE : FALSE;
		window->setVisible(command != SW_HIDE);
	});
	return wasVisible;
}

BOOL SetWindowText(HWND window, const wchar_t* text)
{
	if (!window)
		return FALSE;
	const QString value = QString::fromWCharArray(text ? text : L"");
	nppExecOnGuiThread([&]() { nppExecSetWidgetText(window, value); });
	return TRUE;
}

int GetWindowText(HWND window, wchar_t* text, int size)
{
	if (!text || size <= 0)
		return 0;
	QString value;
	nppExecOnGuiThread([&]() { value = nppExecWidgetText(window); });
	const std::wstring s = value.toStdWString();
	const int n = static_cast<int>(std::min<std::size_t>(s.size(), static_cast<std::size_t>(size - 1)));
	std::wmemcpy(text, s.data(), n);
	text[n] = L'\0';
	return n;
}

int GetWindowTextLength(HWND window)
{
	QString value;
	nppExecOnGuiThread([&]() { value = nppExecWidgetText(window); });
	return static_cast<int>(value.toStdWString().size());
}

namespace
{
	std::mutex g_timersLock;
	std::map<UINT_PTR, QPointer<QTimer>> g_timers;
	UINT_PTR g_nextTimerId = 1;
}

UINT_PTR SetTimer(HWND window, UINT_PTR id, UINT milliseconds, TIMERPROC function)
{
	UINT_PTR timerId = 0;
	nppExecOnGuiThread([&]() {
		{
			std::lock_guard<std::mutex> lock(g_timersLock);
			timerId = (window && id) ? id : g_nextTimerId++;
			auto it = g_timers.find(timerId);
			if (it != g_timers.end() && it->second)
				delete it->second;
		}
		auto* timer = new QTimer(qApp);
		timer->setInterval(static_cast<int>(milliseconds));
		QObject::connect(timer, &QTimer::timeout, timer, [window, timerId, function]() {
			if (function)
				function(window, WM_TIMER, timerId, GetTickCount());
		});
		timer->start();
		std::lock_guard<std::mutex> lock(g_timersLock);
		g_timers[timerId] = timer;
	});
	return timerId;
}

BOOL KillTimer(HWND /*window*/, UINT_PTR id)
{
	QPointer<QTimer> timer;
	{
		std::lock_guard<std::mutex> lock(g_timersLock);
		const auto it = g_timers.find(id);
		if (it == g_timers.end())
			return FALSE;
		timer = it->second;
		g_timers.erase(it);
	}
	nppExecOnGuiThread([&]() {
		if (timer)
		{
			timer->stop();
			timer->deleteLater();
		}
	});
	return TRUE;
}

namespace
{
	QAction* menuAction(QMenu* menu, UINT item, UINT flags)
	{
		if (!menu)
			return nullptr;
		if (flags & MF_BYPOSITION)
			return menu->actions().value(static_cast<int>(item));
		for (QAction* action : menu->actions())
		{
			if (action->data().toInt() == static_cast<int>(item) && !action->isSeparator())
				return action;
			if (QAction* found = menuAction(action->menu(), item, flags))
				return found;
		}
		return nullptr;
	}
}

QAction* nppExecMenuAction(HMENU menu, UINT id)
{
	QAction* action = nullptr;
	nppExecOnGuiThread([&]() { action = menuAction(menu, id, MF_BYCOMMAND); });
	return action;
}

QMenu* nppExecMenuOfCommand(HMENU menu, UINT id)
{
	if (!menu)
		return nullptr;
	for (QAction* action : menu->actions())
	{
		if (!action->isSeparator() && action->data().toInt() == static_cast<int>(id))
			return menu;
		if (QMenu* found = nppExecMenuOfCommand(action->menu(), id))
			return found;
	}
	return nullptr;
}

DWORD CheckMenuItem(HMENU menu, UINT item, UINT flags)
{
	DWORD previous = static_cast<DWORD>(-1);
	nppExecOnGuiThread([&]() {
		if (QAction* action = menuAction(menu, item, flags))
		{
			previous = action->isChecked() ? MF_CHECKED : MF_UNCHECKED;
			// Any item can get a check mark in a Win32 menu
			action->setCheckable(true);
			action->setChecked((flags & MF_CHECKED) != 0);
		}
	});
	return previous;
}

BOOL EnableMenuItem(HMENU menu, UINT item, UINT flags)
{
	BOOL previous = -1;
	nppExecOnGuiThread([&]() {
		if (QAction* action = menuAction(menu, item, flags))
		{
			previous = action->isEnabled() ? MF_ENABLED : MF_GRAYED;
			action->setEnabled((flags & (MF_GRAYED | MF_DISABLED)) == 0);
		}
	});
	return previous;
}

INT_PTR nppExecShellExecute(const wchar_t* file, const wchar_t* parameters)
{
	const QString name = QString::fromWCharArray(file ? file : L"");
	if (name.isEmpty())
		return SE_ERR_FNF;

	// A URL, such as "https://...", opens in the desktop's application for it
	const QUrl url(name);
	if (url.isValid() && url.scheme().size() > 1 && !QFileInfo::exists(name))
		return QDesktopServices::openUrl(url) ? 33 : SE_ERR_NOASSOC;

	// An executable, given by its path or found in PATH, starts with the parameters
	QString program;
	if (name.contains(u'/'))
	{
		const QFileInfo info(name);
		if (!info.exists())
			return SE_ERR_FNF;
		if (info.isFile() && info.isExecutable())
			program = info.absoluteFilePath();
	}
	else
	{
		wchar_t found[4096];
		if (SearchPath(nullptr, file, nullptr, 4096, found, nullptr) > 0 && QFileInfo(QString::fromWCharArray(found)).isExecutable())
			program = QString::fromWCharArray(found);
		else if (!QFileInfo::exists(name))
			return SE_ERR_FNF;
	}
	if (!program.isEmpty())
	{
		const QStringList arguments = QProcess::splitCommand(QString::fromWCharArray(parameters ? parameters : L""));
		return QProcess::startDetached(program, arguments, QDir::currentPath()) ? 33 : SE_ERR_ACCESSDENIED;
	}

	// A document or folder opens with the desktop's application for it
	return QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(name).absoluteFilePath())) ? 33 : SE_ERR_NOASSOC;
}

COLORREF GetSysColor(int index)
{
	QColor color;
	nppExecOnGuiThread([&]() {
		const QPalette palette = QApplication::palette();
		switch (index)
		{
			case COLOR_WINDOW: color = palette.color(QPalette::Base); break;
			case COLOR_WINDOWTEXT: color = palette.color(QPalette::Text); break;
			case COLOR_3DFACE: color = palette.color(QPalette::Button); break;
			case COLOR_GRAYTEXT: color = palette.color(QPalette::Disabled, QPalette::Text); break;
			case COLOR_HIGHLIGHT: color = palette.color(QPalette::Highlight); break;
			case COLOR_HIGHLIGHTTEXT: color = palette.color(QPalette::HighlightedText); break;
			default: color = palette.color(QPalette::Window); break;
		}
	});
	return RGB(color.red(), color.green(), color.blue());
}

namespace
{
	LRESULT defaultWindowProc(HWND, UINT, WPARAM, LPARAM);

	// The subclass of a widget: see SetWindowLongPtr in NppExecWidgets.h
	class Subclass : public QObject
	{
	public:
		Subclass(QWidget* window, WNDPROC proc) : QObject(window), m_window(window), m_proc(proc)
		{
			window->setProperty("nppSubclass", QVariant::fromValue(reinterpret_cast<quintptr>(this)));
			window->installEventFilter(this);
			if (auto* area = qobject_cast<QAbstractScrollArea*>(window))
				area->viewport()->installEventFilter(this);
		}

		static Subclass* of(QWidget* window)
		{
			return window ? reinterpret_cast<Subclass*>(window->property("nppSubclass").value<quintptr>()) : nullptr;
		}

		WNDPROC m_proc = nullptr;

		// Hands the event being filtered to its widget, once
		void deliver()
		{
			if (!m_current || m_delivered)
				return;
			m_delivered = true;
			m_bypass = true;
			QCoreApplication::sendEvent(m_target, m_current);
			m_bypass = false;
		}

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (m_bypass || !m_proc)
				return false;
			switch (event->type())
			{
				case QEvent::KeyPress:
				case QEvent::KeyRelease:
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
				case QEvent::FocusIn:
				case QEvent::FocusOut:
					break;
				default:
					return false;
			}
			if (watched != m_window && event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease &&
				event->type() != QEvent::MouseButtonDblClick)
				return false;

			QEvent* previous = m_current;
			QObject* previousTarget = m_target;
			const bool previousDelivered = m_delivered;
			m_current = event;
			m_target = watched;
			m_delivered = false;
			dispatch(event);
			m_current = previous;
			m_target = previousTarget;
			m_delivered = previousDelivered;
			return true;
		}

	private:
		void dispatch(QEvent* event)
		{
			switch (event->type())
			{
				case QEvent::KeyPress:
				{
					auto* key = static_cast<QKeyEvent*>(event);
					const bool alt = (key->modifiers() & Qt::AltModifier) != 0;
					const UINT vk = nppExecVirtualKey(key->key());
					if (vk != 0)
						m_proc(m_window, alt ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, 1);
					for (const QChar ch : key->text())
						m_proc(m_window, alt ? WM_SYSCHAR : WM_CHAR, ch.unicode(), 1);
					if (vk == 0 && key->text().isEmpty())
						deliver(); // a key of no message, such as a modifier alone
					break;
				}
				case QEvent::KeyRelease:
				{
					auto* key = static_cast<QKeyEvent*>(event);
					const UINT vk = nppExecVirtualKey(key->key());
					if (vk != 0)
						m_proc(m_window, (key->modifiers() & Qt::AltModifier) ? WM_SYSKEYUP : WM_KEYUP, vk, static_cast<LPARAM>(0xC0000001));
					else
						deliver();
					break;
				}
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
				{
					auto* mouse = static_cast<QMouseEvent*>(event);
					if (mouse->button() != Qt::LeftButton)
					{
						deliver();
						break;
					}
					const QPoint pos = mouse->position().toPoint();
					const UINT message = (event->type() == QEvent::MouseButtonPress) ? WM_LBUTTONDOWN
						: (event->type() == QEvent::MouseButtonRelease) ? WM_LBUTTONUP : WM_LBUTTONDBLCLK;
					m_proc(m_window, message, MK_LBUTTON, MAKELPARAM(static_cast<WORD>(pos.x()), static_cast<WORD>(pos.y())));
					break;
				}
				case QEvent::FocusIn:
					m_proc(m_window, WM_SETFOCUS, 0, 0);
					break;
				case QEvent::FocusOut:
					m_proc(m_window, WM_KILLFOCUS, 0, 0);
					break;
				default:
					break;
			}
		}

		QWidget* m_window = nullptr;
		QEvent* m_current = nullptr;
		QObject* m_target = nullptr;
		bool m_delivered = false;
		bool m_bypass = false;
	};

	LRESULT defaultWindowProc(HWND window, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
	{
		if (Subclass* subclass = Subclass::of(window))
			subclass->deliver();
		return 0;
	}
}

LONG_PTR SetWindowLongPtr(HWND window, int index, LONG_PTR value)
{
	if (!window)
		return 0;
	LONG_PTR previous = 0;
	nppExecOnGuiThread([&]() {
		switch (index)
		{
			case GWLP_WNDPROC:
			{
				Subclass* subclass = Subclass::of(window);
				if (!subclass)
					subclass = new Subclass(window, nullptr);
				previous = reinterpret_cast<LONG_PTR>(subclass->m_proc ? subclass->m_proc : &defaultWindowProc);
				const auto proc = reinterpret_cast<WNDPROC>(value);
				// The original procedure put back ends the subclass
				subclass->m_proc = (proc == &defaultWindowProc) ? nullptr : proc;
				break;
			}
			case GWLP_USERDATA:
				previous = static_cast<LONG_PTR>(window->property("nppUserData").toLongLong());
				window->setProperty("nppUserData", static_cast<qlonglong>(value));
				break;
			case DWLP_MSGRESULT:
				previous = static_cast<LONG_PTR>(window->property("nppMsgResult").toLongLong());
				window->setProperty("nppMsgResult", static_cast<qlonglong>(value));
				break;
			default:
				break;
		}
	});
	return previous;
}

LONG_PTR GetWindowLongPtr(HWND window, int index)
{
	if (!window)
		return 0;
	switch (index)
	{
		case GWLP_ID:
			return window->property("nppId").toInt();
		case GWLP_USERDATA:
			return static_cast<LONG_PTR>(window->property("nppUserData").toLongLong());
		case DWLP_MSGRESULT:
			return static_cast<LONG_PTR>(window->property("nppMsgResult").toLongLong());
		case GWLP_WNDPROC:
		{
			const Subclass* subclass = Subclass::of(window);
			return reinterpret_cast<LONG_PTR>((subclass && subclass->m_proc) ? subclass->m_proc : &defaultWindowProc);
		}
		default:
			return 0;
	}
}

LRESULT CallWindowProc(WNDPROC proc, HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	return proc ? proc(window, message, wParam, lParam) : 0;
}

HANDLE GetProp(HWND window, const wchar_t* name)
{
	if (!window)
		return nullptr;
	return reinterpret_cast<HANDLE>(window->property(QString::fromWCharArray(name).toUtf8().constData()).value<quintptr>());
}

BOOL SetProp(HWND window, const wchar_t* name, HANDLE value)
{
	if (!window)
		return FALSE;
	window->setProperty(QString::fromWCharArray(name).toUtf8().constData(), QVariant::fromValue(reinterpret_cast<quintptr>(value)));
	return TRUE;
}

HANDLE RemoveProp(HWND window, const wchar_t* name)
{
	HANDLE value = GetProp(window, name);
	if (window)
		window->setProperty(QString::fromWCharArray(name).toUtf8().constData(), QVariant());
	return value;
}

BOOL IsChild(HWND parent, HWND window)
{
	return (parent && window && parent != window && parent->isAncestorOf(window)) ? TRUE : FALSE;
}

HBITMAP nppExecLoadBitmap(int id)
{
	if (id == IDI_CONSOLEBITMAP)
		return new QPixmap(QStringLiteral(":/NppExec/ConsoleIcon.bmp"));
	return nullptr;
}

HICON nppExecLoadIcon(int id)
{
	if (id == IDI_CONSOLEICON)
		return new QIcon(QStringLiteral(":/NppExec/ConsoleIcon_light.ico"));
	if (id == IDI_CONSOLEICONDARK)
		return new QIcon(QStringLiteral(":/NppExec/ConsoleIcon_dark.ico"));
	return nullptr;
}

namespace
{
	QFont fontOf(const LOGFONT& lf)
	{
		QFont font;
		font.setFamily(QString::fromWCharArray(lf.lfFaceName));
		const int pixels = (lf.lfHeight < 0) ? -lf.lfHeight : lf.lfHeight;
		if (pixels > 0)
			font.setPixelSize(pixels);
		font.setBold(lf.lfWeight >= FW_BOLD);
		font.setItalic(lf.lfItalic != 0);
		font.setUnderline(lf.lfUnderline != 0);
		font.setStrikeOut(lf.lfStrikeOut != 0);
		return font;
	}

	void toLogFont(const QFont& font, LOGFONT* lf)
	{
		std::memset(lf, 0, sizeof(LOGFONT));
		lf->lfHeight = -QFontInfo(font).pixelSize();
		lf->lfWeight = font.bold() ? FW_BOLD : FW_NORMAL;
		lf->lfItalic = font.italic() ? 1 : 0;
		lf->lfUnderline = font.underline() ? 1 : 0;
		lf->lfStrikeOut = font.strikeOut() ? 1 : 0;
		lf->lfCharSet = DEFAULT_CHARSET;
		const std::wstring face = font.family().toStdWString();
		std::wcsncpy(lf->lfFaceName, face.c_str(), LF_FACESIZE - 1);
	}
}

HFONT CreateFontIndirect(const LOGFONT* logFont)
{
	return logFont ? new QFont(fontOf(*logFont)) : nullptr;
}

int GetObject(HFONT font, int size, void* logFont)
{
	if (!font || !logFont || size < static_cast<int>(sizeof(LOGFONT)))
		return 0;
	toLogFont(*font, static_cast<LOGFONT*>(logFont));
	return sizeof(LOGFONT);
}

void DeleteObject(HFONT font)
{
	delete font;
}

void DeleteObject(HBRUSH brush)
{
	delete brush;
}

HFONT GetStockObject(int /*object*/)
{
	static QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	return &fixed;
}

BOOL ChooseFont(CHOOSEFONT* chooseFont)
{
	if (!chooseFont || !chooseFont->lpLogFont)
		return FALSE;
	bool ok = false;
	nppExecOnGuiThread([&]() {
		const QFont chosen = QFontDialog::getFont(&ok, fontOf(*chooseFont->lpLogFont), chooseFont->hwndOwner);
		if (ok)
			toLogFont(chosen, chooseFont->lpLogFont);
	});
	return ok ? TRUE : FALSE;
}

namespace
{
	class CloseWatch : public QObject
	{
	public:
		CloseWatch(QWidget* window, WNDPROC proc) : QObject(window), m_proc(proc) {}
		WNDPROC m_proc;

	protected:
		bool eventFilter(QObject* watched, QEvent* event) override
		{
			if (event->type() == QEvent::Close && m_proc)
			{
				if (m_proc(static_cast<QWidget*>(watched), WM_CLOSE, 0, 0) == 0)
				{
					event->ignore();
					return true;
				}
			}
			return false;
		}
	};
}

void nppExecWatchClose(HWND mainWindow, WNDPROC proc)
{
	if (!mainWindow)
		return;
	nppExecOnGuiThread([&]() {
		auto* watch = reinterpret_cast<CloseWatch*>(mainWindow->property("nppExecCloseWatch").value<quintptr>());
		if (!proc)
		{
			delete watch;
			mainWindow->setProperty("nppExecCloseWatch", QVariant());
			return;
		}
		if (!watch)
		{
			watch = new CloseWatch(mainWindow, proc);
			mainWindow->installEventFilter(watch);
			mainWindow->setProperty("nppExecCloseWatch", QVariant::fromValue(reinterpret_cast<quintptr>(watch)));
		}
		watch->m_proc = proc;
	});
}

int nppExecMessageBoxTimeout(HWND parent, const wchar_t* text, const wchar_t* caption, UINT type, WORD /*language*/, DWORD milliseconds)
{
	int result = 0;
	nppExecOnGuiThread([&]() {
		// The buttons and icon as MessageBox of compat/windows.h gives them, with a timer that closes the box
		bool timedOut = false;
		QTimer timer;
		timer.setSingleShot(true);
		QObject::connect(&timer, &QTimer::timeout, &timer, [&timedOut]() {
			timedOut = true;
			if (QWidget* box = QApplication::activeModalWidget())
				box->close();
		});
		if (milliseconds != 0 && milliseconds != INFINITE)
			timer.start(static_cast<int>(milliseconds));
		result = MessageBox(parent, text, caption, type);
		if (timedOut)
			result = MB_TIMEDOUT;
	});
	return result;
}

BOOL IsMenu(HMENU menu)
{
	// Command IDs are below 65536, where no object is, and only a live widget is looked at
	if (reinterpret_cast<quintptr>(menu) <= 0xFFFF)
		return FALSE;
	BOOL isMenu = FALSE;
	nppExecOnGuiThread([&]() { isMenu = QApplication::allWidgets().contains(reinterpret_cast<QWidget*>(menu)) ? TRUE : FALSE; });
	return isMenu;
}

void DeleteObject(HBITMAP bitmap)
{
	delete bitmap;
}

HBRUSH CreateSolidBrush(COLORREF color)
{
	return new QBrush(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
}

void DestroyIcon(HICON icon)
{
	delete icon;
}

HWND GetFocus()
{
	HWND focused = nullptr;
	nppExecOnGuiThread([&]() { focused = QApplication::focusWidget(); });
	return focused;
}

