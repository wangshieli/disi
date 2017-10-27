#include "Proxy.h"
#include "PSock.h"

#include "../PPPOE_Dial/PPPOEDial.h"
#include "../CurlClient/CurlClient.h"

HANDLE hCompPort = NULL;
DWORD g_dwPageSize = 0;

unsigned int _stdcall completionRoution(LPVOID);
unsigned int _stdcall mode1(LPVOID pVoid);
unsigned int _stdcall mode2(LPVOID pVoid);
unsigned int _stdcall mode2_6086(LPVOID pVoid);

unsigned int _stdcall switch_thread(LPVOID pVoid);

unsigned int _stdcall ontimer_thread(void* pVoid);

void _stdcall ontimer_checkversion(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);

LISTEN_OBJ* g_lobj = NULL;
LISTEN_OBJ* g_lobj6086 = NULL;

SOCKET g_5001socket = INVALID_SOCKET;
SOCKET g_6086socket = INVALID_SOCKET;

HANDLE g_hMode2ThreadStart = NULL;

#ifdef _DEBUG
CRITICAL_SECTION g_csLog;
#endif // _DEBUG

int main()
{
	printf("Current Version: %s\n", PROXY_VERSION);
	SOCKET sListen = INVALID_SOCKET;

	vector<BUFFER_OBJ*> vFreeBuffer;

	if (!GetClient_id(&g_pClient_id))
		goto error;
	if (NULL == g_pClient_id || strcmp(g_pClient_id, "") == 0)
	{
		printf("读取client_id.txt错误，确定是否配置此文件\n");
		Sleep(1000 * 60);
		goto error;
	}

	if (!Init())
		goto error;

	hCompPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	if (NULL == hCompPort)
	{
		printf("CreateIoCompletionPort failed with error: %s\n", WSAGetLastError());
		goto error;
	}

	SYSTEM_INFO sys;
	GetSystemInfo(&sys);
	DWORD dwNumberOfCPU = sys.dwNumberOfProcessors;
	g_dwPageSize = sys.dwPageSize;

#ifdef _DEBUG
	printf("dwNumberOfProcessors = %d\n", sys.dwNumberOfProcessors);
	printf("dwPageSize = %d\n", sys.dwPageSize);
	printf("SIZE_OF_BUFFER_OBJ = %d\n", SIZE_OF_BUFFER_OBJ);
	printf("SIZE_OF_BUFFER_OBJ_T = %d\n", SIZE_OF_BUFFER_OBJ_T);
#endif // _DEBUG

	for (DWORD i = 0; i < dwNumberOfCPU; i++)
	{
		HANDLE hCompThread = (HANDLE)_beginthreadex(NULL, 0, completionRoution, NULL, 0, NULL);
		if (NULL != hCompThread)
			CloseHandle(hCompThread);
	}

	InitializeCriticalSection(&g_csSObj);
	InitializeCriticalSection(&g_csBObj);

	g_hMode2ThreadStart = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef _DEBUG
	InitializeCriticalSection(&g_csLog);
#endif // _DEBUG

	g_hDoingNetWork = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (NULL == g_hDoingNetWork)
	{
		printf("g_hDoingNetWork error: %d\n", GetLastError());
		goto error;
	}

	hRedialThreadStart = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hRedialThreadStart)
	{
		printf("hRedialThreadStart error: %d\n", GetLastError());
		goto error;
	}
	HANDLE hRedialThread = (HANDLE)_beginthreadex(NULL, 0, redial_thread, NULL, 0, NULL);
	if (NULL == hRedialThread)
	{
		printf("_beginthreadex redial_thread error: %d\n", GetLastError());
		goto error;
	}
	if (WaitForSingleObject(hRedialThreadStart, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject hRedialThreadStart error: %d\n", GetLastError());
		goto error;
	}
	printf("redial_thread 启动完成\n");

	hReportThreadStart = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hReportThreadStart)
	{
		printf("hReportThreadStart error: %d\n", GetLastError());
		goto error;
	}
	HANDLE hReportThread = (HANDLE)_beginthreadex(NULL, 0, report_thread, NULL, 0, NULL);
	if (NULL == hReportThread)
	{
		printf("_beginthreadex report_thread error: %d\n", GetLastError());
		goto error;
	}
	if (WaitForSingleObject(hReportThreadStart, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject hReportThreadStart error: %d\n", GetLastError());
		goto error;
	}
	printf("report_thread 启动完成\n");

#ifndef PROXY_DEBUG
	if (!GetAdslInfo(g_AdslIp))
		DoAdsl();

	ontimer_checkversion(NULL, 0, 0, 0);
#endif // PROXY_DEBUG

	g_hSwitchThreadStart = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == g_hSwitchThreadStart)
	{
		printf("g_hSwitchThreadStart error: %d\n", GetLastError());
		goto error;
	}
	HANDLE hSwitchThread = (HANDLE)_beginthreadex(NULL, 0, switch_thread, NULL, 0, &g_switch_threadId);
	if (NULL == hSwitchThread)
	{
		printf("switch_thread 启动完成\n");
		goto error;
	}
	if (WaitForSingleObject(g_hSwitchThreadStart, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject g_hSwitchThreadStart error: %d\n", GetLastError());
		goto error;
	}
	WaitForSingleObject(g_hDoingNetWork, INFINITE);
	if (!PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0))
	{
		printf("PostThreadMessage error: %d\n", GetLastError());
		goto error;
	}

	WaitForSingleObject(hSwitchThread, 1000 * 30);
	HANDLE hTimerThread = (HANDLE)_beginthreadex(NULL, 0, ontimer_thread, NULL, 0, NULL);

	WaitForSingleObject(hSwitchThread, INFINITE);
	printf("switch_thread 退出\n");

	getchar();
	return 0;

