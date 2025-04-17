#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <microhttpd.h>

#define PORT 8080
#define MAX_RESPONSE_SIZE 1048576
// get process information
void getProcessInfo(PROCESSENTRY32 pe32, char *buffer, size_t buffer_size)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
    if (hProcess == NULL)
    {
        return;
    }
    // get priority
    DWORD priority = GetPriorityClass(hProcess);
    // get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    SIZE_T memoryUsage = 0;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
    {
        memoryUsage = pmc.WorkingSetSize / 1024; // Convert to KB
    }
    FILETIME createTime, exitTime, kernelTime, userTime;
    ULONGLONG totalTime = 0;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime))
    {
        ULARGE_INTEGER k, u;
        k.LowPart = kernelTime.dwLowDateTime;
        k.HighPart = kernelTime.dwHighDateTime;
        u.LowPart = userTime.dwLowDateTime;
        u.HighPart = userTime.dwHighDateTime;
        totalTime = (k.QuadPart + u.QuadPart) / 10000; // Convert to milliseconds)
    }
    // add values to buffer
    char line[512];
    snprintf(line, sizeof(line), "PID: %-6lu Priority: %-10lu Memory: %-10zu KB CPU Time: %-10llu Name: %s\n",
             pe32.th32ProcessID, priority, memoryUsage, totalTime, pe32.szExeFile);
    strncat(buffer, line, buffer_size - strlen(buffer) - 1);
    CloseHandle(hProcess);
}

// get process list
char *getAllProcess()
{
    char *response = malloc(MAX_RESPONSE_SIZE);
    strcpy(response, "List of processes:\n");
    strcat(response, "PID\tPriority\tMemory\tCPU Time\tName\n");
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        strcat(response, "Error: Unable to create snapshot\n");
        return response;
    }
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hSnapshot, &pe32))
    {
        CloseHandle(hSnapshot);
        strcat(response, "Error: Unable to get first process\n");
        return response;
    }
    do
    {
        getProcessInfo(pe32, response, MAX_RESPONSE_SIZE);
    } while (Process32Next(hSnapshot, &pe32));
    CloseHandle(hSnapshot);
    return response;
}

// kill process following PID
char *closeProcess(const char *processName)
{
    char *response = malloc(MAX_RESPONSE_SIZE);
    strcpy(response, " ");

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        strcat(response, "Error: Unable to create snapshot\n");
        return response;
    }
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    int terminated = 0;
    if (Process32First(hSnapshot, &pe))
    {
        do
        {
            if (strcmp(pe.szExeFile, processName) == 0)
            {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess)
                {
                    if (TerminateProcess(hProcess, 0))
                    {
                        char line[256];
                        snprintf(line, sizeof(line), "close process: %s (PID : %lu)\n", pe.szExeFile, pe.th32ProcessID);
                        strcat(response, line);
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    if (terminated == 0)
    {
        strcat(response, "Error: Unable to find process\n");
    }
    CloseHandle(hSnapshot);
    return response;
}

// handle request http
static enum MHD_Result requestHandle(void *cls, struct MHD_Connection *connection, const char *url,
                                     const char *method, const char *version, const char *upload_data, size_t *upload_data_size,
                                     void **ptr)
{
    static int dummy;
    if (&dummy != *ptr)
    {
        *ptr = &dummy;
        return MHD_YES;
    }
    struct MHD_Response *response;
    enum MHD_Result ret;
    char *content;

    // route show process
    if (strcmp(url, "/show_all_process") == 0 && strcmp(method, "GET") == 0)
    {
        content = getAllProcess();
        response = MHD_create_response_from_buffer(strlen(content), (void *)content, MHD_RESPMEM_MUST_FREE);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    // route kill process
    if (strncmp(url, "/kill_process/", 14) == 0 && strcmp(method, "POST") == 0)
    {
        const char *processName = url + 14;
        if (strlen(processName) == 0)
        {
            content = strdup("invalid process name\n");
            response = MHD_create_response_from_buffer(strlen(content), content, MHD_RESPMEM_MUST_FREE);
            ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
            MHD_destroy_response(response);
            return ret;
        }
        content = closeProcess(processName);
        response = MHD_create_response_from_buffer(strlen(content), content, MHD_RESPMEM_MUST_FREE);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
    // route not found
    content = strdup("404 toi ne cac ban toi oi\n");
    response = MHD_create_response_from_buffer(strlen(content), content, MHD_RESPMEM_MUST_FREE);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed\n");
        return 1;
    }
    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &requestHandle, NULL, MHD_OPTION_END);
    if (!daemon)
    {
        printf("Failed to start daemon\n");
        return 1;
    }
    printf("Server running on port %d\n", PORT);
    printf("Try: curl http://localhost:%d/show_all_process\n", PORT);
    printf("Try: curl -X POST http://localhost:%d/kill_process/<process_name>\n", PORT);

    getchar();
    MHD_stop_daemon(daemon);
    WSACleanup();

    return 0;
}