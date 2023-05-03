#include <Windows.h>
#include <iostream>
#include <tlhelp32.h> 
#include <tchar.h>
#include <vector>
#include <string>
#include <time.h>
#include <cmath>
using namespace std;

typedef unsigned long long DDWORD;

class Mem_info { // information of memory block
public:
	BYTE* addr; DDWORD block_size;
	Mem_info(BYTE* addr, DDWORD block_size) {
		this->addr = addr; this->block_size = block_size;
	}
};
class Point { // (address - value read from it) pair
public:
	BYTE* addr; USHORT value;
	Point(BYTE* addr, USHORT value) {
		this->addr = addr; this->value = value;
	}
};

HANDLE process;
DWORD pID;
vector<Point>* filtered;
vector<Mem_info>* process_ptrs;
short** prev_values;
BYTE* read_buffer;
BYTE bytes_to_read = 1;
BYTE* exe_addr;
DDWORD max_block_size = 0;

/*void msg_box(const char* msg) {
	MessageBox(NULL, msg, "Debug", NULL);
}*/

void get_process_memory(DWORD start, DWORD end) {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	delete filtered; filtered = NULL;
	delete process_ptrs;
	process_ptrs = new vector<Mem_info>;
	MEMORY_BASIC_INFORMATION mem_info;
	DWORD i = 0;
	for (BYTE* addr = (BYTE*)sysInfo.lpMinimumApplicationAddress;
		addr < (BYTE*)sysInfo.lpMaximumApplicationAddress;
		addr += mem_info.RegionSize) {
		if (!VirtualQueryEx(process, addr, &mem_info, sizeof(mem_info))) {
			printf("Error: VirtualQueryEx");
			return;
		}
		if (i < start) {
			i++;
			continue;
		}
		if (!(mem_info.State & MEM_COMMIT) || !(mem_info.Type & MEM_PRIVATE) || mem_info.Type & MEM_IMAGE ||
			mem_info.Protect & PAGE_GUARD || !(mem_info.Protect & PAGE_READWRITE) || mem_info.Protect & PAGE_NOACCESS) continue;
		process_ptrs->push_back(Mem_info((BYTE*)mem_info.BaseAddress, (DDWORD)mem_info.RegionSize));

		if (end && (i >= end))
			break;
		//printf("%d %p %d\n", i, addr, mem_info.RegionSize);
		i++;
	}
}

