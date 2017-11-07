#include "../ShareApi/ShareApi.h"
#include "../Proxy/md5.h"
#include "dounzip.h"
#include <curl\curl.h>
#include "CurlClient.h"

#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "wldap32.lib")

BOOL Curl_POST(const char* pUrl, const char* pData, pCurlWriteFunction pCWFunc, void* pResponseInfo)
{
	CURL* pCurl = NULL;
	CURLcode code = CURLE_OK;
	
	pCurl = curl_easy_init();
	if (NULL == pCurl)
	{
		printf("Curl_POST curl_easy_init failed\n");
		return FALSE;
	}

	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, FALSE);
	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYHOST, FALSE);

	curl_easy_setopt(pCurl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(pCurl, CURLOPT_URL, pUrl);
	curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, pData);
	curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, pCWFunc);
	curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, pResponseInfo);
	curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 600);

	code = curl_easy_perform(pCurl);
	if (CURLE_OK != code)
	{
		printf("Curl_POST curl_easy_perform failed with error: %d\n", code);
		printf("Error code type ：CURLcode");
		curl_easy_cleanup(pCurl);
		return FALSE;
	}

	curl_easy_cleanup(pCurl);

	return TRUE;
}

BOOL Curl_GET(const char* pUrl, pCurlWriteFunction pCWFunc, void* pResponseInfo)
{
	CURL* pCurl = NULL;
	CURLcode code = CURLE_OK;

	pCurl = curl_easy_init();
	if (NULL == pCurl)
	{
		printf("Curl_GET curl_easy_init failed\n");
		return FALSE;
	}

	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, FALSE);
	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYHOST, FALSE);

	curl_easy_setopt(pCurl, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(pCurl, CURLOPT_URL, pUrl);
	curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, pCWFunc);
	curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, pResponseInfo);
	curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, 600);

	code = curl_easy_perform(pCurl);
	if (CURLE_OK != code)
	{
		printf("Curl_GET curl_easy_perform failed with error: %d\n", code);
		printf("Error code type ：CURLcode");
		curl_easy_cleanup(pCurl);
		return FALSE;
	}

	curl_easy_cleanup(pCurl);

	return TRUE;
}

size_t CWFunc_GetProxyVersionInfo(void* pInfo, size_t size, size_t nmemb, void* pResponse)
{
	struct CurlResponseData* pResponseInfo = (struct CurlResponseData*)pResponse;
	size_t nInfoLen = size * nmemb;
	size_t RecvedLen = MY_ALIGN((pResponseInfo->dwDataLen + nInfoLen + 1), 8);
	if (_msize(pResponseInfo->pData) < RecvedLen)
		pResponseInfo->pData = (char*)realloc(pResponseInfo->pData, RecvedLen);
	if (pResponseInfo->pData)
	{
		memcpy(pResponseInfo->pData + pResponseInfo->dwDataLen, pInfo, nInfoLen);
		pResponseInfo->dwDataLen += nInfoLen;
	}
	return size * nmemb;
}

BOOL Curl_GetProxyVersionInfo(const char* pHost_id, struct CurlResponseData* pResponseData)
{
	char UrlData[MAX_PATH] = { 0 };
	sprintf_s(UrlData, URL_GET_PROXY_VERSION_INFO, pHost_id);

	if (!Curl_GET(UrlData, CWFunc_GetProxyVersionInfo, pResponseData))
		return FALSE;

	pResponseData->pData[pResponseData->dwDataLen] = '\0';

	return TRUE;
}

BOOL Curl_GetProxyReportInfo(const char* pHost_id, struct CurlResponseData* pResponseData)
{
	char UrlData[MAX_PATH] = { 0 };
	sprintf_s(UrlData, URL_REPORTED_PROXY_HOST_INFO, pHost_id);

	if (!Curl_GET(UrlData, CWFunc_GetProxyVersionInfo, pResponseData))
		return FALSE;

	pResponseData->pData[pResponseData->dwDataLen] = '\0';

	return TRUE;
}

