#include <Windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <tchar.h>
#include <string>
#include <vector>
using namespace std;

LPVOID find_dll(TCHAR* szModuleName, DWORD pID) {
	int count = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pID); // make snapshot of all modules within process
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 ModuleEntry32;
		ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(hSnapshot, &ModuleEntry32)) {//store first Module in ModuleEntry32
			do {
				//count++;
				//printf("%s\n", ModuleEntry32.szModule);
				if (_tcsicmp(ModuleEntry32.szModule, szModuleName) == 0) {// if found module matches the module we look for
					printf("%s\n", ModuleEntry32.szModule);
					return (LPVOID)ModuleEntry32.modBaseAddr;
				}
			} while (Module32Next(hSnapshot, &ModuleEntry32)); // go through Module entries in Snapshot and store in ModuleEntry32
		}
		CloseHandle(hSnapshot);
	}
	//printf("%d\n", count);
	return NULL;
}

vector<string> parse(string str) {
	vector<string> choice;
	short start = 0;
	for (short i = 0; i < str.length(); i++)
		if (i == str.length() - 1)
			choice.push_back(str.substr(start, i + 1 - start));
		else if (str[i] == ' ') {
			choice.push_back(str.substr(start, i - start));
			start = i + 1;
		}
	return choice;
}

int main(){
	DWORD pID = NULL;
	printf("1: Minecraft* 1.19.3 - Singleplayer\n2: *Untitled - Notepad\n3: Overwatch\n4: Manual input\n");
	string input; getline(cin, input, '\n');
	vector<string> choice = parse(input);
	HWND hWindow = NULL;
	if (choice[0] == "4")
		pID = stoul(choice[1]);
	else {
		if (choice[0] == "1")
			hWindow = FindWindow(NULL, "Minecraft* 1.19.3 - Singleplayer");
		else if (choice[0] == "2")
			hWindow = FindWindow(NULL, "*Untitled - Notepad");
		else if (choice[0] == "3")
			hWindow = FindWindow(NULL, "Overwatch");
		GetWindowThreadProcessId(hWindow, &pID);
	}
	HANDLE p_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
	if (!p_handle) {
		fprintf(stderr, "[-] ERROR: Couldn't open process %lu. OpenProcess failed with error: %lu.\n", pID, GetLastError());
		return 0;
	}
	//LPCSTR dllPath = "G:\\Software\\Projects\\Dll1\\Release\\Dll1.dll";
	LPCSTR dllPath = "G:\\Software\\Projects\\Dll1\\x64\\Release\\Dll1.dll";
	LPVOID remoteBuffer = VirtualAllocEx(p_handle, NULL, strlen(dllPath), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!remoteBuffer) printf("remote Buffer\n");
	if (!WriteProcessMemory(p_handle, remoteBuffer, dllPath, strlen(dllPath), NULL)) printf("Error: write process memory\n");
	HMODULE hModule = GetModuleHandleA("Kernel32.dll");
	LPTHREAD_START_ROUTINE threadAddress = (LPTHREAD_START_ROUTINE)GetProcAddress(hModule, "LoadLibraryA");
	if (!threadAddress) printf("Error: thread address\n");
	HANDLE hThread = CreateRemoteThread(p_handle, NULL, 0, threadAddress, remoteBuffer, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
	VirtualFreeEx(p_handle, remoteBuffer, strlen(dllPath), MEM_RELEASE); // Free the memory allocated for our dll path
	
	char module_name[] = "Dll1.dll";
	LPVOID dll_addr = find_dll(module_name, pID);
	cin.get();

	threadAddress = (LPTHREAD_START_ROUTINE)GetProcAddress(hModule, "FreeLibrary");
	hThread = CreateRemoteThread(p_handle, NULL, 0, threadAddress, dll_addr, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
	find_dll(module_name, pID);
	return 0;
}