vector<string> parse(string str) { // Parse keyboard inputs
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

long long addr2val(BYTE* addr, float type) { // read an address and convert to value
	if (type == 1)
		return *(char*)addr;
	else if (type == 2)
		return *(short*)addr;
	else if (type == 4)
		return *(long*)addr;
	else if (type == 8)
		return *(long long*)addr;
	else if (abs(type - 4.1) < 0.01)
		return *(float*)addr;
	else if (abs(type - 8.1) < 0.01)
		return *(double*)addr;
}

DDWORD addr2uval(BYTE* addr, float type) { // read an address and convert to unsigned value
	if (type == -1)
		return *(unsigned char*)addr;
	else if (type == -2)
		return *(unsigned short*)addr;
	else if (type == -4)
		return *(unsigned long*)addr;
	else if (type == -8)
		return *(DDWORD*)addr;
}

float decide_type(string choice) {
	float type = stof(choice);
	bytes_to_read = (BYTE)abs(type);
	delete[] read_buffer;
	read_buffer = new BYTE[bytes_to_read];
	return type;
}

bool compare_str(BYTE* chars, string str) { // string comparison
	for (short i = 0; i < bytes_to_read; i++) {
		if ((BYTE)chars[i] != (BYTE)str[i])
			return false;
	}
	return true;
}

void detect(float type, bool is_even) {
	get_process_memory(0, 0);
	DDWORD count = 0;
	long long target = 0, limit = 0;
	string target_str;

	if (!filtered) {
		short** prev_values = NULL;
		printf("1: unknown / 2 [target] / 3 [target str] / 0: exit\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		vector<string> choice2;
		if (choice[0] == "1") { // unknown
			// Make prev_values
			prev_values = new short* [process_ptrs->size()];
			for (DWORD i = 0; i < process_ptrs->size(); i++) {
				DDWORD block_size = process_ptrs->at(i).block_size;
				BYTE* block_buffer = new BYTE[block_size];
				if (!ReadProcessMemory(process, process_ptrs->at(i).addr, block_buffer, block_size, NULL)) {
					delete[] block_buffer;
					continue;
				}
				prev_values[i] = new short[block_size / 2 + 1];
				for (DDWORD j = 0; j < block_size - bytes_to_read + 1; j++) {
					// Skip even or odd address
					BYTE remainder = (DDWORD)(process_ptrs->at(i).addr + j) % 2;
					if ((is_even && remainder != 0) || (!is_even && remainder == 0))
						continue;

					prev_values[i][j / 2] = addr2val(block_buffer + j, type);
					count++;
				}
				delete[] block_buffer;
			}
			printf("Found results: %lld\n", count);
			count = 0;

			// Choose a way to compare with prev_values
			printf("1: increased [limit] / 2: decreased [limit]\n");
			string input; getline(cin, input, '\n');
			choice2 = parse(input);
			if (choice2[0] == "1" || choice2[0] == "2")
				limit = stoull(choice2[1]);
			else {
				for (DWORD i = 0; i < process_ptrs->size(); i++)
					delete[] prev_values[i];
				delete[] prev_values;
				return;
			}
		}
		else if (choice[0] == "2") {// target
			target = stoll(choice[1]);
		}
		else if (choice[0] == "3") { // target string
			target_str = choice[1];
			bytes_to_read = target_str.length();
			delete[] read_buffer;
			read_buffer = new BYTE[bytes_to_read];
		}
		else return;

		// Make first filter
		filtered = new vector<Point>;
		for (DWORD i = 0; i < process_ptrs->size(); i++) {
			DDWORD block_size = process_ptrs->at(i).block_size;
			BYTE* block_buffer = new BYTE[block_size];
			if (!ReadProcessMemory(process, process_ptrs->at(i).addr, block_buffer, block_size, NULL)) {
				delete[] block_buffer;
				continue;
			}
			if (prev_values && IsBadReadPtr(*(prev_values + i), 1)) {
				printf("skip %d\n", i);
				continue;
			}
			for (DDWORD j = 0; j < block_size - bytes_to_read + 1; j++) {
				BYTE* addr = process_ptrs->at(i).addr + j;
				// Skip even or odd address
				BYTE remainder = (DDWORD)addr % 2;
				if ((is_even && remainder != 0) || (!is_even && remainder == 0))
					continue;

				if (choice[0] == "3") { // target string
					for (short byte = 0; byte < bytes_to_read; byte++)
						read_buffer[byte] = block_buffer[j + byte];
					if (compare_str(read_buffer, target_str)) {
						filtered->push_back(Point(addr, NULL));
						count++;
						continue;
					}
				}

				long long value = addr2val(block_buffer + j, type);
				if (choice[0] == "1") { // unknown
					short prev_value = prev_values[i][j / 2];
					if ((choice2[0] == "1" && value > prev_value && (value - prev_value) <= limit) ||
						(choice2[0] == "2" && value < prev_value && (prev_value - value) <= limit)) {
							filtered->push_back(Point(addr, value));
							count++;
					}
				}
				else if (choice[0] == "2" && value == target) { // target
						filtered->push_back(Point(addr, value));
						count++;
				}
			}
			delete[] block_buffer;
			if (choice[0] == "1") // unknown
				delete[] prev_values[i];
		}
		if (choice[0] == "1") // unknown
			delete[] prev_values;
		printf("Found results: %lld\n", count);
		count = 0;
	}

	// Keep filtering
	while (1) {
		printf("1: increased [limit] / 2: decreased [limit] / 3: same / 4 [target] / 5: same string\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		if (choice[0] == "1" || choice[0] == "2")
			limit = stoll(choice[1]);
		else if (choice[0] == "3");
		else if (choice[0] == "4")
			target = stoll(choice[1]);
		else if (choice[0] == "5");
		else return;

		vector<Point>* new_filtered = new vector<Point>();
		for (Point point : *filtered) {
			if (!ReadProcessMemory(process, point.addr, read_buffer, bytes_to_read, NULL)) continue;
			long long value = addr2val(read_buffer, type);

			if ((choice[0] == "1" && value > point.value && (value - point.value) <= limit) || // increased
				(choice[0] == "2" && value < point.value && (point.value - value) <= limit) || // decreased
				(choice[0] == "3" && value == point.value) || // same
				(choice[0] == "4" && value == target) || // target
				(choice[0] == "5" && compare_str(read_buffer, target_str))) { // target str
				new_filtered->push_back(Point(point.addr, value));
				count++;
			}
		}
		delete filtered;
		filtered = new_filtered;
		printf("Found results: %d\n", count);
		count = 0;
	}
}

void get_result(float type) {
	FILE* fp; fopen_s(&fp, "G:/Software/Projects/aim_bot_memory/result.txt", "w");
	DWORD i = 0;
	for (Point point : *filtered) {
		if(!ReadProcessMemory(process, point.addr, read_buffer, bytes_to_read, NULL))
			continue;
		long long value;
		DDWORD u_value;
		if (type > 0) {
			value = addr2val(read_buffer, type);
			fprintf(fp, "(%d)addr: %lld val: %lld\n", i, point.addr, value);
			printf("(%d)addr: %lld val: %lld\n", i++, point.addr, value);
		}
		else {
			u_value = addr2uval(read_buffer, type);
			fprintf(fp, "(%d)addr: %lld val: %lld\n", i, point.addr, u_value);
			printf("(%d)addr: %lld val: %lld\n", i++, point.addr, u_value);
		}
	}
	fclose(fp);
}

void read_range(DDWORD addr, short range, float type) {
	FILE* fp; fopen_s(&fp, "G:/Software/Projects/aim_bot_memory/range.txt", "w");
	for (short offset = -range; offset <= range; offset++) {
		if (!ReadProcessMemory(process, (BYTE*)addr + offset, read_buffer, bytes_to_read, NULL))
			continue;

		long long value;
		DDWORD u_value;
		if (type > 0) {
			value = addr2val(read_buffer, type);
			fprintf(fp, "(%d)addr: %lld val: %lld\n", offset, addr + offset, value);
			printf("(%d)addr: %lld val: %lld\n", offset, addr + offset, value);
		}
		else {
			u_value = addr2uval(read_buffer, type);
			fprintf(fp, "(%d)addr: %lld val: %lld\n", offset, addr + offset, u_value);
			printf("(%d)addr: %lld val: %lld\n", offset, addr + offset, u_value);
		}
	}
	fclose(fp);
}

void read_ptr(DDWORD ptr_addr, float type) {
	const DWORD size = 8; // 64bits pointer
	BYTE ptr[size];
	if(!ReadProcessMemory(process, (BYTE*)ptr_addr, ptr, 8, NULL)) return;
	if(!ReadProcessMemory(process, (BYTE*)*(DDWORD*)ptr, read_buffer, bytes_to_read, NULL)) return; // dereferenced value
	printf("Result: %lld\n", addr2val(read_buffer, type));
}

void write(DDWORD addr, string val, float type) {
	if (type == 4) { // float
		float buffer = stof(val);
		if (!WriteProcessMemory(process, (BYTE*)addr, &buffer, bytes_to_read, NULL))
			printf("Error\n");
	}
	else if (type == 8) { // double
		double buffer = stod(val);
		if (!WriteProcessMemory(process, (BYTE*)addr, &buffer, bytes_to_read, NULL))
			printf("Error\n");
	}
	else { // long long
		long long buffer = stoll(val);
		if (!WriteProcessMemory(process, (BYTE*)addr, &buffer, bytes_to_read, NULL))
			printf("Error\n");
	}
}

void change_through(string val, float type) { // Change values iterating filtered addresses
	DWORD i = 0;
	for (Point point : *filtered) {
		printf("(%d)addr %lld changed. Enter", i++, point.addr);
		string input; getline(cin, input, '\n');
		if (input == "0")
			return;
		if (type == 4) {
			float buffer = stof(val);
			if (!WriteProcessMemory(process, (BYTE*)point.addr, &buffer, bytes_to_read, NULL))
				printf("Error\n");
		}
		else if (type == 8) {
			double buffer = stod(val);
			if (!WriteProcessMemory(process, (BYTE*)point.addr, &buffer, bytes_to_read, NULL))
				printf("Error\n");
		}
		else {
			long long buffer = stoll(val);
			if (!WriteProcessMemory(process, (BYTE*)point.addr, &buffer, bytes_to_read, NULL))
				printf("Error\n");
		}
	}
	while (true) {
		printf("0: exit");
		string input; getline(cin, input, '\n');
		if (input == "0")
			return;
	}
}

/*void read_range_str(DDWORD addr, short range, BYTE length) { // read addresses as string type
	FILE* fp; fopen_s(&fp, "G:/Software/Projects/aim_bot_memory/string.txt", "w");
	BYTE* buffer = new BYTE[length];
	for (short offset = -range; offset <= range; offset++) {
		if (!ReadProcessMemory(process, (BYTE*)addr + offset, buffer, length, NULL))
			continue;

		fprintf(fp, "(%d)addr: %lld val: %s\n", offset, addr + offset, buffer);
		printf("(%d)addr: %lld val: %s\n", offset, addr + offset, buffer);
	}
	fclose(fp);
}*/

void main() {
	printf("1: Minecraft* 1.19.3 - Singleplayer\n2: *Untitled - Notepad\n3: Overwatch\n4: League of Legends (TM) Client\n0: Manual input [pID]\n");
	string input; getline(cin, input, '\n');
	vector<string> choice = parse(input);
	HWND hWindow = NULL;
	if (choice[0] == "0")
		pID = stoul(choice[1]);
	else {
		if (choice[0] == "1")
			hWindow = FindWindow(NULL, "Minecraft* 1.19.3 - Singleplayer");
		else if (choice[0] == "2")
			hWindow = FindWindow(NULL, "*Untitled - Notepad");
		else if (choice[0] == "3")
			hWindow = FindWindow(NULL, "Overwatch");
		else if (choice[0] == "4")
			hWindow = FindWindow(NULL, "League of Legends (TM) Client");
		GetWindowThreadProcessId(hWindow, &pID);
	}
	process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
	if (!process) {
		fprintf(stderr, "[-] ERROR: Couldn't open process %lu. OpenProcess failed with error: %lu.\n", pID, GetLastError());
		return;
	}
	printf("Process %d opened\n", pID);

	float type = 1;
	read_buffer = new BYTE[1];
	//attach_debugger(process, pID, (DWORD_PTR)915943717920);
	while (true) {
		printf("1 [type]: set type / 2: detect even / 3: detect odd\n");
		printf("4: get result / 5 [addr] [range]: read range / 6 [addr] [val]: write\n");
		printf("7 [val]: change through / 8 [ptr_addr]: read ptr\n");
		string input; getline(cin, input, '\n');
		vector<string> choice = parse(input);
		if (choice[0] == "1")
			type = decide_type(choice[1]);
		else if (choice[0] == "2")
			detect(type, true);
		else if (choice[0] == "3")
			detect(type, false);
		else if (choice[0] == "4")
			get_result(type);
		else if (choice[0] == "5")
			read_range(stoull(choice[1]), stoi(choice[2]), type);
		else if (choice[0] == "6")
			write(stoull(choice[1]), choice[2], type);
		else if (choice[0] == "7")
			change_through(choice[1], type);
		else if (choice[0] == "8")	
			read_ptr(stoull(choice[1]), type);
		else return;
	}
}