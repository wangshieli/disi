#include "../ShareApi/ShareApi.h"
#include "../ModifyDNSServers/ModifyDNSServers.h"

// 重启主机: host_restart
// 重启代理: proxy_restart
// 更新dns: dns_update
// chrome重装: chrome_reinstall
// chrome重启: chrome_restart
// 插件重装: extension_reinstall

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("参数错误\n");
		goto error;
	}

	printf("操作类型: %s\n", argv[1]);
	if (strcmp(argv[1], "host_restart") == 0)
	{
		ComputerRestart();
	}
	else if (strcmp(argv[1], "proxy_restart") == 0)
	{
		ProxyRestart();
	}
	else if (strcmp(argv[1], "dns_update") == 0)
	{
		if (argc < 3)
		{
			printf("dns_update 参数错误\n");
			goto error;
		}

		if (argc == 3)
			ModifyDnsServer(argv[2], NULL);
		else if(argc == 4)
		{
			ModifyDnsServer(argv[2], argv[3]);
		}

		// 向6085发送拨号请求
		SOCKET sock = INVALID_SOCKET;
		int err = ConnectToDisiServer(sock, "127.0.0.1", 6085);
		if (0 != err)
		{
			printf("连接6085端口失败\n");
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
		ProxyRestart();
	}
	else if (strcmp(argv[1], "chrome_restart") == 0)
	{
		
	}
	else if (strcmp(argv[1], "extension_reinstall") == 0)
	{
		CloseTheSpecifiedProcess("chrome.exe");
		Sleep(1000 * 2);
		DeleteDirectoryByFullName("\"C:\\Program Files\\chrome\\userdata\"");
		Sleep(1000 * 5);
		ProxyRestart();
	}

	return 0;

error:
	getchar();
	return -1;
}