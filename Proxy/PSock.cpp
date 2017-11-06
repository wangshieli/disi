#include "Proxy.h"
#include "PSock.h"
#include "ObjPool.h"
#include "PCompFunction.h"

GUID GuidAcceptEx = WSAID_ACCEPTEX,
GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS,
GuidConnectEx = WSAID_CONNECTEX;

LPFN_ACCEPTEX lpfnAccpetEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;
LPFN_CONNECTEX lpfnConnectEx = NULL;

BOOL Init()
{
	WSADATA wsadata;
	int err = 0;
	err = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (0 != err)
	{
		printf("WSAStartup failed with error: %d\n", err);
		return FALSE;
	}

	SOCKET sSock = INVALID_SOCKET;
	sSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == sSock)
	{
		printf("WSAIoctl failed with error: %d\n", WSAGetLastError());
		return FALSE;
	}

	DWORD dwBytes = 0;
	err = WSAIoctl(sSock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&lpfnAccpetEx, sizeof(lpfnAccpetEx),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == err)
	{
		printf("WSAIoctl GET \"AcceptEx\" failed with error: %d\n", WSAGetLastError());
		closesocket(sSock);
		return FALSE;
	}

	err = WSAIoctl(sSock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
		&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == err)
	{
		printf("WSAIoctl GET \"GetAcceptExSockaddrs\" failed with error: %d\n", WSAGetLastError());
		closesocket(sSock);
		return FALSE;
	}

	err = WSAIoctl(sSock,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidConnectEx, sizeof(GuidConnectEx),
		&lpfnConnectEx, sizeof(lpfnConnectEx),
		&dwBytes, NULL, NULL);
	if (SOCKET_ERROR == err)
	{
		printf("WSAIoctl GET \"ConnectEx\" failed with error: %d\n", WSAGetLastError());
		closesocket(sSock);
		return FALSE;
	}

	return TRUE;
}

void PCloseSocket(SOCKET_OBJ* obj)
{
	if (NULL != obj)
	{
		if (INVALID_SOCKET != obj->sock)
		{
			closesocket(obj->sock);
			obj->sock = INVALID_SOCKET;
		}
	}
}

LISTEN_OBJ* LSock_Array[WSA_MAXIMUM_WAIT_EVENTS];
HANDLE hEvent_Array[WSA_MAXIMUM_WAIT_EVENTS];
DWORD g_dwListenPortCount = 0;

BOOL InitListenSock(LISTEN_OBJ* lobj, u_short port)
{
	lobj->sListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == lobj->sListenSock)
		return FALSE;

	struct sockaddr_in laddr;
	ZeroMemory(&laddr, sizeof(laddr));
	laddr.sin_family = AF_INET;
	laddr.sin_port = htons(port);
	laddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind(lobj->sListenSock, (SOCKADDR*)&laddr, sizeof(laddr)))
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		return FALSE;
	}

	lobj->hPostAcceptExEvent = WSACreateEvent();
	if (NULL == lobj->hPostAcceptExEvent)
		goto error;

	LSock_Array[g_dwListenPortCount] = lobj;
	hEvent_Array[g_dwListenPortCount++] = lobj->hPostAcceptExEvent;
	if (SOCKET_ERROR == WSAEventSelect(lobj->sListenSock, lobj->hPostAcceptExEvent, FD_ACCEPT))
		goto error;

	if (NULL == CreateIoCompletionPort((HANDLE)lobj->sListenSock, hCompPort, (ULONG_PTR)lobj, 0))
		goto error;

	if (SOCKET_ERROR == listen(lobj->sListenSock, SOMAXCONN))
		goto error;

	return TRUE;

error:
	if (INVALID_SOCKET != lobj->sListenSock)
	{
		closesocket(lobj->sListenSock);
		lobj->sListenSock = INVALID_SOCKET;
	}

	if (NULL != lobj->hPostAcceptExEvent)
	{
		WSACloseEvent(lobj->hPostAcceptExEvent);
		lobj->hPostAcceptExEvent = NULL;
	}

	// 数组中需要修改

	return FALSE;
}

BOOL PostAcceptEx(LISTEN_OBJ* lobj, int n)
{
	DWORD dwBytes = 0;
	SOCKET_OBJ* c_sobj = NULL;
	BUFFER_OBJ* c_bobj = NULL;

	c_bobj = allocBObj(g_dwPageSize);
	if (NULL == c_bobj)
		goto error;

	lobj->AddBObj2AccpetExPendingList(c_bobj);

	c_sobj = allocSObj();
	if (NULL == c_sobj)
		goto error;

	c_sobj->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == c_sobj->sock)
		goto error;

	c_sobj->pRelatedBObj = c_bobj;
	c_bobj->pRelatedSObj = c_sobj;

	if (n == 0)
		c_bobj->SetIoRequestFunction(AcceptCompFailed, AcceptCompSuccess);
	else if (n == 1)
		c_bobj->SetIoRequestFunction(Accept6086CompFailed, Accept6086CompSuccess);

	BOOL bRetVal = FALSE;
	bRetVal = lpfnAccpetEx(lobj->sListenSock,
		c_sobj->sock, c_bobj->data,
		c_bobj->datalen - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&dwBytes, &c_bobj->ol);
	if (!bRetVal)
	{
		int err = WSAGetLastError();
		if (WSA_IO_PENDING != err)
		{
			printf("lpfnAccpetEx failed with error: %d\n", err);
			goto error;
		}
	}

	return TRUE;

error:
	printf("PostAcceptEx Error\n");
	if (NULL != c_bobj)
	{
		lobj->DeleteBObjFromAccpetExPendingList(c_bobj);
		freeBObj(c_bobj);
	}

	if (NULL != c_sobj)
	{
		PCloseSocket(c_sobj);
		freeSObj(c_sobj);
	}
	
	return FALSE;
}

BOOL PostZeroRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0,
		dwFlags = 0;

	int err = 0;

	_bobj->wsaBuf.buf = NULL;
	_bobj->wsaBuf.len = 0;

	err = WSARecv(_sobj->sock, &_bobj->wsaBuf, 1, &dwBytes, &dwFlags, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}

BOOL PostRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0,
		dwFlags = 0;

	int err = 0;

	_bobj->wsaBuf.buf = _bobj->data + _bobj->dwRecvedCount;
	_bobj->wsaBuf.len = _bobj->datalen - _bobj->dwRecvedCount;

	err = WSARecv(_sobj->sock, &_bobj->wsaBuf, 1, &dwBytes, &dwFlags, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}

BOOL PostSend(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj)
{
	DWORD dwBytes = 0;
	int err = 0;

	_bobj->wsaBuf.buf = _bobj->data + _bobj->dwSendedCount;
	_bobj->wsaBuf.len = _bobj->dwRecvedCount - _bobj->dwSendedCount;

	err = WSASend(_sobj->sock, &_bobj->wsaBuf, 1, &dwBytes, 0, &_bobj->ol, NULL);
	if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		return FALSE;

	return TRUE;
}