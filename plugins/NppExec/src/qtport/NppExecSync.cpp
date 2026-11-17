// Qt port: see NppExecSync.h

#include "NppExecWin.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/syscall.h>
#include <unistd.h>

namespace
{
	// One lock and condition for every object, which is plenty for the few that NppExec waits on. They are never
	// destroyed: NppExec's threads may still wait on them as the process exits, and a condition variable's
	// destructor would wait for them.
	std::mutex& g_lock = *new std::mutex;
	std::condition_variable& g_changed = *new std::condition_variable;

	struct Waitable
	{
		bool signalled = false;
		bool manualReset = true;
		DWORD exitCode = STILL_ACTIVE;
	};

	// What a HANDLE points to: each handle owns a reference to its object
	struct Handle
	{
		std::shared_ptr<Waitable> object;
		int fd = -1; // a pipe or terminal, closed with the handle
	};

	std::shared_ptr<Waitable> objectOf(HANDLE handle)
	{
		return handle ? static_cast<Handle*>(handle)->object : nullptr;
	}

	HANDLE newHandle(std::shared_ptr<Waitable> object)
	{
		return new Handle{std::move(object)};
	}

	// With g_lock held: whether the wait is over, taking the signal of auto-reset events
	bool tryTake(const std::vector<std::shared_ptr<Waitable>>& objects, bool waitAll, DWORD& result)
	{
		if (waitAll)
		{
			for (const auto& object : objects)
			{
				if (!object->signalled)
					return false;
			}
			for (const auto& object : objects)
			{
				if (!object->manualReset)
					object->signalled = false;
			}
			result = WAIT_OBJECT_0;
			return true;
		}
		for (std::size_t i = 0; i < objects.size(); ++i)
		{
			if (objects[i]->signalled)
			{
				if (!objects[i]->manualReset)
					objects[i]->signalled = false;
				result = WAIT_OBJECT_0 + static_cast<DWORD>(i);
				return true;
			}
		}
		return false;
	}
}

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES /*attributes*/, BOOL manualReset, BOOL initialState, const wchar_t* /*name*/)
{
	auto event = std::make_shared<Waitable>();
	event->manualReset = manualReset != FALSE;
	event->signalled = initialState != FALSE;
	return newHandle(std::move(event));
}

BOOL SetEvent(HANDLE event)
{
	const auto object = objectOf(event);
	if (!object)
		return FALSE;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		object->signalled = true;
	}
	g_changed.notify_all();
	return TRUE;
}

BOOL ResetEvent(HANDLE event)
{
	const auto object = objectOf(event);
	if (!object)
		return FALSE;
	std::lock_guard<std::mutex> lock(g_lock);
	object->signalled = false;
	return TRUE;
}

HANDLE nppExecCreateSignal()
{
	return newHandle(std::make_shared<Waitable>());
}

void nppExecSignal(HANDLE handle)
{
	SetEvent(handle);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES /*attributes*/, SIZE_T /*stackSize*/, LPTHREAD_START_ROUTINE function, LPVOID parameter, DWORD /*flags*/,
	DWORD* threadId)
{
	auto object = std::make_shared<Waitable>();
	try
	{
		std::thread thread([object, function, parameter]()
		{
			const DWORD exitCode = function(parameter);
			{
				std::lock_guard<std::mutex> lock(g_lock);
				object->exitCode = exitCode;
				object->signalled = true;
			}
			g_changed.notify_all();
		});
		if (threadId)
			*threadId = static_cast<DWORD>(std::hash<std::thread::id>{}(thread.get_id()));
		thread.detach();
	}
	catch (...)
	{
		return nullptr;
	}
	return newHandle(std::move(object));
}

BOOL GetExitCodeThread(HANDLE thread, DWORD* exitCode)
{
	const auto object = objectOf(thread);
	if (!object || !exitCode)
		return FALSE;
	std::lock_guard<std::mutex> lock(g_lock);
	*exitCode = object->exitCode;
	return TRUE;
}

DWORD GetCurrentThreadId()
{
	return static_cast<DWORD>(::syscall(SYS_gettid));
}

DWORD GetCurrentProcessId()
{
	return static_cast<DWORD>(::getpid());
}

DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds)
{
	std::vector<std::shared_ptr<Waitable>> objects;
	for (DWORD i = 0; i < count; ++i)
	{
		auto object = objectOf(handles[i]);
		if (!object)
			return WAIT_FAILED;
		objects.push_back(std::move(object));
	}

	DWORD result = WAIT_TIMEOUT;
	std::unique_lock<std::mutex> lock(g_lock);
	const auto ready = [&]() { return tryTake(objects, waitAll != FALSE, result); };
	if (milliseconds == INFINITE)
		g_changed.wait(lock, ready);
	else if (!g_changed.wait_for(lock, std::chrono::milliseconds(milliseconds), ready))
		return WAIT_TIMEOUT;
	return result;
}

DWORD WaitForSingleObject(HANDLE object, DWORD milliseconds)
{
	return WaitForMultipleObjects(1, &object, FALSE, milliseconds);
}

BOOL CloseHandle(HANDLE object)
{
	if (!object)
		return FALSE;
	auto* handle = static_cast<Handle*>(object);
	if (handle->fd >= 0)
		::close(handle->fd);
	delete handle;
	return TRUE;
}

HANDLE nppExecFileHandle(int fd)
{
	auto* handle = new Handle;
	handle->fd = fd;
	return handle;
}

int nppExecHandleFd(HANDLE object)
{
	return object ? static_cast<Handle*>(object)->fd : -1;
}

void nppExecSetExitCode(HANDLE object, DWORD exitCode)
{
	if (const auto waitable = objectOf(object))
	{
		{
			std::lock_guard<std::mutex> lock(g_lock);
			waitable->exitCode = exitCode;
			waitable->signalled = true;
		}
		g_changed.notify_all();
	}
}

BOOL GetExitCodeProcess(HANDLE process, DWORD* exitCode)
{
	return GetExitCodeThread(process, exitCode);
}

HANDLE nppExecDuplicateHandle(HANDLE object)
{
	return object ? newHandle(static_cast<Handle*>(object)->object) : nullptr;
}

void InitializeCriticalSection(CRITICAL_SECTION* section)
{
	section->lock = new std::recursive_mutex;
}

void DeleteCriticalSection(CRITICAL_SECTION* section)
{
	delete static_cast<std::recursive_mutex*>(section->lock);
	section->lock = nullptr;
}

void EnterCriticalSection(CRITICAL_SECTION* section)
{
	static_cast<std::recursive_mutex*>(section->lock)->lock();
}

void LeaveCriticalSection(CRITICAL_SECTION* section)
{
	static_cast<std::recursive_mutex*>(section->lock)->unlock();
}

LONG InterlockedIncrement(volatile LONG* value)
{
	return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}

LONG InterlockedDecrement(volatile LONG* value)
{
	return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST);
}

LONG InterlockedExchange(volatile LONG* target, LONG value)
{
	return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

LONG InterlockedCompareExchange(volatile LONG* destination, LONG exchange, LONG comparand)
{
	__atomic_compare_exchange_n(destination, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comparand;
}

void Sleep(DWORD milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

ULONGLONG GetTickCount64()
{
	return static_cast<ULONGLONG>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

DWORD GetTickCount()
{
	return static_cast<DWORD>(GetTickCount64());
}