error:
	printf("error\n");
	WSACleanup();
	getchar();
	return -1;
}

unsigned int _stdcall completionRoution(LPVOID)
{
	ULONG_PTR key;
	BUFFER_OBJ* bobj;
	LPOVERLAPPED lpol;
	DWORD dwTranstion;
	BOOL bSuccess;

	while (true)
	{
		bSuccess = GetQueuedCompletionStatus(hCompPort, &dwTranstion, &key, &lpol, INFINITE);
		if (NULL == lpol)
		{
			printf("如果不是认为设定NULL为退出信号，那么就是发生重大错误，直接退出\n");
			return 0;
		}

		bobj = CONTAINING_RECORD(lpol, BUFFER_OBJ, ol);

		if (!bSuccess)
			bobj->pfnFailed((void*)key, bobj);
		else
			bobj->pfnSuccess(dwTranstion, (void*)key, bobj);
	}

	return 0;
}

unsigned int _stdcall switch_thread(LPVOID pVoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	SetEvent(g_hSwitchThreadStart);

	HANDLE hModeThread = NULL;
	HANDLE h6086Thread = NULL;
	unsigned int nModeThreadId = 0;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		switch (msg.message)
		{
		case SWITCH_REDIAL:
		{
			//if (WaitForSingleObject(g_hDoingNetWork, 0) == WAIT_TIMEOUT)
			//	break;

			if (NULL != g_lobj)
			{
				closesocket(g_lobj->sListenSock);
				WSACloseEvent(g_lobj->hPostAcceptExEvent);
				do
				{
					Sleep(1000);
				} while (g_lobj->dwAcceptExPendingCount);
				delete g_lobj;
				g_lobj = NULL;
			}
			if (NULL != g_lobj6086)
			{
				closesocket(g_lobj6086->sListenSock);
				WSACloseEvent(g_lobj6086->hPostAcceptExEvent);
				do
				{
					Sleep(1000);
				} while (g_lobj6086->dwAcceptExPendingCount);
				delete g_lobj6086;
				g_lobj6086 = NULL;
			}

			if (INVALID_SOCKET != g_5001socket)
			{
				closesocket(g_5001socket);
				g_5001socket = INVALID_SOCKET;
			}
			if (INVALID_SOCKET != g_6086socket)
			{
				closesocket(g_6086socket);
				g_6086socket = INVALID_SOCKET;
			}

			if (NULL != hModeThread)
			{
				TerminateThread(hModeThread, 0);
				WaitForSingleObject(hModeThread, INFINITE);
				CloseHandle(hModeThread);
				hModeThread = NULL;
			}

			if (NULL != h6086Thread)
			{
				TerminateThread(h6086Thread, 0);
				WaitForSingleObject(h6086Thread, INFINITE);
				CloseHandle(h6086Thread);
				h6086Thread = NULL;
			}

			printf("线程关闭完成\n");

			g_dwListenPortCount = 0;

			SetEvent(hRedialStartEvent);
			if (WaitForSingleObject(hRedialCompEvent, INFINITE) != WAIT_OBJECT_0)
			{
				printf("WaitForSingleObject hRedialCompEvent error: %d\n", GetLastError());
				SetEvent(g_hDoingNetWork);
				break;
			}
			printf("拨号完成\n");
			SetEvent(g_hDoingNetWork);

			while (PeekMessage(&msg, NULL, SWITCH_REDIAL, SWITCH_REDIAL, PM_REMOVE))
			{
				printf("清空 SWITCH_REDIAL 消息\n");
			}

			GetUsernameAndPassword();

#ifdef PROXY_DEBUG
			bSwithMode1 = TRUE;
			PostThreadMessage(g_switch_threadId, SWITCH_MODE1, 0, 0);
#else
			if (CheckReservedIp(g_AdslIp))// 内网ip
			{
				bSwithMode1 = FALSE;
				PostThreadMessage(g_switch_threadId, SWITCH_MODE2, 0, 0);
			}
			else
			{
				bSwithMode1 = TRUE;
				PostThreadMessage(g_switch_threadId, SWITCH_MODE1, 0, 0);
			}
#endif // PROXY_DEBUG
		}
		break;

		case SWITCH_MODE1:
		{
			hModeThread = (HANDLE)_beginthreadex(NULL, 0, mode1, NULL, 0, &nModeThreadId);
			if (NULL == hModeThread)
			{
				printf("启动 mode1 失败\n");
			}
		}
		break;

		case SWITCH_MODE2:
		{
			hModeThread = (HANDLE)_beginthreadex(NULL, 0, mode2, NULL, 0, &nModeThreadId);
			if (NULL == hModeThread)
			{
				printf("启动 mode2 失败\n");
				break;
			}
			WaitForSingleObject(g_hMode2ThreadStart, INFINITE);
			h6086Thread = (HANDLE)_beginthreadex(NULL, 0, mode2_6086, NULL, 0, NULL);
			if (NULL == h6086Thread)
			{
				printf("启动 mode2_6086 失败\n");
			}
		}
		break;

		default:
			break;
		}
	}

	return 0;
}

