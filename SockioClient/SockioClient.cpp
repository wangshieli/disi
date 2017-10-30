#include "../ShareApi/ShareApi.h"
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>

#include "../CurlClient/CurlClient.h"

#include "../Proxy/cJSON.h"

#include "sockio\sio_client.h"

#pragma warning(disable:4996)

#define BUF_SIZE 512
#define CLIENT_RELINK WM_USER + 501
#define CLIENT_SEND_2_SERVER WM_USER + 502
#define SOCKET_IO_URL "https://host-notification.disi.se/"

using namespace std;
using namespace sio;

char Run_path[MAX_PATH];

mutex _lock;
condition_variable_any _cond;
bool connect_finish = false;

unsigned int g_client_thread_id = 0;

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

int main()
{
	InstallChrome();
	return 0;
}

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
		ShellExecute(NULL, "open", Run_path, PerformParameter, NULL, SW_SHOWNORMAL);

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
	char Client_id[32] = { 0 };
	sio::client h;
	socket::ptr current_socket;
	connection_listener l(h);
	char name[32] = { 0 };
	sprintf_s(name, "%s:proxy", Client_id);
	h.set_host(name);

	string url(SOCKET_IO_URL);
	
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

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
				h.sync_close();
				h.clear_con_listeners();
			}

			h.set_open_listener(std::bind(&connection_listener::on_connected, &l));
			h.set_close_listener(std::bind(&connection_listener::on_close, &l, std::placeholders::_1));
			h.set_fail_listener(std::bind(&connection_listener::on_fail, &l));

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