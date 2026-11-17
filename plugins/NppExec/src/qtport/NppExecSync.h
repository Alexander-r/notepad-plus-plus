// Qt port: Windows' kernel objects as NppExec waits on them, with the same calls.
//
// NppExec's engine runs scripts on threads that wait for events and for each other through handles, with
// WaitForMultipleObjects. These give events, threads and critical sections the meaning they have on Windows, on top
// of the standard library, so that the engine stays as it is. A handle is closed with CloseHandle, and an object lives
// on while a thread still runs or waits on it.

#pragma once

#include <windows.h>

using LPSECURITY_ATTRIBUTES = void*;
using LPTHREAD_START_ROUTINE = DWORD (*)(LPVOID);

inline constexpr DWORD INFINITE = 0xFFFFFFFF;
inline constexpr DWORD WAIT_OBJECT_0 = 0;
inline constexpr DWORD WAIT_ABANDONED_0 = 0x80;
inline constexpr DWORD WAIT_TIMEOUT = 258;
inline constexpr DWORD WAIT_FAILED = 0xFFFFFFFF;
inline constexpr DWORD STILL_ACTIVE = 259;
inline constexpr DWORD CREATE_SUSPENDED = 0x4;

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES attributes, BOOL manualReset, BOOL initialState, const wchar_t* name);
BOOL SetEvent(HANDLE event);
BOOL ResetEvent(HANDLE event);

// A thread that runs the function, and whose handle is signalled when it returns
HANDLE CreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stackSize, LPTHREAD_START_ROUTINE function, LPVOID parameter, DWORD flags,
	DWORD* threadId);
// Threads start at once, so this has nothing to do
inline DWORD ResumeThread(HANDLE /*thread*/) { return 0; }
BOOL GetExitCodeThread(HANDLE thread, DWORD* exitCode);
DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();

DWORD WaitForSingleObject(HANDLE object, DWORD milliseconds);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* objects, BOOL waitAll, DWORD milliseconds);
BOOL CloseHandle(HANDLE object);

// For other code that makes a waitable object, such as a child process: signalled once, when it ends
HANDLE nppExecCreateSignal();
void nppExecSignal(HANDLE object);
// Signals the object, with the exit code that GetExitCodeProcess gives
void nppExecSetExitCode(HANDLE object, DWORD exitCode);
BOOL GetExitCodeProcess(HANDLE process, DWORD* exitCode);
// Another handle of the same object, which lives on until both are closed
HANDLE nppExecDuplicateHandle(HANDLE object);

// A handle of a file descriptor, a pipe or a terminal, which CloseHandle closes
HANDLE nppExecFileHandle(int fd);
int nppExecHandleFd(HANDLE object);

// A recursive lock, as Windows' critical section is
struct CRITICAL_SECTION
{
	void* lock = nullptr;
};
void InitializeCriticalSection(CRITICAL_SECTION* section);
void DeleteCriticalSection(CRITICAL_SECTION* section);
void EnterCriticalSection(CRITICAL_SECTION* section);
void LeaveCriticalSection(CRITICAL_SECTION* section);

LONG InterlockedIncrement(volatile LONG* value);
LONG InterlockedDecrement(volatile LONG* value);
LONG InterlockedExchange(volatile LONG* target, LONG value);
LONG InterlockedCompareExchange(volatile LONG* destination, LONG exchange, LONG comparand);

void Sleep(DWORD milliseconds);
DWORD GetTickCount();
ULONGLONG GetTickCount64();