BOOL Curl_GetHostInfo(const char* pHost_id, struct CurlResponseData* pResponseData)
{
	char UrlData[MAX_PATH] = { 0 };
	sprintf_s(UrlData, URL_GET_PROXY_HOST_INFO, pHost_id);

	char pPostData[128] = { 0 };
	sprintf_s(pPostData, "app_secret=F$~((kb~AjO*xgn~&host_id=%s", pHost_id);

	if (!Curl_POST(UrlData, pPostData, CWFunc_GetProxyVersionInfo, pResponseData))
		return FALSE;

	pResponseData->pData[pResponseData->dwDataLen] = '\0';

	return TRUE;
}

BOOL Curl_PostData2Server(const char* url, const char* pData, struct CurlResponseData* pResponseData)
{
	char postUrl[MAX_PATH] = { 0 };
	sprintf_s(postUrl, url, g_pClient_id);

	if (!Curl_POST(postUrl, pData, CWFunc_GetProxyVersionInfo, pResponseData))
		return FALSE;

	pResponseData->pData[pResponseData->dwDataLen] = '\0';

	return TRUE;
}

size_t CWFunc_DownloadFile(void* pInfo, size_t size, size_t nmemb, void* file)
{
	FILE* pFile = (FILE*)file;
	fwrite(pInfo, 1, size * nmemb, pFile);

	return size * nmemb;
}

BOOL Curl_DownloadFile(const char* url, const char* downPath)
{
	FILE* pFile = NULL;
	fopen_s(&pFile, downPath, "wb");
	if (NULL == pFile)
	{
		printf("打开文件下载目录失败\n");
		return FALSE;
	}
	if (!Curl_GET(url, CWFunc_DownloadFile, pFile))
	{
		printf("文件下载失败\n");
		goto error;
	}
	printf("文件 %s 下载完成\n", downPath);

	fclose(pFile);

	return TRUE;

error:
	if (NULL != pFile)
		fclose(pFile);
	return FALSE;
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

HANDLE hReportStartEvent = NULL;
//HANDLE hReportCompEvent = NULL;
HANDLE hReportThreadStart = NULL;

unsigned int _stdcall report_thread(LPVOID pVoid)
{
	hReportStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
//	hReportCompEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	SetEvent(hReportThreadStart);

	while (WaitForSingleObject(hReportStartEvent, 1000 * 60 * 5) != WAIT_FAILED)
	{
#ifdef PROXY_DEBUG
		continue;
#endif // PROXY_DEBUG

		//if (WaitForSingleObject(g_hDoingNetWork, 0) == WAIT_TIMEOUT)
		//	continue;

		BOOL bSuccess = FALSE;
		int nAttempts = 0;
		CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
		if (NULL == pResponseData)
		{
			printf("内存分配失败\n");
			//SetEvent(g_hDoingNetWork);
			continue;
		}
		pResponseData->pData = (char*)malloc(512);
		if (NULL == pResponseData->pData)
		{
			printf("内存分配失败\n");
			//SetEvent(g_hDoingNetWork);
			free(pResponseData);
			continue;
		}
		do
		{
			pResponseData->dwDataLen = 0;

			char szPort[8] = { 0 };
			_itoa_s(g_nPort, szPort, 10);
			char PostData[256] = { 0 };
			if (bSwithMode1)// 模式1
			{
				char* pDataTemp = "value={\"host_id\":\"%s\",\"ip\":\"%s\",\"sip\":null,\"port\":%s,\"u\":\"%s\",\"p\":\"%s\"}";
				sprintf_s(PostData, pDataTemp, g_pClient_id, g_AdslIp, szPort, g_username, g_password);
			}
			else// 模式2 sip
			{
				char* pDataTemp = "value={\"host_id\":\"%s\",\"ip\":null,\"sip\":\"%s\",\"port\":%s,\"u\":\"%s\",\"p\":\"%s\"}";
				sprintf_s(PostData, pDataTemp, g_pClient_id, vip[pArrayIndex[nIndex]], szPort, g_username, g_password);
			}
			bSuccess = Curl_PostData2Server(URL_REPORTED_PROXY_HOST_INFO, PostData, pResponseData);
			nAttempts += 1;
		} while (!bSuccess && nAttempts < 5);
		if (!bSuccess)
		{
			//SetEvent(g_hDoingNetWork);
			if (WaitForSingleObject(g_hDoingNetWork, 0) == WAIT_TIMEOUT)
				continue;
			PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0);
			continue;
		}
		printf("上报结果: %s\n", pResponseData->pData);
		//SetEvent(g_hDoingNetWork);

		free(pResponseData->pData);
		free(pResponseData);
//		SetEvent(hReportCompEvent);
	}
	printf("report_thread exit error: %d\n", GetLastError());
	return 0;
}