unsigned int _stdcall mode1(LPVOID pVoid)
{
	vector<BUFFER_OBJ*> vFreeBuffer;
	
	g_lobj = new LISTEN_OBJ();
	g_lobj->init();
	g_lobj6086 = new LISTEN_OBJ();
	g_lobj6086->init();

	g_nPort = GetListenPort();
	if (!InitListenSock(g_lobj, g_nPort))
	{
		printf("设置 5005 监听端口失败\n");
		goto error;
	}

	for (size_t i = 0; i < 10; i++)
	{
		PostAcceptEx(g_lobj, 0);
	}
	printf("%d\n", g_lobj->dwAcceptExPendingCount);

	if (!InitListenSock(g_lobj6086, 6086))
	{
		printf("设置 6086 监听端口失败\n");
		goto error;
	}

	for (size_t i = 0; i < 10; i++)
	{
		PostAcceptEx(g_lobj6086, 1);
	}
	printf("%d\n", g_lobj6086->dwAcceptExPendingCount);

	SetEvent(hReportStartEvent);
	if (WaitForSingleObject(hReportCompEvent, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject hReportCompEvent error: %d\n", GetLastError());
		goto error;
	}
	printf("上报完成\n");

	while (true)
	{
		int nRetVal = WSAWaitForMultipleEvents(g_dwListenPortCount, hEvent_Array, FALSE, INFINITE, FALSE);
		if (WSA_WAIT_FAILED == nRetVal)
		{
			printf("WSAWaitForMultipleEvents failed with error: %d\n", WSAGetLastError());
			printf("代理进程退出\n");
			getchar();
			break;
		}

		for (DWORD i = 0; i < g_dwListenPortCount; i++)
		{
			nRetVal = WaitForSingleObject(hEvent_Array[i], 0);
			if (WAIT_TIMEOUT == nRetVal)
				continue;
			else if (WAIT_FAILED == nRetVal)
			{
				printf("WaitForSingleObject %d failed with error: %d\n", i, WSAGetLastError());
				printf("代理进程退出\n");
				getchar();
				return -1;
			}

			WSAResetEvent(hEvent_Array[i]);
			for (DWORD j = 0; j < 10; j++)
			{
				PostAcceptEx(LSock_Array[i], i);
			}

			if (LSock_Array[i]->dwAcceptExPendingCount > 10 * 10)
			{
				vFreeBuffer.clear();
				int optlen,
					optval;
				optlen = sizeof(optval);
				EnterCriticalSection(&LSock_Array[i]->cs);
				BUFFER_OBJ* bobj = LSock_Array[i]->pAcceptExPendingList;
				while (bobj)
				{
					SOCKET_OBJ* sobj = bobj->pRelatedSObj;
					nRetVal = getsockopt(sobj->sock, SOL_SOCKET, SO_CONNECT_TIME, (char*)&optval, &optlen);
					if (SOCKET_ERROR == nRetVal)
						printf("getsockopt SO_CONNECT_TIME error: %d\n", WSAGetLastError());
					else
					{
						if (optval != 0xffffffff && optval > 300)
							vFreeBuffer.push_back(bobj);
					}

					bobj = bobj->pNext;
				}
				LeaveCriticalSection(&LSock_Array[i]->cs);

				size_t nsize = vFreeBuffer.size();
				for (size_t i = 0; i < nsize; i++)
					PCloseSocket(vFreeBuffer[i]->pRelatedSObj);
			}
		}
	}

	return 0;

error:
	printf("error\n");
	WaitForSingleObject(g_hDoingNetWork, INFINITE);
	PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0);
	return -1;
}

