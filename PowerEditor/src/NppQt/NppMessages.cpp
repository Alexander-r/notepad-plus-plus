// This file is part of Notepad++ project

#include "NppMessages.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QWidget>

namespace
{
	struct Registry
	{
		QMutex mutex;
		QHash<HWND, std::shared_ptr<NppMessageHandler>> handlers;
	};

	Registry& registry()
	{
		static Registry instance;
		return instance;
	}

	std::shared_ptr<NppMessageHandler> handlerOf(HWND target)
	{
		Registry& r = registry();
		QMutexLocker lock(&r.mutex);
		return r.handlers.value(target);
	}
}

namespace
{
	HWND g_mainWindow = nullptr;
}

HWND nppMainWindow()
{
	return g_mainWindow;
}

void nppSetMainWindow(HWND window)
{
	g_mainWindow = window;
}

void nppRegisterMessageTarget(HWND target, NppMessageHandler handler)
{
	Registry& r = registry();
	QMutexLocker lock(&r.mutex);
	r.handlers.insert(target, std::make_shared<NppMessageHandler>(std::move(handler)));
}

void nppUnregisterMessageTarget(HWND target)
{
	Registry& r = registry();
	QMutexLocker lock(&r.mutex);
	r.handlers.remove(target);
}

namespace
{
	// A widget of no handler, such as a plugin's, gets the message as an NppMessageEvent, if it is still there
	LRESULT sendAsEvent(HWND target, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (!target || !QApplication::allWidgets().contains(target))
			return 0;
		NppMessageEvent event(message, wParam, lParam);
		QCoreApplication::sendEvent(target, &event);
		return event.result;
	}
}

LRESULT SendMessage(HWND target, UINT message, WPARAM wParam, LPARAM lParam)
{
	const auto handler = handlerOf(target);
	if (!handler)
	{
		if (!target)
			return 0;
		QCoreApplication* app = QCoreApplication::instance();
		if (!app)
			return 0;
		if (QThread::currentThread() == app->thread())
			return sendAsEvent(target, message, wParam, lParam);
		LRESULT result = 0;
		QMetaObject::invokeMethod(app, [&]() { result = sendAsEvent(target, message, wParam, lParam); }, Qt::BlockingQueuedConnection);
		return result;
	}

	QCoreApplication* app = QCoreApplication::instance();
	if (!app || QThread::currentThread() == app->thread())
		return (*handler)(message, wParam, lParam);

	LRESULT result = 0;
	QMetaObject::invokeMethod(app, [&]() { result = (*handler)(message, wParam, lParam); }, Qt::BlockingQueuedConnection);
	return result;
}

bool PostMessage(HWND target, UINT message, WPARAM wParam, LPARAM lParam)
{
	QCoreApplication* app = QCoreApplication::instance();
	if (!app || !target)
		return false;
	if (!handlerOf(target))
	{
		// For a widget of no handler, as an NppMessageEvent once it is the GUI thread's turn, if the widget is still there
		return QMetaObject::invokeMethod(app, [target, message, wParam, lParam]()
		{
			if (const auto handler = handlerOf(target))
				(*handler)(message, wParam, lParam);
			else
				sendAsEvent(target, message, wParam, lParam);
		}, Qt::QueuedConnection);
	}

	return QMetaObject::invokeMethod(app, [target, message, wParam, lParam]()
	{
		if (const auto handler = handlerOf(target))
			(*handler)(message, wParam, lParam);
	}, Qt::QueuedConnection);
}
