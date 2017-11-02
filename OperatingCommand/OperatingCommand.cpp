#include "../ShareApi/ShareApi.h"
#include "../CurlClient/CurlClient.h"
#include "../ModifyDNSServers/ModifyDNSServers.h"
#include "../RegistryOperation/RegistryOperation.h"

// ��������: host_restart
// ��������: proxy_restart
// ����dns: dns_update
// chrome��װ: chrome_reinstall
// chrome����: chrome_restart
// �����װ: extension_reinstall
// chrome��� chrome_check

BOOL Proxy(cJSON** pAppInfo)
{
	char* pVersion = cJSON_GetObjectItem(pAppInfo[0], "version")->valuestring;
	if (NULL == pVersion)
		return FALSE;

	char szProxyPath[MAX_PATH];

	sprintf_s(szProxyPath, "%s\\proxy2-v%s.exe", CommandFiler, pVersion);

	char* pProxyUrl = cJSON_GetObjectItem(pAppInfo[0], "update_url")->valuestring;
	if (NULL == pProxyUrl)
		return FALSE;

	char szProxyUrl[MAX_PATH] = { 0 };
	UrlFormating(pProxyUrl, "\\", szProxyUrl);

	char* pProxyMd5 = cJSON_GetObjectItem(pAppInfo[0], "ext_md5")->valuestring;
	if (NULL == pProxyMd5)
		return FALSE;

	if (!doDownLoad(szProxyPath, szProxyUrl, pProxyMd5))
		return FALSE;
		
	ShellExecute(0, "open", szProxyPath, NULL, NULL, SW_SHOWNORMAL);

	return TRUE;
}

void DowndProxy()
{
	CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
	if (NULL == pResponseData)
	{
		printf("�ڴ����ʧ��\n");
		return;
	}
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		printf("�ڴ����ʧ��\n");
		free(pResponseData);
		return;
	}

	cJSON* root = NULL;
	pResponseData->dwDataLen = 0;
	if (!Curl_GetProxyVersionInfo(g_pClient_id, pResponseData))
	{
		printf("��ȡ�汾��Ϣʧ��\n");
		goto error;
	}
	printf("GET���󷵻ص���Ϣ: %s\n", pResponseData->pData);

	// ��������
	root = cJSON_Parse(pResponseData->pData);
	if (NULL == root)
	{
		printf("��ʽ��json����ʧ��\n");
		goto error;
	}

	cJSON* cjSuccess = cJSON_GetObjectItem(root, "success");
	if (NULL == cjSuccess)
	{
		printf("���յ���������û�� success �ֶ�\n");
		goto error;
	}
	if (0 == cjSuccess->valueint)
	{
		printf("�ϴ��������д�������\n");
		goto error;
	}

	cJSON* cjArray = cJSON_GetObjectItem(root, "data");
	if (NULL == cjArray)
	{
		printf("���յ���������û�� data �ֶ�\n");
		goto error;
	}

	int nArraySize = cJSON_GetArraySize(cjArray);
	if (nArraySize < 3)
	{
		printf("���ص�������û����Ҫ�� 3 ��exe��Ϣ = %d\n", nArraySize);
		goto error;
	}

	cJSON* app_info[1];
	app_info[0] = cJSON_GetObjectItem(cjArray, "host-proxy2");
	if (NULL == app_info[0])
	{
		printf("��ȡ app-proxy �汾��Ϣʧ��\n");
		goto error;
	}

	if (!Proxy(app_info))
	{
		printf("���ش���ʧ�ܴ���ʧ��\n");
		goto error;
	}

error:
	if (NULL != root)
		cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
}

BOOL ProxyRestart()
{
	HKEY hKey;
	if (!PRegCreateKey(SProxy, &hKey))
		return FALSE;

	char proxy_path[MAX_PATH];
	if (!GetRegValue(hKey, "path", proxy_path))
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);

	if (_access(proxy_path, 0) != 0)
		return FALSE;

	char FileName[_MAX_FNAME] = { 0 };
	_splitpath_s(proxy_path, NULL, 0, NULL, 0, FileName, _MAX_FNAME, NULL, 0);
	strcat_s(FileName, ".exe");
	CloseTheSpecifiedProcess(FileName);
	Sleep(1000 * 2);

	ShellExecute(0, "open", proxy_path, NULL, NULL, SW_SHOWNORMAL);

	return TRUE;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("��������\n");
		goto error;
	}

	if (!GetClient_id(&g_pClient_id))
	{
		printf("��ȡclient_idʧ��");
		goto error;
	}

	printf("��������: %s\n", argv[1]);
	if (strcmp(argv[1], "host_restart") == 0)
	{
		ComputerRestart();
	}
	else if (strcmp(argv[1], "proxy_restart") == 0)
	{
		if (!ProxyRestart())
		{
			DowndProxy();
		}
	}
	else if (strcmp(argv[1], "dns_update") == 0)
	{
		if (argc < 3)
		{
			printf("dns_update ��������\n");
			goto error;
		}

		if (argc == 3)
			ModifyDnsServer(argv[2], NULL);
		else if(argc == 4)
		{
			ModifyDnsServer(argv[2], argv[3]);
		}

		// ��6085���Ͳ�������
		SOCKET sock = INVALID_SOCKET;
		int err = ConnectToDisiServer(sock, "127.0.0.1", 6085);
		if (0 != err)
		{
			printf("����6085�˿�ʧ��\n");
		}
		DWORD nTimeOut = 5 * 1000;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeOut, sizeof(DWORD));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeOut, sizeof(DWORD));
		const char* pSend = "GET /change HTTP/1.1\r\n\r\n";
		send(sock, pSend, strlen(pSend), 0);
		char retBuff[256] = { 0 };
		recv(sock, retBuff, 256, 0);
		closesocket(sock);
	}
	else if (strcmp(argv[1], "chrome_reinstall") == 0)
	{
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		DeleteDirectoryByFullName("\"C:\\Program Files\\chrome\\chrome-bin\"");
		Sleep(1000 * 5);
		InstallChrome();
	}
	else if (strcmp(argv[1], "chrome_restart") == 0)
	{
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		ShellExecute(NULL, "open",
			"C:\\Program Files\\chrome\\chrome-bin\\chrome.exe",
			"--user-data-dir=\"C:\\Program Files\\chrome\\userdata\"",
			NULL, SW_SHOWNORMAL);
	}
	else if (strcmp(argv[1], "extension_reinstall") == 0)
	{
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		DeleteDirectoryByFullName("\"C:\\Program Files\\chrome\\userdata\"");
		Sleep(1000 * 5);
		InstallChrome();
	}
	else if (strcmp(argv[1], "chrome_check") == 0)
	{
		InstallChrome();
	}

	return 0;

error:
	getchar();
	return -1;
}