#ifdef PROXY_DEBUG
#define PROXY_MONITOR_PATH "D:\\Monitor"
#else
#define PROXY_MONITOR_PATH "C:\\Monitor"
#endif // PROXY_DEBUG

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

BOOL Monitor(cJSON** pAppInfo)
{
	char* pVersion = cJSON_GetObjectItem(pAppInfo[0], "version")->valuestring;
	if (CompareVersion("0.0.1", pVersion))
	{
		printf("proxy-monitor不需要更新\n");
		return TRUE;
	}

	char* pMd5 = cJSON_GetObjectItem(pAppInfo[0], "ext_md5")->valuestring;
	char* pDUrl = cJSON_GetObjectItem(pAppInfo[0], "update_url")->valuestring;
	char FormatDownUrl[MAX_PATH] = { 0 };
	UrlFormating(pDUrl, "\\", FormatDownUrl);
	char pFilePath[MAX_PATH];
	sprintf_s(pFilePath, "%s\\pwatch-v%s.exe", PROXY_MONITOR_PATH, pVersion);

	if (!doDownLoad(pFilePath, FormatDownUrl, pMd5))
		return FALSE;

	Sleep(1000);
	ShellExecute(0, "open", pFilePath, NULL, NULL, SW_SHOWNORMAL);
	printf("proxy_monitor.exe 更新完成\n");
	return TRUE;
}

