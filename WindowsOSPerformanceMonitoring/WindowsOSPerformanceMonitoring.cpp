#include "../ShareApi/ShareApi.h"
#include <Psapi.h>
#include <assert.h>
#include "WindowsOSPerformanceMonitoring.h"

#pragma comment(lib, "Psapi.lib")

#define SIZE_LOG_NAME			128

FILE* fp;
SYSTEMTIME CurSysTime;
char szLogFileName[SIZE_LOG_NAME];
char szCurrentTime[SIZE_LOG_NAME];

//void PrintMemoryInfo(DWORD processID)
//{
//	HANDLE hProcess;
//	PROCESS_MEMORY_COUNTERS pmc;
//
//	printf("The Process ID: %u\n", processID);
//
//	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
//		PROCESS_VM_READ,
//		FALSE, processID);
//	if (NULL == hProcess)
//		return;
//
//	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
//	{
//		//printf("\tPageFaultCount: 0x%08X\n", pmc.PageFaultCount);
//		//printf("\tPeakWorkingSetSize: 0x%08X\n",
//		//	pmc.PeakWorkingSetSize);
//		//printf("\tWorkingSetSize: 0x%08X\n", pmc.WorkingSetSize);
//		//printf("\tQuotaPeakPagedPoolUsage: 0x%08X\n",
//		//	pmc.QuotaPeakPagedPoolUsage);
//		//printf("\tQuotaPagedPoolUsage: 0x%08X\n",
//		//	pmc.QuotaPagedPoolUsage);
//		//printf("\tQuotaPeakNonPagedPoolUsage: 0x%08X\n",
//		//	pmc.QuotaPeakNonPagedPoolUsage);
//		//printf("\tQuotaNonPagedPoolUsage: 0x%08X\n",
//		//	pmc.QuotaNonPagedPoolUsage);
//		//printf("\tPagefileUsage: 0x%08X\n", pmc.PagefileUsage);
//		//printf("\tPeakPagefileUsage: 0x%08X\n",
//		//	pmc.PeakPagefileUsage);
////		printf("\t页面错误的数量: %d\n", pmc.PageFaultCount / 1024);// 页面错误的数量
//		printf("\t峰值工作集: %d\n",
//			pmc.PeakWorkingSetSize / 1024);// 峰值工作集大小(字节为单位)
//		printf("\t工作集: %d\n", pmc.WorkingSetSize / 1024);// 当前工作集大小 -
//		printf("\t峰值分页缓冲池: %d\n",
//			pmc.QuotaPeakPagedPoolUsage / 1024);// 峰值分页池使用率 -
//		printf("\t分页缓冲池: %d\n",
//			pmc.QuotaPagedPoolUsage / 1024);// 当前分页池使用率 -
//		printf("\t峰值非分页缓冲池: %d\n",
//			pmc.QuotaPeakNonPagedPoolUsage / 1024);// 峰值非分页池使用率 -
//		printf("\t非分页缓冲池: %d\n",
//			pmc.QuotaNonPagedPoolUsage / 1024);// 当前非分页池使用率 -
//		//printf("\tPagefileUsage: %d\n", pmc.PagefileUsage / 1024);// 此进程的提交费用值（以字节为单位）。提交费用是内存管理器为正在运行的进程提交的内存总量
//		//printf("\tPeakPagefileUsage: %d\n",
//		//	pmc.PeakPagefileUsage / 1024);// 在这个过程的生命周期中，Commit Charge的字节的峰值
//	}
//	CloseHandle(hProcess);
//}

#define DIV 1024
#define WIDTH 7

void PCMemoryInfo()
{
	MEMORYSTATUSEX statex;

	statex.dwLength = sizeof(statex);

	GlobalMemoryStatusEx(&statex);

	printf("There is  %*ld percent of memory in use.\n",
		WIDTH, statex.dwMemoryLoad);
	printf("There are %*I64d total KB of physical memory.\n",
		WIDTH, statex.ullTotalPhys / DIV);
	printf("There are %*I64d free  KB of physical memory.\n",
		WIDTH, statex.ullAvailPhys / DIV);
	printf("There are %*I64d total KB of paging file.\n",
		WIDTH, statex.ullTotalPageFile / DIV);
	printf("There are %*I64d free  KB of paging file.\n",
		WIDTH, statex.ullAvailPageFile / DIV);
	printf("There are %*I64d total KB of virtual memory.\n",
		WIDTH, statex.ullTotalVirtual / DIV);
	printf("There are %*I64d free  KB of virtual memory.\n",
		WIDTH, statex.ullAvailVirtual / DIV);

	// Show the amount of extended memory available.

	printf(TEXT("There are %*I64d free  KB of extended memory.\n"),
		WIDTH, statex.ullAvailExtendedVirtual / DIV);
}

