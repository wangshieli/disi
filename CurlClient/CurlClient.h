#pragma once
#include "../Proxy/cJSON.h"
#include "../Proxy/md5.h"

#define URL_REPORTED_PROXY_HOST_INFO	"https://cnapi.disi.se/hashes/proxies/%s" // %s = host_id
#define URL_GET_PROXY_HOST_INFO	"https://wwwphpapi-0.disi.se/v1/host/%s/get"
#define URL_GET_PROXY_VERSION_INFO	"https://wwwphpapi-0.disi.se/v1/extension/versions?app_secret=F$~((kb~AjO*xgn~&codes=app-proxy,proxy-monitor,app-upgrade,host-controller,host-proxy2,host-command&host_id=%s"
#define	URL_GET_CTX_VERSION_INFO	"https://wwwphpapi-0.disi.se/v1/extension/versions?app_secret=F$~((kb~AjO*xgn~&codes=edffiillgkekafkdjahahdjhjffllgjg,npogbneglgfmgafpepecdconkgapppkd&host_id=%s"

#define URL_DOWNLOAD_CHROME	"http://chex.oss-cn-shanghai.aliyuncs.com/a/chrome-bin.zip"
#define URL_DOWNLOAD_USERDATA "http://chex.oss-cn-shanghai.aliyuncs.com/a/userdata.zip"

struct CurlResponseData
{
	char* pData;
	DWORD dwDataLen;
};

typedef size_t(*pCurlWriteFunction)(void*, size_t, size_t, void*);

BOOL Curl_POST(const char* pUrl, const char* pData, pCurlWriteFunction pCWFunc, void* pResponseInfo);

BOOL Curl_GET(const char* pUrl, pCurlWriteFunction pCWFunc, void* pResponseInfo);

size_t CWFunc_GetProxyVersionInfo(void* pInfo, size_t size, size_t nmemb, void* pResponse);
BOOL Curl_GetProxyVersionInfo(const char* pHost_id, struct CurlResponseData* pResponseData);
BOOL Curl_GetProxyReportInfo(const char* pHost_id, struct CurlResponseData* pResponseData);
BOOL Curl_GetHostInfo(const char* pHost_id, struct CurlResponseData* pResponseData);
BOOL Curl_PostData2Server(const char* url, const char* pData, struct CurlResponseData* pResponseData);


size_t CWFunc_DownloadFile(void* pInfo, size_t size, size_t nmemb, void* pFile);
BOOL Curl_DownloadFile(const char* url, const char* downPath);

unsigned int _stdcall report_thread(LPVOID pVoid);

BOOL CheckIsNetWorking();

BOOL CheckProxyIsNetworking(const char* ProxyIp, u_short ProxyPort);

BOOL CheckMd5(const char* pFilePath, const char* pMd5);

BOOL doDownLoad(const char* path, const char* pUrl, const char* pMd5);

// pModeName = proxy2.exe monitor.exe controller.exe
BOOL AutoUpdate(cJSON** pAppInfo, const char* pModeName, const char* pUpgradeName, BOOL bCheckMd5 = FALSE);

BOOL ProcessAutoUpdate(const char* pCode, const char* pModeName, const char* pUpgradeName);

#ifdef USE_INSTALL_CHROME
BOOL InstallChrome();
#endif

extern HANDLE hReportStartEvent;
extern HANDLE hReportCompEvent;
extern HANDLE hReportThreadStart;