// Qt port: child processes for NppExec's CChildProcess, with fork and exec, pipes or a pseudo terminal.
//
// The process handle is a waitable object of NppExecSync.h, signalled when the process ends, so that
// WaitForSingleObject and GetExitCodeProcess work on it as on Windows. Its pipes are file handles of NppExecSync.h,
// read and written with the calls of the same names. The child gets a process group of its own, so that a signal for
// it, as Ctrl+C in a console on Windows, reaches the processes it starts.

#pragma once

#include "NppExecWin.h"

#include <string>
#include <vector>

struct PROCESS_INFORMATION
{
	HANDLE hProcess = nullptr;
	HANDLE hThread = nullptr;
	DWORD dwProcessId = 0;
	DWORD dwThreadId = 0;
};

// Starts the program of argv[0], found through PATH, with the arguments. Its standard input is stdinRead, and its
// output and errors go to stdoutWrite, or all three go to a new pseudo terminal of that size when ptyColumns is
// above 0, whose master is given in *ptyMaster. On failure gives false, with errno's message in *error.
bool nppExecStartProcess(const std::vector<std::wstring>& argv, HANDLE stdinRead, HANDLE stdoutWrite, int ptyColumns, int ptyRows,
	HANDLE* ptyMaster, PROCESS_INFORMATION* processInfo, std::wstring* error);

// A pipe, as CreatePipe
BOOL CreatePipe(HANDLE* readPipe, HANDLE* writePipe, void* attributes, DWORD size);
// The bytes ready to be read, as PeekNamedPipe gives them. Fails when the other end is closed and nothing is left.
BOOL PeekNamedPipe(HANDLE pipe, void* buffer, DWORD size, DWORD* bytesRead, DWORD* totalBytesAvail, DWORD* bytesLeftThisMessage);
BOOL ReadFile(HANDLE file, void* buffer, DWORD size, DWORD* bytesRead, void* overlapped);
BOOL WriteFile(HANDLE file, const void* buffer, DWORD size, DWORD* bytesWritten, void* overlapped);
BOOL FlushFileBuffers(HANDLE file);

// Sends the signal to the process group of the child, as a console's Ctrl+C (SIGINT) and Ctrl+Break (SIGQUIT) reach
// every process of the console, or as WM_CLOSE asks a program to end (SIGTERM)
bool nppExecSignalProcess(const PROCESS_INFORMATION* processInfo, int signal);
// As TerminateProcess: kills the process group at once
BOOL TerminateProcess(HANDLE process, UINT exitCode);
