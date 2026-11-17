// This file is part of Notepad++ project
//
// Connects a new start of the application to the instance already running, as WM_COPYDATA does on Windows: a new
// start hands over its file names and goes away. It is a local socket that only this user can reach.

#pragma once

#include <memory>
#include <vector>

#include <QByteArray>

#include "NppPlatform.h"

class QLocalServer;

struct NppInstanceMessage
{
	ULONG_PTR _type = 0; // COPYDATA_*
	QByteArray _data;
};

// Whether another instance has taken the messages. An empty list only brings it to the front.
bool nppSendToRunningInstance(const std::vector<NppInstanceMessage>& messages);

// Whether an instance is running
bool nppIsInstanceRunning();

// The running instance's end. Messages arrive at the main window, as WM_COPYDATA.
class NppInstanceServer
{
public:
	NppInstanceServer();
	~NppInstanceServer();

	bool listen();
	void setMainWindow(HWND mainWindow) { _mainWindow = mainWindow; }

private:
	std::unique_ptr<QLocalServer> _server;
	HWND _mainWindow = nullptr;
};
