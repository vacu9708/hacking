#include <Windows.h>
#include <iostream>
using namespace std;

#include <tlhelp32.h> 
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

inline BOOL SetPrivilege(HANDLE hToken) {
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(
        NULL,            // lookup privilege on local system
        SE_DEBUG_NAME,   // privilege to lookup 
        &luid))        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError());
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Enable the privilege or disable all privileges.

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
        printf("AdjustTokenPrivileges error: %u\n", GetLastError());
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        printf("The token does not have the specified privilege. \n");
        return FALSE;
    }

    return TRUE;
}

inline void attach_debugger(HANDLE process, DWORD pID, DWORD_PTR address) {
    HANDLE hToken;
    if (!OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return;
    //SetPrivilege(hToken);
	DebugActiveProcess(pID);
    return;
	DebugSetProcessKillOnExit(false);
    DWORD_PTR dwThreadID = GetProcessThreadID(process);
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadID);
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_INTEGER;
    ctx.Dr0 = address;
    //ctx.Dr6 = 0;           //clear debug status register (only bits 0-3 of dr6 are cleared by processor)
    ctx.Dr7 = 0x00000001;

    SetThreadContext(hThread, &ctx);
    DWORD continueStatus = DBG_CONTINUE;
    DEBUG_EVENT dbgEvent;
    long i = 0;
    while (true) {
        if (WaitForDebugEvent(&dbgEvent, INFINITE) == 0)
            break;
        //printf("1\n");
        if (dbgEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            dbgEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP) // EXCEPTION_BREAKPOINT
        {

            CONTEXT newCtx = { 0 };
            newCtx.ContextFlags = CONTEXT_ALL;
            GetThreadContext(hThread, &newCtx);
            if (dbgEvent.u.Exception.ExceptionRecord.ExceptionAddress == (LPVOID)address)
            {
                newCtx.Dr0 = newCtx.Dr6 = newCtx.Dr7 = 0;
                newCtx.EFlags |= (1 << 8);
            }
            else {
                newCtx.Dr0 = address;
                newCtx.Dr7 = 0x00000001;
                newCtx.EFlags &= ~(1 << 8);
            }
            SetThreadContext(hThread, &newCtx);
        }
        ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, DBG_CONTINUE);
    }
}