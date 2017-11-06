#include "../ShareApi/ShareApi.h"
#include "HbServer.h"

unsigned int _stdcall hb_server(LPVOID pVoid)
{
	WSADATA wsadata;
	int err = 0;
	err = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (0 != err)
	{
		printf("WSAStartup failed with error: %d\n", err);
		return 0;
	}

	SOCKET sSock = INVALID_SOCKET;
	sSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == sSock)
	{
		WSACleanup();
		return 0;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(6083);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == bind(sSock, (sockaddr*)&addr, sizeof(addr)) ||
		SOCKET_ERROR == listen(sSock, 1))
	{
		if (WSAEADDRINUSE == WSAGetLastError())
			printf("6083端口被占用\n");

		closesocket(sSock);
		sSock = INVALID_SOCKET;
		WSACleanup();
		return 0;
	}

	SOCKET sAccept = INVALID_SOCKET;
	while (true)
	{
		if (INVALID_SOCKET != sAccept)
		{
			closesocket(sAccept);
			sAccept = INVALID_SOCKET;
		}
		struct sockaddr_in taddr;
		int len = sizeof(taddr);
		sAccept = WSAAccept(sSock, (sockaddr*)&taddr, &len, NULL, NULL);
		if (INVALID_SOCKET == sAccept)
		{
			printf("接收监控程序连接失败 err = %d\n", WSAGetLastError());
			Sleep(1000 * 5);
			continue;
		}

		DWORD nTimeOut = 5 * 1000;
		setsockopt(sSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeOut, sizeof(DWORD));

		int nRecvLen = 0;
		int nInfoTotalLen = 512;
		char* pRecvInfo = (char*)malloc(512);
		memset(pRecvInfo, 0x00, 512);
		do
		{
			if (nRecvLen >= nInfoTotalLen)
				break;

			err = recv(sAccept, pRecvInfo + nRecvLen, nInfoTotalLen - nRecvLen, 0);
			if (err > 0)
				nRecvLen += err;
			else
			{
				if (SOCKET_ERROR == err)
				{
					printf("6083端口在使用中出现错误 error = %d\n", WSAGetLastError());
					break;
				}
			}

			if (strstr(pRecvInfo, "\r\n\r\n") != NULL)
			{
				send(sAccept, "ok", 2, 0);
				break;
			}
		} while (TRUE);

		free(pRecvInfo);
	}

	WSACleanup();
	return 0;
}