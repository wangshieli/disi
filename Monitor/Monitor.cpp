#include "../ShareApi/ShareApi.h"
#include "../RegistryOperation/RegistryOperation.h"
#include "../CurlClient/CurlClient.h"

//#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")

BOOL RegMsconfig()
{
	char filepath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, filepath, MAX_PATH);
	HKEY hKey = NULL;
	if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
		0, KEY_ALL_ACCESS, &hKey))
	{
		if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
			0, KEY_ALL_ACCESS, &hKey))
			return FALSE;
	}

	if (!SetRegValue(hKey, "pmonitor", filepath))
	{
		printf("设置开机启动项失败\n");
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);
	return TRUE;
}

void _stdcall ontimer_checkversion(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	ProcessAutoUpdate("proxy-monitor", "monitor.exe");
}

unsigned int _stdcall monitor_ontimer(LPVOID pVoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	SetTimer(NULL, 1, 1000 * 60 * 10, ontimer_checkversion);

	while (GetMessage(&msg, 0, 0, 0))
	{
		if (msg.message == WM_TIMER)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

int main()
{
	HANDLE hWaitForExplorer = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	WaitForSingleObject(hWaitForExplorer, 1000 * 5);
	CloseTheSpecifiedProcess("explorer.exe");
	WaitForSingleObject(hWaitForExplorer, 1000 * 5);
	WinExec("explorer.exe", SW_NORMAL);
	WaitForSingleObject(hWaitForExplorer, 1000 * 3);
	WinExec("rundll32.exe user32.dll LockWorkStation", SW_NORMAL);

	if (!RegselfInfo(SMonitor) && RegMsconfig())
		return 0;

	if (!GetClient_id(&g_pClient_id))
		return 0;

	HANDLE hTimer = (HANDLE)_beginthreadex(NULL, 0, monitor_ontimer, NULL, 0, NULL);

	do
	{
		BOOL bFind = FALSE;
		HKEY hKey = NULL;
		char manage_path[MAX_PATH];
		if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, ManageModule, 0, KEY_ALL_ACCESS, &hKey))
			continue;

		if (!GetRegValue(hKey, "path", manage_path))
		{
			RegCloseKey(hKey);
			continue;
		}
		RegCloseKey(hKey);

		char FileName[_MAX_FNAME] = { 0 };
		_splitpath_s(manage_path, NULL, 0, NULL, 0, FileName, _MAX_FNAME, NULL, 0);
		strcat_s(FileName, ".exe");

		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (INVALID_HANDLE_VALUE == hProcessSnap)
		{
			printf_s("创建 \"%s\" 系统进程映射失败 error = %d\n", FileName, GetLastError());
			return FALSE;
		}

		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(pe32);

		BOOL bMode = Process32First(hProcessSnap, &pe32);
		while (bMode)
		{
			if (0 == strcmp(pe32.szExeFile, FileName))
			{
				bFind = TRUE;
				break;
			}
			bMode = Process32Next(hProcessSnap, &pe32);
		}

		CloseHandle(hProcessSnap);
		if (!bFind)
			ShellExecute(0, "open", manage_path, NULL, NULL, SW_SHOWNORMAL);

	} while (WAIT_OBJECT_0 != WaitForSingleObject(hWaitForExplorer, 1000 * 60 * 2));

	getchar();
	return 0;
}