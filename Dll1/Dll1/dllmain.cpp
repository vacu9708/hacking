#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <tlhelp32.h> 
#include <tchar.h>
#include <psapi.h>
#include <vector>
#include <string>
using namespace std;

DWORD_PTR addr = (DWORD_PTR)2989548806904;
CONTEXT ctx;
HANDLE hThread;

LONG WINAPI veh(PEXCEPTION_POINTERS ExceptionInfo) {
	printf("MyVEHHandler (0x%x)\n", ExceptionInfo->ExceptionRecord->ExceptionCode);
	CONTEXT newCtx = { 0 };
	newCtx.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &newCtx);
	
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
		if (ExceptionInfo->ExceptionRecord->ExceptionAddress == (LPVOID)addr) {
			printf("Breakpoint %p\n", ExceptionInfo->ExceptionRecord->ExceptionAddress);
			newCtx.Dr0 = newCtx.Dr6 = newCtx.Dr7 = 0;
			newCtx.EFlags |= (1 << 8);
			SetThreadContext(hThread, &newCtx);
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		else {
			newCtx.Dr0 = addr;
			newCtx.Dr7 = 0x00000001;
			newCtx.EFlags &= ~(1 << 8);
		}
		SetThreadContext(hThread, &newCtx);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

inline DWORD GetProcessThreadID(HANDLE Process)
{
	THREADENTRY32 entry;
	entry.dwSize = sizeof(THREADENTRY32);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	if (Thread32First(snapshot, &entry) == TRUE)
	{
		DWORD PID = GetProcessId(Process);
		while (Thread32Next(snapshot, &entry) == TRUE)
		{
			if (entry.th32OwnerProcessID == PID)
			{
				CloseHandle(snapshot);
				return entry.th32ThreadID;
			}
		}
	}
	CloseHandle(snapshot);
	return NULL;
}

void my_main() {
	AllocConsole();
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	ios::sync_with_stdio();

	CONTEXT ctx = { 0 };
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_INTEGER;
	ctx.Dr0 = addr;
	//ctx.Dr6 = 0;           //clear debug status register (only bits 0-3 of dr6 are cleared by processor)
	ctx.Dr7 = 0x00000001;
	DWORD_PTR dwThreadID = GetProcessThreadID(GetCurrentProcess());
	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadID);
	printf("hi\n");
	SetUnhandledExceptionFilter(veh);
	AddVectoredExceptionHandler(1, veh);
	AddVectoredContinueHandler(1, veh);
}
	

#include <thread>
bool attached = false;
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		thread(my_main).detach();
	return TRUE;
}
