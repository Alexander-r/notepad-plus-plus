#pragma once

struct ExportData;

class Exporter
{
public:
	Exporter(void) : _clipboardId(0) {};
	~Exporter(void);
	virtual bool exportData(ExportData * ) { return false; };
	virtual const TCHAR * getClipboardType() { return NULL; }; // Qt port: const, as it gives string literals
	int getClipboardID() { return _clipboardId; };
protected:
	UINT _clipboardId;

	void setClipboardID(UINT id) {
		_clipboardId = id;
	}
};