size_t CWFunc_CheckNetWorking(void* pInfo, size_t size, size_t nmemb, void* pResponse)
{
	return size * nmemb;
}

BOOL CheckIsNetWorking()
{
	CURL* curl;
	CURLcode res;
	curl = curl_easy_init();
	if (NULL == curl)
		return FALSE;

	curl_easy_setopt(curl, CURLOPT_URL, "http://www.baidu.com");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CWFunc_CheckNetWorking);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK)
		return FALSE;

	return TRUE;
}

BOOL CheckProxyIsNetworking(const char* ProxyIp, u_short ProxyPort)
{
	CURL* curl;
	CURLcode res;
	curl = curl_easy_init();
	if (NULL == curl)
		return FALSE;

	char ProxyInfo[32] = { 0 };
	sprintf_s(ProxyInfo, "%s:%d", ProxyIp, ProxyPort);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, FALSE);

	curl_easy_setopt(curl, CURLOPT_URL, "https://www.baidu.com");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CWFunc_CheckNetWorking);
	curl_easy_setopt(curl, CURLOPT_PROXY, ProxyInfo);
	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1); // 请求完成之后立即端口连接
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK)
		return FALSE;

	return TRUE;
}

BOOL AutoUpdate(cJSON** pAppInfo, const char* pModeName, const char* pUpgradeName)
{
	char* pVersion = cJSON_GetObjectItem(pAppInfo[0], "version")->valuestring;
	if (CompareVersion(VERSION, pVersion))
		return TRUE;

	char szProxyPath[MAX_PATH];
	char szUpgradePath[MAX_PATH];
	char szTempPath[MAX_PATH];
	int nTempLen = GetTempPath(MAX_PATH, szTempPath);
	_makepath_s(szProxyPath, NULL, szTempPath, pModeName, NULL);
	_makepath_s(szUpgradePath, NULL, szTempPath, pUpgradeName, NULL);

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

BOOL ProcessAutoUpdate(const char* pCode, const char* pModeName, const char* pUpgradeName)
{
	CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
	if (NULL == pResponseData)
	{
		printf("内存分配失败\n");
		return FALSE;
	}
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		printf("内存分配失败\n");
		free(pResponseData);
		return FALSE;
	}

	cJSON* root = NULL;
	pResponseData->dwDataLen = 0;
	if (!Curl_GetProxyVersionInfo(g_pClient_id, pResponseData))
	{
		printf("获取版本信息失败\n");
		goto error;
	}

#ifdef PROXY_DEBUG
	printf("GET请求返回的信息: %s\n", pResponseData->pData);
#endif // PROXY_DEBUG

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
	// "host-proxy2"
	cJSON* app_info[2];
	app_info[0] = cJSON_GetObjectItem(cjArray, pCode);
	if (NULL == app_info[0])
	{
		printf("获取 %s 版本信息失败\n", pCode);
		goto error;
	}
	app_info[1] = cJSON_GetObjectItem(cjArray, "app-upgrade");
	if (NULL == app_info[1])
	{
		printf("获取 app-upgrade 版本信息失败\n");
		goto error;
	}

	if (!AutoUpdate(app_info, pModeName, pUpgradeName))
	{
		printf("升级 %s 失败\n", pCode);
		goto error;
	}

	cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
	return TRUE;

error:
	if (NULL != root)
		cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
	return FALSE;
}

