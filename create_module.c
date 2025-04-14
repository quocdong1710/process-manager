#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>
#include <psapi.h>

void processInfo(PROCESSENTRY32 pe32) {
    // open process
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
    if (hProcess == NULL)
        return;

    // get priority
    DWORD priority = GetPriorityClass(hProcess);

    // memory
    PROCESS_MEMORY_COUNTERS pmc;
    SIZE_T memoryUsage = 0;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        memoryUsage = pmc.WorkingSetSize / 1024;

    // runtime
    FILETIME createTime, exitTime, kernelTime, userTime;
    ULONGLONG totalTime = 0;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernelTime.dwLowDateTime;
        k.HighPart = kernelTime.dwHighDateTime;
        u.LowPart = userTime.dwLowDateTime;
        u.HighPart = userTime.dwHighDateTime;
        totalTime = (k.QuadPart + u.QuadPart) / 10000000;
    }

    // PID, Priority, Memory , CPU Time , name process
    printf("%-6u %-10u %-10zu KB %-10llu %s\n",
        pe32.th32ProcessID,
        priority,
        memoryUsage,
        totalTime,
        pe32.szExeFile
    );
	

    CloseHandle(hProcess);
}

int main() {
    HANDLE hSnap;
    PROCESSENTRY32 pe32;

    // snap shot
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        printf("fail create snap\n");
        return 1;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hSnap, &pe32)) {
        CloseHandle(hSnap);
        printf("fail create first process\n");
        return 1;
    }
    printf("%-6s %-10s %-14s %-12s %s\n", "PID", "Priority", "Memory", "CPU Time", "Process Name");

    // traversal all process
    do {
        processInfo(pe32);
    } while (Process32Next(hSnap, &pe32));

    CloseHandle(hSnap);
    return 0;
}
