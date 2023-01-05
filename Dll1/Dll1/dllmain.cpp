#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <tlhelp32.h> 
#include <tchar.h>
#include <psapi.h>
#include <vector>
#include <queue>
#include <memory>
#include <unordered_map>
#include <string>
#include <time.h>
using namespace std;

typedef unsigned long long DDWORD;

class Mem_info {
public:
	char* addr; DDWORD block_size;
	Mem_info(char* addr, DDWORD block_size) {
		this->addr = addr; this->block_size = block_size;
	}
};
class Point {
public:
	char* addr; USHORT value;
	Point(char* addr, USHORT value) {
		this->addr = addr; this->value = value;
	}
};

HANDLE process = GetCurrentProcess();
DWORD pID;
vector<Point>* filtered;
vector<Mem_info>* process_ptrs;
short** prev_values;
BYTE bytes_to_read;

void debug(const char* msg) {
	MessageBox(NULL, msg, "Debug", NULL);
}

void ListProcessModules(){
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
	MODULEENTRY32 me32;
	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pID);
	me32.dwSize = sizeof(MODULEENTRY32);
	if(!Module32First(hModuleSnap, &me32)){
		CloseHandle(hModuleSnap);
		return;
	}
	do {
		_tprintf(TEXT("%s "), me32.szModule);
		printf("%p\n", me32.hModule, me32.modBaseAddr);
	} while (Module32Next(hModuleSnap, &me32));
	CloseHandle(hModuleSnap);
	return;
}

