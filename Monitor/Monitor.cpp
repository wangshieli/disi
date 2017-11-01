#include "../ShareApi/ShareApi.h"
#include "../RegistryOperation/RegistryOperation.h"
#include "../CurlClient/CurlClient.h"
#include "../Proxy/cJSON.h"
#include "../Proxy/md5.h"

//#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup")

#define VERSION "0.0.1"

BOOL RegselfInfo()
{
	HKEY hKey = NULL;
	if (!PRegCreateKey(SMonitor, &hKey))
		return FALSE;

	char filepath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, filepath, MAX_PATH);
	if (!SetRegValue(hKey, "path", filepath))
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	if (!SetRegValue(hKey, "version", VERSION))
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);

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

BOOL CheckMd5(const char* pFilePath, const char* pMd5)
{
	FILE* fptr = NULL;
	fopen_s(&fptr, pFilePath, "rb");
	if (NULL == fptr)
		return FALSE;

	fclose(fptr);
	string md5value = MD5(ifstream(pFilePath, ios::binary)).toString();
	if (strcmp(md5value.c_str(), pMd5) == 0)
		return TRUE;

	DeleteFile(pFilePath);
	return FALSE;;
}

BOOL doDownLoad(const char* path, const char* pUrl, const char* pMd5)
{
	do
	{
		if (!Curl_DownloadFile(pUrl, path))
			return FALSE;
	} while (!CheckMd5(path, pMd5));

	return TRUE;
}

BOOL Proxy(cJSON** pAppInfo)
{
	char* pVersion = cJSON_GetObjectItem(pAppInfo[0], "version")->valuestring;
	if (CompareVersion(VERSION, pVersion))
		return TRUE;

	char szProxyPath[MAX_PATH];
	char szUpgradePath[MAX_PATH];
	char szTempPath[MAX_PATH];
	int nTempLen = GetTempPath(MAX_PATH, szTempPath);
	_makepath_s(szProxyPath, NULL, szTempPath, "monitor.exe", NULL);
	_makepath_s(szUpgradePath, NULL, szTempPath, "upgrade.exe", NULL);

	char* pProxyUrl = cJSON_GetObjectItem(pAppInfo[0], "update_url")->valuestring;
	char* pUpgradeUrl = cJSON_GetObjectItem(pAppInfo[1], "update_url")->valuestring;
	if (NULL == pProxyUrl || NULL == pUpgradeUrl)
		return FALSE;

	char szProxyUrl[MAX_PATH] = { 0 };
	char szUpgradeUrl[MAX_PATH] = { 0 };
	UrlFormating(pUpgradeUrl, "\\", szUpgradeUrl);
	UrlFormating(pProxyUrl, "\\", szProxyUrl);

	char* pProxyMd5 = cJSON_GetObjectItem(pAppInfo[0], "ext_md5")->valuestring;
	char* pUpgradeMd5 = cJSON_GetObjectItem(pAppInfo[1], "ext_md5")->valuestring;
	if (NULL == pProxyMd5 || NULL == pUpgradeMd5)
		return FALSE;

	if (!doDownLoad(szProxyPath, szProxyUrl, pProxyMd5))
		return FALSE;

	if (!doDownLoad(szUpgradePath, szUpgradeUrl, pUpgradeMd5))
		return FALSE;

	char CurrentProxyPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, CurrentProxyPath, MAX_PATH);
	DWORD nCurrentProxyPid = GetCurrentProcessId();

	char Param[1024] = { 0 };
	char* pParamTemp = "%d \"%s\" \"%s\"";
	sprintf_s(Param, pParamTemp, nCurrentProxyPid, szProxyPath, CurrentProxyPath);

	ShellExecute(0, "open", szUpgradePath, Param, NULL, SW_SHOWNORMAL);

	return TRUE;
}

void _stdcall ontimer_checkversion(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
	if (NULL == pResponseData)
	{
		printf("内存分配失败\n");
		return;
	}
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		printf("内存分配失败\n");
		free(pResponseData);
		return;
	}

	cJSON* root = NULL;
	pResponseData->dwDataLen = 0;
	if (!Curl_GetProxyVersionInfo(g_pClient_id, pResponseData))
	{
		printf("获取版本信息失败\n");
		goto error;
	}
	printf("GET请求返回的信息: %s\n", pResponseData->pData);

	// 解析数据
	root = cJSON_Parse(pResponseData->pData);
	if (NULL == root)
	{
		printf("格式化json对象失败\n");
		goto error;
	}

	cJSON* cjSuccess = cJSON_GetObjectItem(root, "success");
	if (NULL == cjSuccess)
	{
		printf("接收到的数据中没有 success 字段\n");
		goto error;
	}
	if (0 == cjSuccess->valueint)
	{
		printf("上传数据中有错误数据\n");
		goto error;
	}

	cJSON* cjArray = cJSON_GetObjectItem(root, "data");
	if (NULL == cjArray)
	{
		printf("接收到的数据中没有 data 字段\n");
		goto error;
	}

	int nArraySize = cJSON_GetArraySize(cjArray);
	if (nArraySize < 2)
	{
		printf("返回的数据中没有需要的 2 个exe信息 = %d\n", nArraySize);
		goto error;
	}

	cJSON* app_info[2];
	app_info[0] = cJSON_GetObjectItem(cjArray, "proxy-monitor");
	if (NULL == app_info[0])
	{
		printf("获取 proxy-monitor 版本信息失败\n");
		goto error;
	}
	app_info[1] = cJSON_GetObjectItem(cjArray, "app-upgrade");
	if (NULL == app_info[1])
	{
		printf("获取 app-upgrade 版本信息失败\n");
		goto error;
	}

	if (!Proxy(app_info))
	{
		printf("升级代理失败\n");
		goto error;
	}

error:
	SetEvent(g_hDoingNetWork);
	if (NULL != root)
		cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
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

	if (!RegselfInfo())
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