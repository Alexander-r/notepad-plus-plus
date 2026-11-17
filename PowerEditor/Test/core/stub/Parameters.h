// Minimal stand-in so FileInterface.cpp can be tested without the full settings layer.
#pragma once

#include <string>

#include "Common.h"

class NppParameters
{
public:
	static NppParameters& getInstance() { static NppParameters instance; return instance; }
	bool isEndSessionCritical() const { return false; }
	const std::wstring& getUserPath() const { return _userPath; }

private:
	std::wstring _userPath;
};
