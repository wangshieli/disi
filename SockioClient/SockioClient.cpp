#include "../ShareApi/ShareApi.h"
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "../CurlClient/CurlClient.h"
#include "../RegistryOperation/RegistryOperation.h"

#include "../Proxy/cJSON.h"
#include "../Proxy/md5.h"

#include "sockio\sio_client.h"

#pragma warning(disable:4996)


#define VERSION "0.1.33"
#define SOCKET_IO_VERSION ("socketio-v"##VERSION##"_test")

#define BUF_SIZE 512
#define CLIENT_RELINK WM_USER + 501
#define CLIENT_SEND_2_SERVER WM_USER + 502
#define SOCKET_IO_URL "https://host-notification.disi.se/"
#define SOCKET_IO_NAME "host-notification.disi.se"

HANDLE hClientThreadStart = NULL;
char CommandPath[MAX_PATH] = { 0 };

using namespace std;
using namespace sio;

mutex _lock;
condition_variable_any _cond;
bool connect_finish = false;

extern unsigned int g_client_thread_id;
unsigned int g_client_thread_id = 0;

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
	_makepath_s(szProxyPath, NULL, szTempPath, "proxy.exe", NULL);
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

BOOL PWorkUnit(cJSON** pAppInfo)
{
	HKEY hKey = NULL;
	if (!PRegCreateKey(CommandExecute, &hKey))
		return FALSE;
	char Version[MAX_PATH] = { 0 };
	//if (!GetRegValue(hKey, "version", Version))
	//{
	//	RegCloseKey(hKey);
	//	return FALSE;
	//}
	GetRegValue(hKey, "version", Version);

	char pFilePath[MAX_PATH];
	sprintf_s(pFilePath, "%s\\pworkunit-v%s.exe", CommandFiler, Version);
	char* pVersion = cJSON_GetObjectItem(pAppInfo[2], "version")->valuestring;
	if (CompareVersion(Version, pVersion) && _access(pFilePath, 0) == 0)
	{
		printf("proxy-monitor不需要更新\n");
		RegCloseKey(hKey);
		return TRUE;
	}

	CreateDirectory(CommandFiler, NULL);

	char* pMd5 = cJSON_GetObjectItem(pAppInfo[2], "ext_md5")->valuestring;
	char* pDUrl = cJSON_GetObjectItem(pAppInfo[2], "update_url")->valuestring;
	char FormatDownUrl[MAX_PATH] = { 0 };
	UrlFormating(pDUrl, "\\", FormatDownUrl);

	// "C:\\Command\\PWorkUnit.exe"
	sprintf_s(pFilePath, "%s\\pworkunit-v%s.exe", CommandFiler, pVersion);

	if (!doDownLoad(pFilePath, FormatDownUrl, pMd5))
	{
		RegCloseKey(hKey);
		return FALSE;
	}
	SetRegValue(hKey, "version", pVersion);
	SetRegValue(hKey, "path", pFilePath);
	printf("pworkunit.exe 更新完成\n");
	RegCloseKey(hKey);
	return TRUE;
}

void _stdcall io_ontimer_checkversion(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
#ifdef PROXY_DEBUG
	return;
#endif // PROXY_DEBUG

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

	cJSON* app_info[3];
	app_info[0] = cJSON_GetObjectItem(cjArray, "host-controller");
	if (NULL == app_info[0])
	{
		printf("获取 host-controller 版本信息失败\n");
		goto error;
	}
	app_info[1] = cJSON_GetObjectItem(cjArray, "app-upgrade");
	if (NULL == app_info[1])
	{
		printf("获取 app-upgrade 版本信息失败\n");
		goto error;
	}
	app_info[2] = cJSON_GetObjectItem(cjArray, "host-command");
	if (NULL == app_info[2])
	{
		printf("获取 app-pworkunit 版本信息失败\n");
		goto error;
	}

	if (!Proxy(app_info))
	{
		printf("升级代理失败\n");
		goto error;
	}

	if (!PWorkUnit(app_info))
	{
		printf("升级pworunit程序失败\n");
		goto error;
	}

error:
	if (NULL != root)
		cJSON_Delete(root);
	free(pResponseData->pData);
	free(pResponseData);
}