BOOL Proxy(cJSON** pAppInfo)
{
	char* pVersion = cJSON_GetObjectItem(pAppInfo[1], "version")->valuestring;
	if (CompareVersion(PROXY_VERSION, pVersion))
		return TRUE;

	char szProxyPath[MAX_PATH];
	char szUpgradePath[MAX_PATH];
	char szTempPath[MAX_PATH];
	int nTempLen = GetTempPath(MAX_PATH, szTempPath);
	_makepath_s(szProxyPath, NULL, szTempPath, "proxy.exe", NULL);
	_makepath_s(szUpgradePath, NULL, szTempPath, "upgrade.exe", NULL);

	char* pProxyUrl = cJSON_GetObjectItem(pAppInfo[1], "update_url")->valuestring;
	char* pUpgradeUrl = cJSON_GetObjectItem(pAppInfo[2], "update_url")->valuestring;
	if (NULL == pProxyUrl || NULL == pUpgradeUrl)
		return FALSE;

	char szProxyUrl[MAX_PATH] = { 0 };
	char szUpgradeUrl[MAX_PATH] = { 0 };
	UrlFormating(pUpgradeUrl, "\\", szUpgradeUrl);
	UrlFormating(pProxyUrl, "\\", szProxyUrl);

	char* pProxyMd5 = cJSON_GetObjectItem(pAppInfo[1], "ext_md5")->valuestring;
	char* pUpgradeMd5 = cJSON_GetObjectItem(pAppInfo[2], "ext_md5")->valuestring;
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
#ifdef PROXY_DEBUG
	return;
#endif // PROXY_DEBUG

	if (WaitForSingleObject(g_hDoingNetWork, 0) == WAIT_TIMEOUT)
		return;
	CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
	if (NULL == pResponseData)
	{
		printf("内存分配失败\n");
		SetEvent(g_hDoingNetWork);
		return;
	}
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		printf("内存分配失败\n");
		SetEvent(g_hDoingNetWork);
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
	if (nArraySize < 3)
	{
		printf("返回的数据中没有需要的 3 个exe信息 = %d\n", nArraySize);
		goto error;
	}

	cJSON* app_info[3];
	app_info[0] = cJSON_GetObjectItem(cjArray, "proxy-monitor");
	if (NULL == app_info[0])
	{
		printf("获取 proxy-monitor 版本信息失败\n");
		goto error;
	}
	app_info[1] = cJSON_GetObjectItem(cjArray, "app-proxy");
	if (NULL == app_info[1])
	{
		printf("获取 app-proxy 版本信息失败\n");
		goto error;
	}
	app_info[2] = cJSON_GetObjectItem(cjArray, "app-upgrade");
	if (NULL == app_info[2])
	{
		printf("获取 app-upgrade 版本信息失败\n");
		goto error;
	}

	if (!Proxy(app_info))
	{
		printf("升级代理失败\n");
		goto error;
	}

	//if (!Monitor(app_info))
	//{
	//	printf("升级监控程序失败\n");
	//	goto error;
	//}

error:
	SetEvent(g_hDoingNetWork);
	if (NULL != root)
		cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
}

void _stdcall ontimer_checknet(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	
}

unsigned int _stdcall ontimer_thread(void* pVoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

//	SetTimer(NULL, 1, 1000 * 60, ontimer_checknet);
	SetTimer(NULL, 2, 1000 * 60 * 10, ontimer_checkversion);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_TIMER)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

BOOL SendData(SOCKET sock, char* pSendBuf)
{
	int iRet = 0;
	int nSendBufLen = strlen(pSendBuf);

	int sendlen = 0;
	while (sendlen < nSendBufLen)
	{
		iRet = send(sock, pSendBuf + sendlen, nSendBufLen - sendlen, 0);
		if (iRet == 0 || iRet == SOCKET_ERROR)
		{
			printf("send host_id to server time out wiht error code : %d", WSAGetLastError());
			return FALSE;
		}
		sendlen += iRet;
	}
	return TRUE;
}

int RecvPortFromDisiServer(SOCKET sock, char** pInfo)
{
	int nRet = 0;
	unsigned long WaitBytes = 0;
	char* pRecvBuf = (char*)malloc(128);
	memset(pRecvBuf, 0x00, 128);

	WaitBytes = 0;
	int RecvLen = 0;
	nRet = recv(sock, pRecvBuf, WaitBytes, 0);
	if (SOCKET_ERROR != nRet && 0 != nRet)
	{
		RecvLen += nRet;
		while (RecvLen < (int)WaitBytes)
		{
			nRet = recv(sock, pRecvBuf + RecvLen, WaitBytes - RecvLen, 0);
			if (SOCKET_ERROR == nRet || 0 == nRet)
			{
				free(pRecvBuf);
				int err = WSAGetLastError();
				printf("RecvPortFromDisiServer error: %d\n", err);
				if (10054 == err || 10060 == err)
					return 2;

				return 1;
			}
			RecvLen += nRet;
		}

		int nMsgLen = pRecvBuf[0] * 256 + pRecvBuf[1];
		if (nMsgLen <= 0 || nMsgLen > 256)
		{
			delete pRecvBuf;
			return 2;
		}
		if (nMsgLen > 128)
		{
			free(pRecvBuf);
			int nByteSize = MY_ALIGN(nMsgLen + 1, 8);
			pRecvBuf = (char*)malloc(nByteSize);
			memset(pRecvBuf, 0x00, nByteSize);
		}
		RecvLen = 0;
		while (RecvLen < nMsgLen)
		{
			nRet = recv(sock, pRecvBuf + RecvLen, nMsgLen - RecvLen, 0);
			if (SOCKET_ERROR == nRet || 0 == nRet)
			{
				free(pRecvBuf);
				int err = WSAGetLastError();
				printf("RecvPortFromDisiServer error: %d\n", err);
				if (10054 == err || 10060 == err)
					return 2;

				return 1;
			}
			RecvLen += nRet;
		}
		*pInfo = pRecvBuf;
	}
	else
	{
		free(pRecvBuf);
		int err = WSAGetLastError();
		printf("RecvPortFromDisiServer error: %d\n", err);
		if (10054 == err || 10060 == err)
			return 2;

		return 1;
	}

	return 0;
}