/// 时间转换
//static uint64_t file_time_2_utc(const FILETIME* ftime)
//{
//	LARGE_INTEGER li;
//
//	assert(ftime);
//	li.LowPart = ftime->dwLowDateTime;
//	li.HighPart = ftime->dwHighDateTime;
//
//	return li.QuadPart;
//}
//
///// 获得CPU的核数 
//static int get_processor_number()
//{
//	SYSTEM_INFO info;
//	GetSystemInfo(&info);
//	return (int)info.dwNumberOfProcessors;
//}

int get_memory_usage(HANDLE hProcess, uint64_t* mem, uint64_t* vmem, uint64_t* qp, uint64_t* qnp)
{
	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(/*GetCurrentProcess()*/hProcess, &pmc, sizeof(pmc)))
	{
		if (mem) *mem = pmc.WorkingSetSize;
		if (vmem) *vmem = pmc.PagefileUsage;
		if (qp) *qp = pmc.QuotaPagedPoolUsage;
		if (qnp) *qnp = pmc.QuotaNonPagedPoolUsage;
		return 0;
	}
	return -1;
}

int get_io_bytes(HANDLE hProcess, uint64_t* read_bytes, uint64_t* write_bytes)
{
	IO_COUNTERS io_counter;
	if (GetProcessIoCounters(/*GetCurrentProcess()*/hProcess, &io_counter))
	{
		if (read_bytes) *read_bytes = io_counter.ReadTransferCount;
		if (write_bytes) *write_bytes = io_counter.WriteTransferCount;
		return 0;
	}
	return -1;
}

void LogProcessCpuMemInfo()
{
	char* pArray[3] = { 0 };
	pArray[0] = "chrome.exe";
	pArray[1] = "proxy2-v";
	pArray[2] = "controller-v";
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hProcessSnap)
		return;

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);

	SYSTEMTIME MsgSysTime;
	GetLocalTime(&MsgSysTime);

	if (CurSysTime.wDay != MsgSysTime.wDay)
	{
		fclose(fp);

		if (MsgSysTime.wDayOfWeek == 0)// 0星期日 1星期一
		{
			char CurrentProxyPath[MAX_PATH] = { 0 };
			GetModuleFileName(NULL, CurrentProxyPath, MAX_PATH);
			char PanName[_MAX_FNAME] = { 0 };
			char FileName[_MAX_FNAME] = { 0 };
			_splitpath_s(CurrentProxyPath, PanName, _MAX_FNAME, FileName, _MAX_FNAME, NULL, 0, NULL, 0);
			char szPath[MAX_PATH] = { 0 };
			sprintf_s(szPath, "%s%sLOG", PanName, FileName);
			DeleteDirectoryByFullName(szPath);
			Sleep(1000 * 5);
			CreateDirectory("LOG", NULL);
		}

		CurSysTime = MsgSysTime;
		sprintf_s(szLogFileName, "LOG\\%04d-%02d-%02d.txt",
			CurSysTime.wYear,
			CurSysTime.wMonth,
			CurSysTime.wDay);
		fopen_s(&fp, szLogFileName, "a+b");
	}
	sprintf_s(szCurrentTime, "%04d-%02d-%02d %02d:%02d:%02d\r\n",
		MsgSysTime.wYear,
		MsgSysTime.wMonth,
		MsgSysTime.wDay,
		MsgSysTime.wHour,
		MsgSysTime.wMinute,
		MsgSysTime.wSecond);
	fputs(szCurrentTime, fp);
	fflush(fp);

	BOOL bFind = Process32First(hProcessSnap, &pe32);
	while (bFind)
	{
		int i = 0;
		do
		{
			if (NULL != strstr(pe32.szExeFile, pArray[i]))
			{
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
					PROCESS_VM_READ/*PROCESS_ALL_ACCESS*/,
					FALSE, pe32.th32ProcessID);

				if (NULL == hProcess)
					break;

				uint64_t mem, vmem, qp, qnp/*, r, w*/;

				get_memory_usage(hProcess, &mem, &vmem, &qp, &qnp);
				//get_io_bytes(hProcess, &r, &w);
				CPUusage usg(pe32.th32ProcessID);
				float cpu = usg.get_cpu_usage();
				Sleep(500 * 2);
				cpu = usg.get_cpu_usage();
				
				char cLog[256] = { 0 };
				char* pTemp = "%s,cpu:%.2f%%,mem:%u kb,qpm:%u kb,qnpm:%u kb";
				sprintf_s(cLog, pTemp, pArray[i], cpu, mem / 1024, qp / 1024, qnp / 1024);
				//printf("%s CPU使用率: %.2f%%\n", pArray[i], cpu);
				//printf("%s 内存使用: %u K\n", pArray[i], mem / 1024);
				//printf("%s 虚拟内存使用: %u K\n", pArray[i], vmem / 1024);
				//printf("%s 总共读: %u 字节\n", pArray[i], r);
				//printf("%s 总共写: %u 字节\n", pArray[i], w);

				fputs(cLog, fp);
				fputs("\r\n", fp);
				fflush(fp);

				CloseHandle(hProcess);
				break;
			}
			i++;
		} while (i < 3);

		bFind = Process32Next(hProcessSnap, &pe32);
	}

	CloseHandle(hProcessSnap);
}

