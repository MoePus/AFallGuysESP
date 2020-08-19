#include "RemoteType.h"
#include "tlhelp32.h"
#include "psapi.h"
#include <fstream>
#include <iostream>
#include <string>
#include "const.h"

bool EnableDebugPrivileges()
{
	HANDLE hToken = 0;
	TOKEN_PRIVILEGES newPrivs;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		return FALSE;

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &newPrivs.Privileges[0].Luid))
	{
		CloseHandle(hToken);
		return FALSE;
	}

	newPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	newPrivs.PrivilegeCount = 1;

	if (AdjustTokenPrivileges(hToken, FALSE, &newPrivs, 0, NULL, NULL))
	{
		CloseHandle(hToken);
		return TRUE;
	}
	else
	{
		CloseHandle(hToken);
		return FALSE;
	}

}

HANDLE hProcess = 0;
HANDLE getVictimHandle(bool force)
{
	if (!hProcess || force)
	{
		DWORD processId = 0;
		HWND victim = FindWindowA(0, FGWINDOW);
		if (!victim)
		{
			throw std::invalid_argument("Cannot find Window");
		}
		GetWindowThreadProcessId(victim, &processId);
		EnableDebugPrivileges();
		hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
			PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processId);

		if (!hProcess)
		{
			throw std::runtime_error("Cannot Open Process");
		}
	}
	return hProcess;
}

BOOL RPM(
	LPCVOID lpBaseAddress,
	LPVOID lpBuffer,
	SIZE_T nSize,
	SIZE_T* lpNumberOfBytesRead
)
{
	//log_file << std::to_string((size_t)lpBaseAddress)+"\n";
	return ReadProcessMemory(getVictimHandle(), lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);
}