BOOL RegselfInfo()
{
	HKEY hKey = NULL;
	if (!PRegCreateKey(ManageModule, &hKey))
		return FALSE;

	char filepath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, filepath, MAX_PATH);
	if (!SetRegValue(hKey, "manage_path", filepath))
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
	return TRUE;
}

class connection_listener
{
	sio::client &handler;
public:
	connection_listener(sio::client& h):
		handler(h)
	{}
	
	void on_connected()
	{
		_lock.lock();
		_cond.notify_all();
		connect_finish = true;
		_lock.unlock();
	}

	void on_close(client::close_reason const& reason)
	{
		cout << "sio closed" << endl;
	}

	void on_fail()
	{
		cout << "sio failed" << endl;
	}
};

struct Json_Data
{
public:
	Json_Data()
	{
		pAction = NULL;
		//	pData = NULL;
		jData = NULL;
		pOld = NULL;
	}

	~Json_Data()
	{
		pAction = NULL;
		//	pData = NULL;
		jData = NULL;
		pOld = NULL;
	}
public:
	char* pAction;
	//char* pData;
	cJSON* jData;
	char* pOld;
};

void DisposeMap2String(std::map<std::string, message::ptr>& mp, char* buf)
{
	sprintf(buf + strlen(buf), "%s", "{");
	std::map<std::string, message::ptr>::iterator it = mp.begin();
	for (; it != mp.end(); it++)
	{
		if (it->second->get_flag() == message::flag::flag_string)
		{
			if (it == mp.begin())
				sprintf(buf + strlen(buf), "\"%s\":\"%s\"", it->first.c_str(), it->second->get_string().c_str());
			else
				sprintf(buf + strlen(buf), ",\"%s\":\"%s\"", it->first.c_str(), it->second->get_string().c_str());
		}
		else if (it->second->get_flag() == message::flag::flag_object)
		{
			if (it == mp.begin())
				sprintf(buf + strlen(buf), "\"%s\":", it->first.c_str());
			else
				sprintf(buf + strlen(buf), ",\"%s\":", it->first.c_str());
			DisposeMap2String(it->second->get_map(), buf);
		}
	}

	sprintf(buf + strlen(buf), "%s", "}");
}

cJSON* ParseJsonData(const char* _pInfo, Json_Data& jd)
{
	cJSON* root = NULL;
	root = cJSON_Parse(_pInfo);
	if (NULL == root)
		return NULL;

	cJSON* action = cJSON_GetObjectItem(root, "action");
	cJSON* data = cJSON_GetObjectItem(root, "data");

	if (NULL == action)
	{
		cJSON_Delete(root);
		return NULL;
	}

	jd.pAction = action->valuestring;
	if (NULL != data)
		jd.jData = data;
	jd.pOld = (char*)_pInfo;

	return root;
}

char* PackCallBackJsonData(Json_Data& jd, const char* pValue, BOOL bError = FALSE)
{
	cJSON* root = NULL;
	root = cJSON_CreateObject();
	if (NULL == root)
		return NULL;

	char CallBackAction[64];
	sprintf_s(CallBackAction, "%s__callback", jd.pAction);
	cJSON_AddStringToObject(root, "action", CallBackAction);

	cJSON* subroot = NULL;
	subroot = cJSON_CreateObject();
	if (NULL == subroot)
	{
		cJSON_Delete(root);
		return NULL;
	}

	cJSON_AddStringToObject(subroot, "value", pValue);
	if (bError)
		cJSON_AddStringToObject(subroot, "oldData", jd.pOld);

	cJSON_AddItemToObject(root, "data", subroot);

	char* pJsonData = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	return pJsonData;
}