#ifdef USE_INSTALL_CHROME
BOOL InstallChrome()
{
	BOOL bUpdateShortcut = FALSE;
	if (_access("C:\\Program Files\\chrome\\chrome-bin", 0) != 0)
	{
		bUpdateShortcut = TRUE;
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		if (!Curl_DownloadFile(URL_DOWNLOAD_CHROME, "C:\\chrome-bin.zip"))
		{
			printf("下载 chrome-bin.zip 失败\n");
			return FALSE;
		}
		wchar_t* pSrcZip = ConvertUtf8ToUnicode_("C:\\chrome-bin.zip");
		if (!dounzip(pSrcZip, L"C:\\Program Files\\chrome"))
		{
			free(pSrcZip);
			pSrcZip = NULL;
			printf("解压 chrome-bin.zip 失败\n");
			return FALSE;
		}
		free(pSrcZip);
		pSrcZip = NULL;
		DeleteFile("C:\\chrome-bin.zip");
		printf("chrome-bin安装完成\n");
	}

	char pTpath[MAX_PATH] = { 0 };
	sprintf_s(pTpath, MAX_PATH, "%s%s", "C:\\Program Files\\chrome\\userdata", "\\Default\\Extensions\\npogbneglgfmgafpepecdconkgapppkd");
	char pWpath[MAX_PATH] = { 0 };
	sprintf_s(pWpath, MAX_PATH, "%s%s", "C:\\Program Files\\chrome\\userdata", "\\Default\\Extensions\\edffiillgkekafkdjahahdjhjffllgjg");

	if (_access(pTpath, 0) != 0 || _access(pWpath, 0) != 0)
	{
		bUpdateShortcut = TRUE;
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		if (!Curl_DownloadFile(URL_DOWNLOAD_USERDATA, "C:\\userdata.zip"))
		{
			printf("下载 userdata.zip 失败\n");
			return FALSE;
		}
		wchar_t* pSrcZip = ConvertUtf8ToUnicode_("C:\\userdata.zip");
		if (!dounzip(pSrcZip, L"C:\\Program Files\\chrome"))
		{
			free(pSrcZip);
			pSrcZip = NULL;
			printf("解压 userdata.zip 失败\n");
			return FALSE;
		}
		free(pSrcZip);
		pSrcZip = NULL;
		DeleteFile("C:\\userdata.zip");
		printf("userdata安装完成\n");
	}

	char szDesk[MAX_PATH] = { 0 };
	if (!SHGetSpecialFolderPath(NULL, szDesk, CSIDL_DESKTOPDIRECTORY, 0))
	{
		printf("获取桌面地址信息失败\n");
		return FALSE;
	}
	strcat_s(szDesk, "\\chrome46.lnk");

	if (bUpdateShortcut)
	{
		if (_access(szDesk, 0) == 0)
			DeleteFile(szDesk);
		Sleep(1000 * 2);
		CreateShortcuts("C:\\Program Files\\chrome\\chrome-bin\\chrome.exe",
			"--user-data-dir=\"C:\\Program Files\\chrome\\userdata\"",
			"C:\\Program Files\\chrome\\chrome-bin", szDesk);
	}

	if (bUpdateShortcut)
		ShellExecute(NULL, "open", 
			"C:\\Program Files\\chrome\\chrome-bin\\chrome.exe", 
			"--user-data-dir=\"C:\\Program Files\\chrome\\userdata\"",
			NULL, SW_SHOWNORMAL);

	return TRUE;
}
#endif

BOOL CheckProxyIsNetworking01(const char* ProxyIp, u_short ProxyPort)
{
	CURL* curl;
	CURLcode res;
	curl = curl_easy_init();
	if (NULL == curl)
		return FALSE;

	char ProxyInfo[32] = { 0 };
	sprintf_s(ProxyInfo, "%s:%d", ProxyIp, ProxyPort);

	curl_easy_setopt(curl, CURLOPT_URL, "http://www.baidu.com");
	//curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CWFunc_CheckNetWorking);
	curl_easy_setopt(curl, CURLOPT_PROXY, ProxyInfo);
	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1); // 请求完成之后立即端口连接
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK)
		return FALSE;

	return TRUE;
}