void get_process_memory() {
	//ListProcessModules();
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	delete filtered; filtered = NULL;
	delete process_ptrs;
	process_ptrs = new vector<Mem_info>;
	MEMORY_BASIC_INFORMATION mem_info;
	DWORD i = 0;
	for (char* addr = (char*)sysInfo.lpMinimumApplicationAddress;
		addr < (char*)sysInfo.lpMaximumApplicationAddress;
		addr += mem_info.RegionSize) {
		if (!VirtualQuery(addr, &mem_info, sizeof(mem_info))) {
			printf("Error: VirtualQueryEx");
			return;
		}
		if (!(mem_info.State & MEM_COMMIT) || !(mem_info.Type & MEM_PRIVATE) || mem_info.Type & MEM_IMAGE ||
			mem_info.Protect & PAGE_GUARD || !(mem_info.Protect & PAGE_READWRITE) || mem_info.Protect & PAGE_NOACCESS) continue;
		//if ((char*)mem_info.BaseAddress - (char*)0x700000000000 > 0) continue; // Skip module
		//if ((char*)mem_info.BaseAddress - (char*)0x100000000 < 0) continue; // Skip stack
		process_ptrs->push_back(Mem_info((char*)mem_info.BaseAddress, (DDWORD)mem_info.RegionSize));
		//printf("%d %p %d\n", i++, addr, mem_info.RegionSize);
	}
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

long ptr2val(char* ptr, char type) {
	if (type == 1)
		return *(unsigned char*)ptr;
	else if (type == -1)
		return *(char*)ptr;
	else if (type == 2)
		return *(unsigned short*)ptr;
	else if (type == -2)
		return *(short*)ptr;
	else if (type == 4)
		return *(float*)ptr;
}

void detect(BYTE type, bool is_even) {
	get_process_memory();
	DWORD count = 0;
	USHORT target = 0, limit = 0;
	if (!filtered) {
		short** prev_values = NULL;
		printf("1 [target] / 2: unknown / 0: exit\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		vector<string> choice2;
		if (choice[0] == "1")
			target = stoi(choice[1]);
		else if (choice[0] == "2") {
			// Make prev_values
			prev_values = new short* [process_ptrs->size()];
			for (DWORD i = 0; i < process_ptrs->size(); i++) {
				DDWORD block_size = process_ptrs->at(i).block_size;
				prev_values[i] = new short[(DDWORD)(block_size / 2) + 1];
				for (DDWORD j = 0; j < block_size - bytes_to_read + 1; j++) {
					char* addr = process_ptrs->at(i).addr + j;
					if (IsBadReadPtr(addr, 1))
						continue;
					BYTE remainder = (DDWORD)addr % 2;
					if ((is_even && remainder != 0) || (!is_even && remainder != 1))
						continue;

					prev_values[i][(DDWORD)(j / 2)] = ptr2val(addr, type);;
					count++;
				}
			}
			printf("Found results: %d\n", count);
			count = 0;

			// Choose a way to compare with prev_values
			printf("1: increased [limit] / 2: decreased [limit] / 3: changed [limit] / 0: exit\n");
			string input; getline(cin, input, '\n');
			choice2 = parse(input);
			if (choice2[0] == "1" || choice2[0] == "2" || choice[0] == "3")
				limit = stoi(choice2[1]);
			else {
				for (DWORD i = 0; i < process_ptrs->size(); i++)
					delete[] prev_values[i];
				delete[] prev_values;
				return;
			}
		}
		else return;

		// Make first filter
		filtered = new vector<Point>;
		for (DWORD i = 0; i < process_ptrs->size(); i++) {
			for (DDWORD j = 0; j < process_ptrs->at(i).block_size - bytes_to_read + 1; j++) {
				char* addr = process_ptrs->at(i).addr + j;
				if (IsBadReadPtr(addr, 1))
					continue;
				BYTE remainder = (DDWORD)addr % 2;
				if ((is_even && remainder != 0) || (!is_even && remainder != 1))
					continue;

				long value = ptr2val(addr, type);

				DDWORD half_j = (DDWORD)(j / 2);
				if (choice[0] == "1" && value == target) {
					filtered->push_back(Point(addr, value));
					count++;
					//printf("%p %d\n", addr, data);
				}
				else if (choice[0] == "2" &&
						((choice2[0] == "1" && value > prev_values[i][half_j] && (value - prev_values[i][half_j]) <= limit) ||
						(choice2[0] == "2" && value < prev_values[i][half_j] && (prev_values[i][half_j] - value) <= limit) ||
						(choice2[0] == "3" && value != prev_values[i][half_j] && abs(prev_values[i][half_j] - value) <= limit))
					) {
					filtered->push_back(Point(addr, value));
					count++;
				}
			}
			if (choice[0] == "2")
				delete[] prev_values[i];
		}
		if (choice[0] == "2")
			delete[] prev_values;
		printf("Found results: %d\n", count);
		count = 0;
	}

	// Keep filtering
	while (1) {
		printf("1 [target] / 2: increased [limit] / 3: decreased [limit] / 4: changed  [limit]/ 0: exit\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		if (choice[0] == "1")
			target = stoul(choice[1]);
		else if (choice[0] == "2" || choice[0] == "3" || choice[0] == "4")
			limit = stoi(choice[1]);
		else return;

		vector<Point>* new_filtered = new vector<Point>();
		for (Point point : *filtered) {
			if (IsBadReadPtr(point.addr, 1))
				continue;
			long value = ptr2val(point.addr, type);

			if ((choice[0] == "1" && value == target) ||
				(choice[0] == "2" && value > point.value && (value - point.value) <= limit) ||
				(choice[0] == "3" && value < point.value && (point.value - value) <= limit) ||
				(choice[0] == "4" && value != point.value && abs(point.value - value) <= limit)) {
				new_filtered->push_back(Point(point.addr, value));
				count++;
				//printf("%p %d\n", point.addr, data);
			}
		}
		delete filtered;
		filtered = new_filtered;
		printf("Found results: %d\n", count);
		count = 0;
	}
}

void read_result(int type) {
	while (true) {
		printf("(read_memory)\n1: start / 0: exit\n");
		string input; getline(cin, input, '\n');
		if (input == "0")
			return;

		DWORD index = 0;
		BYTE* read_buffer = new BYTE[abs(type)];
		for (Point point : *filtered) {
			if (IsBadReadPtr(point.addr, 1))
				continue;
			long value = ptr2val(point.addr, type);
			printf("(%d) %p %d\n", index++, point.addr, value);
		}
		delete[] read_buffer;
	}
}

BYTE decide_type(string choice) {
	BYTE type = stoi(choice);
	bytes_to_read = abs(type);
	return type;
}

void my_main() {
	AllocConsole();
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	ios::sync_with_stdio();

	BYTE type = 1;
	while (true) {
		printf("1 [type] / 2: detect even / 3: detect odd / 4: read result / 0: exit\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		if (choice[0] == "1")
			type = decide_type(choice[1]);
		else if (choice[0] == "2")
			detect(type, true);
		else if (choice[0] == "3")
			detect(type, false);
		else if (choice[0] == "4")
			read_result(type);
		else return;
	}
}

#include <thread>
bool attached = false;
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		thread(my_main).detach();
	return TRUE;
}