void bind_events(socket::ptr& socket)
{
	socket->on("name", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck, message::list &ack_resp)
	{
		_lock.lock();
		cout << "监听到的事件:" << name << endl;
		cout << "接收到的数据:" << data->get_string() << endl;
		cout << "事件 " << name << " 处理完成" << endl;
		_lock.unlock();
	}));

	socket->on("data", sio::socket::event_listener_aux([&](string const& name, message::ptr const& data, bool isAck, message::list &ack_resp)
	{
		_lock.lock();
		cout << "监听到的事件: " << name << endl;
		cout << "事件中元素个数: " << (data->get_map()).size() << endl;
		char* pJsonData = (char*)malloc(BUF_SIZE);
		memset(pJsonData, 0x00, BUF_SIZE);
		DisposeMap2String(data->get_map(), pJsonData);
		printf("事件内容: %s\n", pJsonData);
		char* pBackJsonData = NULL;
		char PerformParameter[MAX_PATH];
		Json_Data jd;
		cJSON* root = ParseJsonData(pJsonData, jd);
		if (NULL == root)
		{
			printf("事件错误: 格式化错误\n");
			jd.pAction = "error";
			jd.pOld = pJsonData;
			pBackJsonData = PackCallBackJsonData(jd, "FORMERR", TRUE);
			PostThreadMessage(g_client_thread_id, CLIENT_SEND_2_SERVER, (WPARAM)pBackJsonData, NULL);
			free(pJsonData);
			_lock.unlock();
			return;
		}
		if (strcmp(jd.pAction, "host_restart") == 0)
		{ 
			pBackJsonData = PackCallBackJsonData(jd, "host_restart ok");
			sprintf_s(PerformParameter, "%s", jd.pAction);
			
		}else if (strcmp(jd.pAction, "proxy_restart") == 0)
		{ 
			pBackJsonData = PackCallBackJsonData(jd, "proxy_restart ok");
			sprintf_s(PerformParameter, "%s", jd.pAction);
		}
		else if (strcmp(jd.pAction, "dns_update") == 0)
		{
			if (NULL == jd.jData)
			{
				pBackJsonData = PackCallBackJsonData(jd, "dns is null", TRUE);
				PostThreadMessage(g_client_thread_id, CLIENT_SEND_2_SERVER, (WPARAM)pBackJsonData, NULL);
				goto error;
			}
			cJSON* jDns = cJSON_GetObjectItem(jd.jData, "dns");
			if (NULL == jDns)
			{
				pBackJsonData = PackCallBackJsonData(jd, "dns is null", TRUE);
				PostThreadMessage(g_client_thread_id, CLIENT_SEND_2_SERVER, (WPARAM)pBackJsonData, NULL);
				goto error;
			}
			char* pDns = jDns->valuestring;
			if (pDns == NULL || strcmp(pDns, "") == 0)
			{
				pBackJsonData = PackCallBackJsonData(jd, "dns is null", TRUE);
				PostThreadMessage(g_client_thread_id, CLIENT_SEND_2_SERVER, (WPARAM)pBackJsonData, NULL);
				goto error;
			}
			char* pdns1 = strtok(pDns, ",");
			char* pdns2 = strtok(NULL, ",");
			sprintf_s(PerformParameter, "%s %s %s", jd.pAction, pdns1, pdns2);
			pBackJsonData = PackCallBackJsonData(jd, "dns_update ok");
		}
		else if (strcmp(jd.pAction, "chrome_reinstall") == 0)
		{
			pBackJsonData = PackCallBackJsonData(jd, "chrome_reinstall ok");
			sprintf_s(PerformParameter, "%s", jd.pAction);
		}
		else if (strcmp(jd.pAction, "chrome_restart") == 0)
		{
			pBackJsonData = PackCallBackJsonData(jd, "chrome_restart ok");
			sprintf_s(PerformParameter, "%s", jd.pAction);
		}
		else if (strcmp(jd.pAction, "extension_reinstall") == 0)
		{
			pBackJsonData = PackCallBackJsonData(jd, "extension_reinstall ok");
			sprintf_s(PerformParameter, "%s", jd.pAction);
		}

		if (NULL == pBackJsonData)
		{
			printf("事件错误: 组织返回信息失败\n");
			goto error;
		}

		PostThreadMessage(g_client_thread_id, CLIENT_SEND_2_SERVER, (WPARAM)pBackJsonData, NULL);
		//ShellExecute(NULL, "open", Run_path, PerformParameter, NULL, SW_SHOWNORMAL);
		ShellExecute(NULL, "open", CommandPath, PerformParameter, NULL, SW_SHOWNORMAL);

	error:
		free(pJsonData);
		cJSON_Delete(root);
		cout << "事件 " << name << " 处理完成" << endl;
		_lock.unlock();
	}));
}

string url_b = "";
string url_a = "";

