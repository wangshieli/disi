#include "../ShareApi/ShareApi.h"

#include "PPPOEDial.h"

BOOL FindRasphone()
{
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hProcessSnap)
	{
		printf_s("创建系统进程映射失败 error = %d\n", GetLastError());
		return FALSE;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);

	BOOL bFind = Process32First(hProcessSnap, &pe32);
	while (bFind)
	{
		if (0 == strcmp(pe32.szExeFile, "rasphone.exe"))
		{
			CloseHandle(hProcessSnap);
			return TRUE;
		}
		bFind = Process32Next(hProcessSnap, &pe32);
	}

	CloseHandle(hProcessSnap);
	return FALSE;
}

BOOL DoAdsl()
{
	while (true)
	{
		CloseRasphone();
		WinExec("rasphone -d adsl", SW_NORMAL);

		Sleep(1000 * 4);
		HWND hAdsl = NULL;
		hAdsl = FindWindow(NULL, "连接 adsl");
		if (NULL == hAdsl)
			continue;

		HWND hLinkButton = NULL;
		hLinkButton = FindWindowEx(hAdsl, NULL, "Button", "连接(&C)");
		if (NULL == hLinkButton)
			continue;
		SendMessage(hLinkButton, BM_CLICK, 0, 0);

		BOOL bContinue = FALSE;
		DWORD dwWaitTime = 0;
		HWND hLinking = NULL;
		do
		{
			Sleep(1000 * 2);
			dwWaitTime += 2;
			hLinking = FindWindow(NULL, "正在连接 adsl...");
			if (NULL == hLinking)
			{
				if (FindRasphone())
				{
					bContinue = TRUE;
					break;
				}
			}
			else
			{
				HWND hErrorWnd = NULL;
				hErrorWnd = FindWindow(NULL, "连接到 adsl 时出错");
				if (NULL != hErrorWnd)
				{
					bContinue = TRUE;
					break;
				}
			}
		} while (NULL != hLinking && dwWaitTime < 90);

		if (bContinue)
			continue;

		return TRUE;
	}

	return TRUE;
}

HANDLE hRedialingEvent = NULL;
HANDLE hRedialStartEvent = NULL;
HANDLE hRedialCompEvent = NULL;
HANDLE hRedialThreadStart = NULL;

unsigned int _stdcall redial_thread(LPVOID pVoid)
{
	hRedialingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	hRedialStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hRedialCompEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	SetEvent(hRedialThreadStart);

	while (WaitForSingleObject(hRedialStartEvent, INFINITE) != WAIT_FAILED)
	{
		while (true)
		{
			char oldIP[16] = { 0 };
			if (!GetAdslInfo(oldIP))
				memcpy(oldIP, "0.0.0.0", strlen("0.0.0.0"));

#ifdef PROXY_DEBUG
			printf("测试不拨号\n");
#else
			DoAdsl();
#endif // PROXY_DEBUG

			char newIP[16] = { 0 };
#ifdef PROXY_DEBUG
			memcpy(newIP, "192.168.2.150", strlen("192.168.2.150"));
#else
			if (!GetAdslInfo(newIP))
				continue;
#endif // PROXY_DEBUG
			printf("%s\n", newIP);

#ifdef PROXY_DEBUG
			printf("测试不验证ip\n");
#else
			if (0 == strcmp(newIP, "") || 0 == strcmp(newIP, "0.0.0.0"))
				continue;

			if (0 == strcmp(newIP, oldIP))
				continue;
#endif // PROXY_DEBUG

			memcpy(g_AdslIp, newIP, 16);

			break;
		}
		
		SetEvent(hRedialCompEvent);
	}
	printf("redial_thread exit error: %d\n", GetLastError());
	return 0;
}

#ifndef USE_PPPOE_DIAL
int main()
{
	hRedialingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	hRedialStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hRedialCompEvent = CreateEvent(NULL, FALSE, FALSE , NULL);
	getchar();

	DoAdsl();
	char ip[16] = { 0 };
	GetAdslInfo(ip);

	printf("%s\n", ip);

	getchar();
	return 0;
}
#endif // !USE_PPPOE_DIAL