BOOL EnableDebugPrivilege()
{
	HANDLE hToken;
	BOOL bOK = FALSE;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
		{
			printf("cannt lookup");
		}

		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL))
		{
			printf("adjust error\n");
		}

		bOK = (GetLastError() == ERROR_SUCCESS);
	}
	CloseHandle(hToken);
	return bOK;
}

unsigned int _stdcall information_collection(LPVOID pVoid)
{
//	EnableDebugPrivilege();
	HANDLE hPCInfoEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	DWORD FileAttr;

	if ((FileAttr = GetFileAttributes("LOG")) == 0xffffffff)
		CreateDirectory("LOG", NULL);

	GetLocalTime(&CurSysTime);

	sprintf_s(szLogFileName, "LOG\\%04d-%02d-%02d.txt",
		CurSysTime.wYear, CurSysTime.wMonth, CurSysTime.wDay);
	fopen_s(&fp, szLogFileName, "a + b");

	if (NULL == fp)
	{
		printf("日志记录线程启动失败\n");
		return 0;
	}

	do
	{
		LogProcessCpuMemInfo();
	} while (WaitForSingleObject(hPCInfoEvent, 1000 * 60) == WAIT_TIMEOUT);

	return 0;
}

#ifndef USE_WOPM
int main()
{
	_beginthreadex(NULL, 0, information_collection, NULL, 0, NULL);
	return 0;
}
#endif // !USE_WOPM


float CPUusage::get_cpu_usage()
{
	FILETIME now;
	FILETIME creation_time;
	FILETIME exit_time;
	FILETIME kernel_time;
	FILETIME user_time;
	int64_t system_time;
	int64_t time;
	int64_t system_time_delta;
	int64_t time_delta;

	DWORD exitcode;

	float cpu = -1;

	if (!_hProcess) return -1;

	GetSystemTimeAsFileTime(&now);

	//判断进程是否已经退出
	GetExitCodeProcess(_hProcess, &exitcode);
	if (exitcode != STILL_ACTIVE) {
		clear();
		return -1;
	}

	//计算占用CPU的百分比
	if (!GetProcessTimes(_hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
	{
		clear();
		return -1;
	}
	system_time = (file_time_2_utc(&kernel_time) + file_time_2_utc(&user_time))
		/ _processor;
	time = file_time_2_utc(&now);

	//判断是否为首次计算
	if ((_last_system_time == 0) || (_last_time == 0))
	{
		_last_system_time = system_time;
		_last_time = time;
		return -2;
	}

	system_time_delta = system_time - _last_system_time;
	time_delta = time - _last_time;

	if (time_delta == 0) {
		return -1;
	}

	cpu = (float)system_time_delta * 100 / (float)time_delta;
	_last_system_time = system_time;
	_last_time = time;
	return cpu;
}

uint64_t CPUusage::file_time_2_utc(const FILETIME* ftime)
{
	LARGE_INTEGER li;

	li.LowPart = ftime->dwLowDateTime;
	li.HighPart = ftime->dwHighDateTime;
	return li.QuadPart;
}

int CPUusage::get_processor_number()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}