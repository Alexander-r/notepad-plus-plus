// Qt port: see NppExecProcess.h

#include "NppExecProcess.h"

#include <QString>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#include <fcntl.h>
#include <pty.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
	// The process of each process handle's object, for TerminateProcess, which only has the handle
	std::mutex g_processesLock;
	std::map<HANDLE, pid_t> g_processes;

	std::string toLocal(const std::wstring& text)
	{
		return QString::fromStdWString(text).toLocal8Bit().toStdString();
	}
}

bool nppExecStartProcess(const std::vector<std::wstring>& argv, HANDLE stdinRead, HANDLE stdoutWrite, int ptyColumns, int ptyRows,
	HANDLE* ptyMaster, PROCESS_INFORMATION* processInfo, std::wstring* error)
{
	if (argv.empty())
	{
		if (error)
			*error = L"No program to start";
		return false;
	}

	// Everything the child needs, made before fork, where only async-signal-safe calls may follow
	std::vector<std::string> args;
	for (const std::wstring& arg : argv)
		args.push_back(toLocal(arg));
	std::vector<char*> cargs;
	for (std::string& arg : args)
		cargs.push_back(arg.data());
	cargs.push_back(nullptr);

	// The child tells an exec failure through this pipe, which closes on a successful exec
	int failPipe[2];
	if (::pipe2(failPipe, O_CLOEXEC) != 0)
	{
		if (error)
			*error = QString::fromLocal8Bit(std::strerror(errno)).toStdWString();
		return false;
	}

	int master = -1;
	pid_t pid = -1;
	if (ptyColumns > 0)
	{
		winsize size{};
		size.ws_col = static_cast<unsigned short>(ptyColumns);
		size.ws_row = static_cast<unsigned short>(ptyRows);
		pid = ::forkpty(&master, nullptr, nullptr, &size);
	}
	else
		pid = ::fork();

	if (pid < 0)
	{
		const int err = errno;
		::close(failPipe[0]);
		::close(failPipe[1]);
		if (error)
			*error = QString::fromLocal8Bit(std::strerror(err)).toStdWString();
		return false;
	}

	if (pid == 0)
	{
		// The child
		if (ptyColumns <= 0)
		{
			::setpgid(0, 0);
			::dup2(nppExecHandleFd(stdinRead), 0);
			::dup2(nppExecHandleFd(stdoutWrite), 1);
			::dup2(nppExecHandleFd(stdoutWrite), 2);
		}
		// forkpty has made a session of its own, whose group is the child's
		::signal(SIGINT, SIG_DFL);
		::signal(SIGQUIT, SIG_DFL);
		::signal(SIGPIPE, SIG_DFL);
		::close(failPipe[0]);
		::execvp(cargs[0], cargs.data());
		const int err = errno;
		[[maybe_unused]] const ssize_t written = ::write(failPipe[1], &err, sizeof(err));
		::_exit(127);
	}

	::close(failPipe[1]);
	int childError = 0;
	ssize_t got;
	do
		got = ::read(failPipe[0], &childError, sizeof(childError));
	while (got < 0 && errno == EINTR);
	::close(failPipe[0]);
	if (got == sizeof(childError))
	{
		::waitpid(pid, nullptr, 0);
		if (master >= 0)
			::close(master);
		if (error)
			*error = QString::fromLocal8Bit(std::strerror(childError)).toStdWString();
		return false;
	}
	if (ptyColumns <= 0)
		::setpgid(pid, pid); // also here, so that the group is there before anything signals it

	HANDLE process = nppExecCreateSignal();
	{
		std::lock_guard<std::mutex> lock(g_processesLock);
		g_processes[process] = pid;
	}
	// The reaper: signals the process handle with the exit code when the process ends, which an
	// object of its own keeps alive whatever happens to the handle
	HANDLE reaped = nppExecDuplicateHandle(process);
	std::thread([pid, reaped]()
	{
		int status = 0;
		while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
		{
		}
		DWORD exitCode = 0;
		if (WIFEXITED(status))
			exitCode = static_cast<DWORD>(WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			exitCode = static_cast<DWORD>(128 + WTERMSIG(status)); // as a shell gives it
		nppExecSetExitCode(reaped, exitCode);
		CloseHandle(reaped);
	}).detach();

	processInfo->hProcess = process;
	processInfo->hThread = nullptr;
	processInfo->dwProcessId = static_cast<DWORD>(pid);
	processInfo->dwThreadId = static_cast<DWORD>(pid);
	if (ptyMaster)
		*ptyMaster = (master >= 0) ? nppExecFileHandle(master) : nullptr;
	return true;
}

BOOL CreatePipe(HANDLE* readPipe, HANDLE* writePipe, void* /*attributes*/, DWORD /*size*/)
{
	// Closed on exec: the child gets its ends through dup2, and no other child inherits them
	int fds[2];
	if (::pipe2(fds, O_CLOEXEC) != 0)
		return FALSE;
	*readPipe = nppExecFileHandle(fds[0]);
	*writePipe = nppExecFileHandle(fds[1]);
	return TRUE;
}

BOOL PeekNamedPipe(HANDLE pipe, void* /*buffer*/, DWORD /*size*/, DWORD* bytesRead, DWORD* totalBytesAvail, DWORD* bytesLeftThisMessage)
{
	const int fd = nppExecHandleFd(pipe);
	if (fd < 0)
		return FALSE;
	int available = 0;
	if (::ioctl(fd, FIONREAD, &available) != 0)
		return FALSE;
	if (available == 0)
	{
		// Nothing to read: fails as on Windows once the writing end has closed, which poll tells by a hang-up
		pollfd p{fd, POLLIN, 0};
		if (::poll(&p, 1, 0) > 0 && (p.revents & (POLLHUP | POLLERR)) != 0 && (p.revents & POLLIN) == 0)
			return FALSE;
	}
	if (bytesRead)
		*bytesRead = static_cast<DWORD>(available);
	if (totalBytesAvail)
		*totalBytesAvail = static_cast<DWORD>(available);
	if (bytesLeftThisMessage)
		*bytesLeftThisMessage = 0;
	return TRUE;
}

BOOL ReadFile(HANDLE file, void* buffer, DWORD size, DWORD* bytesRead, void* /*overlapped*/)
{
	const int fd = nppExecHandleFd(file);
	ssize_t got;
	do
		got = ::read(fd, buffer, size);
	while (got < 0 && errno == EINTR);
	if (bytesRead)
		*bytesRead = (got > 0) ? static_cast<DWORD>(got) : 0;
	// A terminal whose child has gone gives EIO, which is its end of file
	return (got >= 0 || errno == EIO) ? TRUE : FALSE;
}

BOOL WriteFile(HANDLE file, const void* buffer, DWORD size, DWORD* bytesWritten, void* /*overlapped*/)
{
	const int fd = nppExecHandleFd(file);
	const char* data = static_cast<const char*>(buffer);
	DWORD written = 0;
	while (written < size)
	{
		const ssize_t n = ::write(fd, data + written, size - written);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		written += static_cast<DWORD>(n);
	}
	if (bytesWritten)
		*bytesWritten = written;
	return (written == size) ? TRUE : FALSE;
}

BOOL FlushFileBuffers(HANDLE /*file*/)
{
	// Pipes and terminals have no buffer of their own here
	return TRUE;
}

bool nppExecSignalProcess(const PROCESS_INFORMATION* processInfo, int signal)
{
	const pid_t pid = static_cast<pid_t>(processInfo->dwProcessId);
	if (pid <= 0)
		return false;
	// The group, which is the child's own, or the child alone if the group has gone
	return ::kill(-pid, signal) == 0 || ::kill(pid, signal) == 0;
}

BOOL TerminateProcess(HANDLE process, UINT /*exitCode*/)
{
	pid_t pid = -1;
	{
		std::lock_guard<std::mutex> lock(g_processesLock);
		const auto it = g_processes.find(process);
		if (it != g_processes.end())
			pid = it->second;
	}
	if (pid <= 0)
		return FALSE;
	return (::kill(-pid, SIGKILL) == 0 || ::kill(pid, SIGKILL) == 0) ? TRUE : FALSE;
}