int AanlyzePortFromInfo(char* pPortInfo)
{
	int nPort = 0;
	char* pBegin = strstr(pPortInfo, "post=");
	if (NULL == pBegin)
		return nPort;

	char* pEnd = strstr(pBegin, "IAMNEWPORTEND");
	if (NULL == pEnd)
		return nPort;

	int portlen = pEnd - pBegin - strlen("post=");
	int nByteSize = MY_ALIGN(portlen + 1, 8);
	char* pNewPort = (char*)malloc(nByteSize);
	memset(pNewPort, 0x00, nByteSize);
	memcpy(pNewPort, pBegin + strlen("post="), portlen);

	nPort = atoi(pNewPort);
	free(pNewPort);

	return nPort;
}

unsigned int _stdcall mode2(LPVOID pVoid)
{
	int err = 0;

	if (!GetServerIpFromHost())
		goto error;

	GetRandIndex(pArrayIndex, nCountOfArray);

	for (int i = 0; i < nCountOfArray;)
	{
		int nCode = ConnectToDisiServer(g_5001socket, vip[pArrayIndex[i]], 5001);

		if (0 != nCode)
		{
			if (++i == nCountOfArray)
				goto error;

			continue;
		}

		DWORD nTimeOut = 5 * 1000;
		setsockopt(g_5001socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeOut, sizeof(DWORD));

		struct tcp_keepalive alive_in = { TRUE, 1000 * 60 * 2, 1000 * 5};
		struct tcp_keepalive alive_out = { 0 };
		unsigned long ulBytesReturn = 0;
		err = WSAIoctl(g_5001socket, SIO_KEEPALIVE_VALS,
			&alive_in, sizeof(alive_in),
			&alive_out, sizeof(alive_out),
			&ulBytesReturn, NULL, NULL);

		char* pTemp = "IAMPROXYBEGINhost_id=%sIAMPROXYEND";
		char* pSendBuf = (char*)malloc(128);
		sprintf_s(pSendBuf, 128, pTemp, g_pClient_id);
		if (!SendData(g_5001socket, pSendBuf))
		{
			free(pSendBuf);
			if (++i == nCountOfArray)
				goto error;

			continue;
		}
		free(pSendBuf);

		char* pPortInfo = NULL;
		nCode = RecvPortFromDisiServer(g_5001socket, &pPortInfo);
		if (nCode != 0)
		{
			if (++i == nCountOfArray)
				goto error;
			
			continue;
		}

		if (NULL == pPortInfo)
		{
			printf("Recv PORT info error");
			goto error;
		}

		if (strstr(pPortInfo, "-Proxy5001IsRead") == NULL)
		{
			free(pPortInfo);
			if (++i == nCountOfArray)
				goto error;

			continue;
		}

		printf("DISI_SERVER 端口信息: %s\n", pPortInfo);
		g_nPort = AanlyzePortFromInfo(pPortInfo);
		if (0 == g_nPort)
		{
			free(pPortInfo);
			if (++i == nCountOfArray)
				goto error;

			continue;
		}

		free(pPortInfo);

		nIndex = i;
		break;
	}

	SetEvent(g_hMode2ThreadStart);

	while (true)
	{
		char* pPortInfo = NULL;
		int nCode = RecvPortFromDisiServer(g_5001socket, &pPortInfo);
		if (nCode == 1)
			goto error;
		else if (nCode == 2)
			return 0;

		if (NULL == pPortInfo)
		{
			printf("Recv disi PORT info error");
			goto error;
		}

		int nPort = AanlyzePortFromInfo(pPortInfo);
		free(pPortInfo);
		if (0 == nPort)
			continue;

		Server_CONNECT(nPort);
	}

	return 0;

error:
	WaitForSingleObject(g_hDoingNetWork, INFINITE);
	PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0);
	return -1;
}

