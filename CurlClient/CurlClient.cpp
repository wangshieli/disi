#include "../ShareApi/ShareApi.h"
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

HANDLE hReportStartEvent = NULL;
HANDLE hReportCompEvent = NULL;
HANDLE hReportThreadStart = NULL;

unsigned int _stdcall report_thread(LPVOID pVoid)
{
	hReportStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hReportCompEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	SetEvent(hReportThreadStart);

	while (WaitForSingleObject(hReportStartEvent, 1000 * 60 * 5) != WAIT_FAILED)
	{
		if (WaitForSingleObject(g_hDoingNetWork, 0) == WAIT_TIMEOUT)
			continue;

		BOOL bSuccess = FALSE;
		int nAttempts = 0;
		CurlResponseData* pResponseData = (CurlResponseData*)malloc(sizeof(CurlResponseData));
		if (NULL == pResponseData)
		{
			printf("内存分配失败\n");
			SetEvent(g_hDoingNetWork);
			continue;
		}
		pResponseData->pData = (char*)malloc(512);
		if (NULL == pResponseData->pData)
		{
			printf("内存分配失败\n");
			SetEvent(g_hDoingNetWork);
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
			SetEvent(g_hDoingNetWork);
			PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, 0, 0);
			continue;
		}
		printf("上报结果: %s\n", pResponseData->pData);
		SetEvent(g_hDoingNetWork);

		free(pResponseData->pData);
		free(pResponseData);
		SetEvent(hReportCompEvent);
	}
	printf("report_thread exit error: %d\n", GetLastError());
	return 0;
}

#ifndef USE_CURL_CLIENT
int main()
{
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

	CurlResponseData* pResponseData = new CurlResponseData();
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