#ifndef USE_CURL_CLIENT
int main()
{
	if (!CheckProxyIsNetworking01("127.0.0.1", 5005))
		printf("失败\n");
	getchar();
	CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
	if (NULL == pResponseData)
	{
		printf("内存分配失败\n");
		return 0;
	}
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		printf("内存分配失败\n");
		return 0;
	}

	pResponseData->dwDataLen = 0;
	if (!Curl_GetProxyReportInfo("ah122", pResponseData))
	{
		printf("获取版本信息失败\n");
		return 0;
	}
	printf("GET请求返回的信息: %s\n", pResponseData->pData);
	getchar();
	if (CheckProxyIsNetworking("127.0.0.1", 5005))
		printf("pnet working\n");
	else
		printf("pnet not working\n");
	getchar();

	if (CheckIsNetWorking())
		printf("net working\n");
	else
		printf("net not working\n");
	getchar();

	CurlResponseData* pResponseData1 = new CurlResponseData();
	if (NULL == pResponseData1)
		return -1;
	pResponseData1->pData = (char*)malloc(512);
	if (NULL == pResponseData1->pData)
	{
		delete pResponseData1;
		return -1;
	}
	pResponseData1->dwDataLen = 0;
	/*if (!Curl_GetProxyVersionInfo("bj008", pResponseData1))
	{
		free(pResponseData1->pData);
		delete pResponseData1;
		return -1;
	}*/
	printf("GET请求返回的信息: %s\n", pResponseData1->pData);
	free(pResponseData1->pData);
	delete pResponseData1;
	getchar();

	/*CurlResponseData* */pResponseData = new CurlResponseData();
	if (NULL == pResponseData)
		return -1;
	pResponseData->pData = (char*)malloc(512);
	if (NULL == pResponseData->pData)
	{
		delete pResponseData;
		return -1;
	}
	pResponseData->dwDataLen = 0;
	if (!Curl_GetProxyVersionInfo("bj008", pResponseData))
	{
		free(pResponseData->pData);
		delete pResponseData;
		return -1;
	}
	printf("GET请求返回的信息: %s\n", pResponseData->pData);
	free(pResponseData->pData);
	delete pResponseData;
	getchar();


	DeleteDirectoryByFullName("D:\\test\\b46.0");
	getchar();

	wchar_t* pSrcZip = NULL;
	if (_access("C:\\Program Files\\chrome\\b46.0", 0) != 0)
	{
		if (!Curl_DownloadFile(URL_DOWNLOAD_CHROME, "C:\\chrome46.zip"))
			return -1;

		wchar_t* pSrcZip = ConvertUtf8ToUnicode_("C:\\chrome46.zip");
		if (!dounzip(pSrcZip, L"C:\\Program Files\\chrome"))
			return -1;
		delete pSrcZip;
		printf("ok\n");
	}

	char pTpath[MAX_PATH] = { 0 };
	sprintf_s(pTpath, MAX_PATH, "%s%s", "C:\\Program Files\\chrome\\userdata", "\\Default\\Extensions\\npogbneglgfmgafpepecdconkgapppkd");
	char pWpath[MAX_PATH] = { 0 };
	sprintf_s(pWpath, MAX_PATH, "%s%s", "C:\\Program Files\\chrome\\userdata", "\\Default\\Extensions\\edffiillgkekafkdjahahdjhjffllgjg");
	if (_access(pTpath, 0) != 0 || _access(pWpath, 0) != 0)
	{
		if (!Curl_DownloadFile(URL_DOWNLOAD_USERDATA, "C:\\userdata.zip"))
			return -1;
		pSrcZip = ConvertUtf8ToUnicode_("C:\\userdata.zip");
		if (!dounzip(pSrcZip, L"C:\\Program Files\\chrome"))
			return -1;
		delete pSrcZip;
		printf("ok\n");
	}

	char szDesk[MAX_PATH] = { 0 };
	if (!SHGetSpecialFolderPath(NULL, szDesk, CSIDL_DESKTOPDIRECTORY, 0))
	{
		printf("获取桌面地址信息失败\n");
		return -1;
	}
	strcat_s(szDesk, "\\chrome46.0.lnk");

	if (_access(pTpath, 0) != 0)
		CreateShortcuts("C:\\Program Files\\chrome\\b46.0\\chrome.exe", "--user-data-dir=\"C:\\Program Files\\chrome\\userdata\"", "C:\\Program Files\\chrome\\b46.0", szDesk);
	getchar();
	return 0;
}
#endif //USE_CURL_CLIENT