unsigned int _stdcall mode2_6086(LPVOID pVoid)
{
	int nTry = 0;
	int nCode = 0;
	int err = 0;
	struct tcp_keepalive alive_in = { TRUE, 1000 * 60 * 2, 1000 * 5 };
	struct tcp_keepalive alive_out = { 0 };
	unsigned long ulBytesReturn = 0;

	do
	{
		if (nTry > 5)
			goto error;

		nCode = ConnectToDisiServer(g_6086socket, vip[pArrayIndex[nIndex]], 6086);
		if (0 == nCode)
			break;
		else if (1 == nCode)
			goto error;
		else
			nTry++;
	} while (true);

	DWORD nTimeOut = 5 * 1000;
	setsockopt(g_6086socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeOut, sizeof(DWORD));

	err = WSAIoctl(g_5001socket, SIO_KEEPALIVE_VALS,
		&alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out),
		&ulBytesReturn, NULL, NULL);

	char* pTemp = "IAMPROXY6086BEGINhost_id=%sIAMPROXY6086END";
	char* pSendBuf = (char*)malloc(128);
	sprintf_s(pSendBuf, 128, pTemp, g_pClient_id);
	if (!SendData(g_5001socket, pSendBuf))
	{
		free(pSendBuf);
		goto error;
	}
	free(pSendBuf);

	SetEvent(hReportStartEvent);
	if (WaitForSingleObject(hReportCompEvent, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject hReportCompEvent error: %d\n", GetLastError());
		goto error;
	}
	printf("上报完成\n");

	int nRecvLen = 0;
	int nInfoTotalLen = 1024;
	char* pRecv6086Info = (char*)malloc(1024);
	memset(pRecv6086Info, 0x00, 1024);
	do
	{
		if (nRecvLen >= nInfoTotalLen)
		{
			free(pRecv6086Info);
			goto error;
		}
		err = recv(g_6086socket, pRecv6086Info + nRecvLen, nInfoTotalLen - nRecvLen, 0);
		if (err > 0)
			nRecvLen += err;
		else
		{
			free(pRecv6086Info);
			err = WSAGetLastError();
			if (10053 == err || 10038 == err)
				return 0;
			else
				goto error;
		}
		char* pHeader = NULL;
		if ((pHeader = strstr(pRecv6086Info, "\r\n\r\n")) == NULL)
			continue;
		else
		{
			int HeaderLen = pHeader - pRecv6086Info;
			HeaderLen += strlen("\r\n\r\n");
			int len = strlen("Content-Length: ");
			char* pContentLength = StrStrI(pRecv6086Info, "Content-Length: ");
			if (NULL == pContentLength)
			{
				free(pRecv6086Info);
				goto error;
			}			
			char* pContentLengthEnd = strstr(pContentLength, "\r\n");
			if (NULL == pContentLengthEnd)
			{
				free(pRecv6086Info);
				goto error;
			}
			int nLengthLen = pContentLengthEnd - pContentLength - len;
			char Length[8] = { 0 };
			memcpy_s(Length, 8, pContentLength + len, nLengthLen);
			len = atoi(Length);
			if ((HeaderLen + len) > nRecvLen)
				continue;
			else if ((HeaderLen + len) == nRecvLen)
				break;
		}
	} while (TRUE);

	printf("mode2 6086: %s\n", pRecv6086Info);
	free(pRecv6086Info);

error:
	WaitForSingleObject(g_hDoingNetWork, INFINITE);
	PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0);
	return 0;
}