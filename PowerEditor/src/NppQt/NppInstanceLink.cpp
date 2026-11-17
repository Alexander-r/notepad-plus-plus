// This file is part of Notepad++ project

#include "NppInstanceLink.h"

#include <QDataStream>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QWidget>

#include <unistd.h>

#include "Notepad_plus_msgs.h"
#include "NppMessages.h"
#include "resource.h"

namespace
{
	constexpr int connectTimeoutMs = 300;
	constexpr int writeTimeoutMs = 3000;
	constexpr qint64 maxMessageBytes = 64 * 1024 * 1024;

	QString socketName()
	{
		QString folder = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
		if (folder.isEmpty())
			folder = QDir::tempPath();
		return folder + QStringLiteral("/notepad-plus-plus-%1.instance").arg(::getuid());
	}

	// Each message is its type and its bytes
	void deliver(HWND mainWindow, const QByteArray& received)
	{
		// Restore the window, bring it to front, etc
		if (!::SendMessage(mainWindow, NPPM_INTERNAL_RESTOREFROMMINIMIZED, 0, 0))
		{
			mainWindow->raise();
			mainWindow->activateWindow();
		}

		QDataStream stream(received);
		while (!stream.atEnd())
		{
			quint64 type = 0;
			QByteArray data;
			stream >> type >> data;
			if (stream.status() != QDataStream::Ok || data.isEmpty())
				return;

			COPYDATASTRUCT message;
			message.dwData = static_cast<ULONG_PTR>(type);
			message.cbData = static_cast<DWORD>(data.size());
			message.lpData = data.data();
			::SendMessage(mainWindow, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&message));
		}
	}
}

bool nppIsInstanceRunning()
{
	QLocalSocket socket;
	socket.connectToServer(socketName());
	return socket.waitForConnected(connectTimeoutMs);
}

bool nppSendToRunningInstance(const std::vector<NppInstanceMessage>& messages)
{
	QLocalSocket socket;
	socket.connectToServer(socketName());
	if (!socket.waitForConnected(connectTimeoutMs))
		return false;

	QByteArray payload;
	QDataStream stream(&payload, QIODevice::WriteOnly);
	for (const NppInstanceMessage& message : messages)
		stream << static_cast<quint64>(message._type) << message._data;

	socket.write(payload);
	socket.waitForBytesWritten(writeTimeoutMs);
	socket.disconnectFromServer();
	if (socket.state() != QLocalSocket::UnconnectedState)
		socket.waitForDisconnected(writeTimeoutMs);
	return true;
}

NppInstanceServer::NppInstanceServer() = default;
NppInstanceServer::~NppInstanceServer() = default;

bool NppInstanceServer::listen()
{
	_server = std::make_unique<QLocalServer>();
	_server->setSocketOptions(QLocalServer::UserAccessOption);

	// Left behind by an instance that crashed. Callers have already found that nobody answers on it.
	QLocalServer::removeServer(socketName());
	if (!_server->listen(socketName()))
		return false;

	QObject::connect(_server.get(), &QLocalServer::newConnection, _server.get(), [this]()
	{
		while (QLocalSocket* socket = _server->nextPendingConnection())
		{
			auto* received = new QByteArray;
			QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, received]()
			{
				received->append(socket->readAll());
				if (received->size() > maxMessageBytes)
				{
					received->clear();
					socket->abort();
				}
			});
			QObject::connect(socket, &QLocalSocket::disconnected, socket, [this, socket, received]()
			{
				received->append(socket->readAll());
				if (_mainWindow)
					deliver(_mainWindow, *received);
				delete received;
				socket->deleteLater();
			});
		}
	});
	return true;
}