unsigned int _stdcall client_thread(LPVOID pVoid)
{
	sio::client h;
	socket::ptr current_socket;
	connection_listener l(h);
	char name[32] = { 0 };
	sprintf_s(name, "%s:proxy", g_pClient_id);
	h.set_host(name);

	url_a = SOCKET_IO_URL;
	string url(SOCKET_IO_URL);
	
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	SetEvent(hClientThreadStart);

	while (GetMessage(&msg, 0, 0, 0))
	{
		switch (msg.message)
		{
		case CLIENT_RELINK:
		{
			if (NULL != current_socket)
			{
				current_socket->off_all();
				current_socket->off_error();
				current_socket->close();
				current_socket.reset();
				h.sync_close();
				h.clear_con_listeners();
			}

			url = url_a;
			url_a = url_b;
			url_b = url;

			h.set_open_listener(std::bind(&connection_listener::on_connected, &l));
			h.set_close_listener(std::bind(&connection_listener::on_close, &l, std::placeholders::_1));
			h.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

			printf("%s\n", url.c_str());
			h.connect(url);
			_lock.lock();
			if (!connect_finish)
				_cond.wait(_lock);
			_lock.unlock();

			current_socket = h.get_ptr();

			bind_events(current_socket);
		}
		break;
		
		case CLIENT_SEND_2_SERVER:
		{
			char* pJsonData = (char*)msg.wParam;
			if (h.opened())
			{
				string info(pJsonData);
				current_socket->emit("data", info);
			}
			free(pJsonData);
			pJsonData = NULL;
		}
		break;
		default:
			break;
		}
	}

	return 0;
}

void _stdcall io_ontimer_resolve(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	ADDRINFOT* pAddrInfo = ResolveIp(SOCKET_IO_NAME, "443");
	if (NULL == pAddrInfo)
		return;

	char ResolveIp[32] = { 0 };
	sockaddr_in *p = (sockaddr_in*)pAddrInfo->ai_addr;
	char* pip = inet_ntoa(p->sin_addr);
	sprintf_s(ResolveIp, "https://%s:443/", pip);
}

unsigned int _stdcall io_ontimer_thread(void* pVoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	SetTimer(NULL, 1, 1000 * 60 * 10, io_ontimer_checkversion);
	SetTimer(NULL, 2, 1000 * 60 * 60 * 3, io_ontimer_resolve);

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

int main()
{
	printf("Current Version: %s\n", SOCKET_IO_VERSION);
	if (!RegselfInfo())
	{
		printf("注册管理模块信息失败\n");
		getchar();
		return 0;
	}

	if (!GetClient_id(&g_pClient_id))
	{
		printf("获取client_id失败\n");
		return 0;
	}

	io_ontimer_checkversion(0, 0, 0, 0);

	HKEY hKey = NULL;
	if (!PRegCreateKey(CommandExecute, &hKey))
	{
		printf("打开注册表 %s 失败\n", CommandExecute);
		getchar();
		return 0;
	}
	if (!GetRegValue(hKey, "path", CommandPath))
	{
		printf("获取命令执行程序地址失败\n");
		RegCloseKey(hKey);
		getchar();
		return 0;
	}

	RegCloseKey(hKey);


	ADDRINFOT* pAddrInfo = ResolveIp(SOCKET_IO_NAME, "443");
	if (NULL == pAddrInfo)
		return 0;

	char ResolveIp[32] = { 0 };
	sockaddr_in *p = (sockaddr_in*)pAddrInfo->ai_addr;
	char* pip = inet_ntoa(p->sin_addr);
	sprintf_s(ResolveIp, "https://%s:443/", pip);
	url_b = ResolveIp;
	//HANDLE hLogThread = (HANDLE)_beginthreadex(NULL, 0, log_thread, NULL, 0, &g_log_thread_id);

	HANDLE hTimerThread = (HANDLE)_beginthreadex(NULL, 0, io_ontimer_thread, NULL, 0, NULL);

	hClientThreadStart = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hClientThreadStart)
	{
		printf("hClientThreadStart error %d\n", GetLastError());
		return 0;
	}
	HANDLE hIoClientThread = (HANDLE)_beginthreadex(NULL, 0, client_thread, NULL, 0, &g_client_thread_id);
	if (NULL == hIoClientThread)
	{
		printf("创建 client_thread 线程失败\n");
		return 0;
	}
	WaitForSingleObject(hClientThreadStart, INFINITE);
	PostThreadMessage(g_client_thread_id, CLIENT_RELINK, 0, 0);

	getchar();
	return 0;
}