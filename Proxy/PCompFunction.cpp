#include "Proxy.h"
#include "PSock.h"
#include "ObjPool.h"
#include "PCompFunction.h"
#include "../PPPOE_Dial/PPPOEDial.h"

struct tcp_keepalive alive_in = { TRUE, 1000 * 10, 1000 };
struct tcp_keepalive alive_out = { 0 };
unsigned long ulBytesReturn = 0;

BOOL BindSObjWithCompPort(SOCKET_OBJ* obj)
{
	if (NULL == CreateIoCompletionPort((HANDLE)obj->sock, hCompPort, (ULONG_PTR)obj, 0))
	{
		printf("CreateIoCompletionPort failed with error: %d\n", WSAGetLastError());
		return FALSE;
	}

	if (SOCKET_ERROR == WSAIoctl(obj->sock, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in),
		&alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL))
	{
#ifdef _DEBUG
		printf("BindSObjWithCompPort WSAIoctl failed with error: %d\n", WSAGetLastError());
#endif // _DEBUG
	}

	return TRUE;
}

void Request_CONNECT(SOCKET_OBJ* c_sobj, BUFFER_OBJ* c_bobj)
{
	SOCKET_OBJ* s_sobj = NULL;
	BUFFER_OBJ* s_bobj = NULL;
	ADDRINFOT hints, *sAddrInfo = NULL;
	char UrlHost[MAX_PATH] = { 0 };
	char UrlPort[8] = "80";
	char* pUrlEnd = strchr(c_bobj->data + 8, ' ');
	if (NULL == pUrlEnd)
		goto error;
	int nUrlHostLen = pUrlEnd - c_bobj->data - 8;
	memcpy(UrlHost, c_bobj->data + 8, nUrlHostLen);
	
	char* pUrlPortStartPoint = strstr(UrlHost, ":");
	if (NULL != pUrlPortStartPoint)
	{
		int UrlLen = pUrlPortStartPoint - UrlHost;
		int nPortLen = nUrlHostLen - UrlLen - 1;
		*pUrlPortStartPoint = '\0';
		memcpy(UrlPort, pUrlPortStartPoint + 1, nPortLen);
		UrlPort[nPortLen] = '\0';
	}
	
	int err = 0;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	err = GetAddrInfo(UrlHost, UrlPort, &hints, &sAddrInfo);
	if (0 != err)
		goto error;
	
	s_sobj = allocSObj();
	if (NULL == s_sobj)
		goto error;

	s_bobj = allocBObj(g_dwPageSize);
	if (NULL == s_bobj)
		goto error;

	s_sobj->pRelatedBObj = s_bobj;
	s_bobj->pRelatedSObj = s_sobj;
	s_sobj->pPairedSObj = c_sobj;
	c_sobj->pPairedSObj = s_sobj;

	s_sobj->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == s_sobj->sock)
		goto error;

	struct sockaddr_in taddr;
	taddr.sin_family = AF_INET;
	taddr.sin_port = htons(0);
	taddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == bind(s_sobj->sock, (sockaddr*)&taddr, sizeof(taddr)))
		goto error;

	if (!BindSObjWithCompPort(s_sobj))
		goto error;

	s_bobj->SetIoRequestFunction(CONNECT_ConnectServerFailed, CONNECT_ConnectServerSuccess);
	s_sobj->sAddrInfo = sAddrInfo;
	DWORD dwBytes = 0;
	if (!lpfnConnectEx(s_sobj->sock, (sockaddr*)sAddrInfo->ai_addr, sAddrInfo->ai_addrlen, NULL, 0, &dwBytes, &s_bobj->ol))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
			goto error;
	}

	return;

error:
	if (NULL != sAddrInfo)
		FreeAddrInfo(sAddrInfo);

	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
	if (NULL != s_sobj)
	{
		if (INVALID_SOCKET != s_sobj->sock)
			PCloseSocket(s_sobj);
		freeSObj(s_sobj);
	}
	if (NULL != s_bobj)
		freeBObj(s_bobj);
	return;
}

void Request_GET(SOCKET_OBJ* c_sobj, BUFFER_OBJ* c_bobj, int nlen)
{
	SOCKET_OBJ* s_sobj = NULL;
	BUFFER_OBJ* s_bobj = NULL;
	ADDRINFOT hints, *sAddrInfo = NULL;
	char UrlHost[MAX_PATH] = { 0 };
	char UrlPort[8] = "80";
	int nLen = nlen;
	char* pUrlEnd = strchr(c_bobj->data + nLen + HTTP_TAG_LEN, '/');
	if (NULL == pUrlEnd)
		goto error;
	int nUrlHostLen = pUrlEnd - c_bobj->data - nLen - HTTP_TAG_LEN;
	memcpy(UrlHost, c_bobj->data + nLen + HTTP_TAG_LEN, nUrlHostLen);

	char* pUrlPortStartPoint = strstr(UrlHost, ":");
	if (NULL != pUrlPortStartPoint)
	{
		int UrlLen = pUrlPortStartPoint - UrlHost;
		int nPortLen = nUrlHostLen - UrlLen - 1;
		*pUrlPortStartPoint = '\0';
		memcpy(UrlPort, pUrlPortStartPoint + 1, nPortLen);
		UrlPort[nPortLen] = '\0';
	}

	//char* pHttp = strstr(pUrlEnd, " HTTP/1.1\r\n");
	//if (NULL == pHttp)
	//	goto error;
	//pHttp[8] = '0';

	//int l1 =  c_bobj->dwRecvedCount - (pUrlEnd - c_bobj->data);
	//memcpy(c_bobj->data + nLen, pUrlEnd, l1);
	//c_bobj->dwRecvedCount = nLen + l1;

	char* pProxy_Connection = StrStrI(pUrlEnd, "Proxy-Connection:");
	if (NULL == pProxy_Connection)
		goto error;
	char* pProxy_ConnectionEnd = strstr(pProxy_Connection, "\r\n");
	if (NULL == pProxy_ConnectionEnd)
		goto error;
	int nBeforePrxoy_ConnectionLen = pProxy_Connection - pUrlEnd;
	int nAfterPrxoy_ConnectionEndLen = c_bobj->dwRecvedCount - (pProxy_ConnectionEnd - c_bobj->data);
	int nNewConnectionLne = strlen("Connection: close");

	memcpy(c_bobj->data + nLen, pUrlEnd, nBeforePrxoy_ConnectionLen);
	nLen += nBeforePrxoy_ConnectionLen;
	memcpy(c_bobj->data + nLen, "Connection: close", nNewConnectionLne);
	nLen += nNewConnectionLne;
	memcpy(c_bobj->data + nLen, pProxy_ConnectionEnd, nAfterPrxoy_ConnectionEndLen);
	nLen += nAfterPrxoy_ConnectionEndLen;
	c_bobj->dwRecvedCount = nLen;

	int err = 0;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	err = GetAddrInfo(UrlHost, UrlPort, &hints, &sAddrInfo);
	if (0 != err)
		goto error;

	s_sobj = allocSObj();
	if (NULL == s_sobj)
		goto error;

	s_bobj = allocBObj(g_dwPageSize);
	if (NULL == s_bobj)
		goto error;

	s_bobj->pRelatedSObj = c_sobj;
	c_sobj->pRelatedBObj = s_bobj;
	c_bobj->pRelatedSObj = s_sobj;
	s_sobj->pRelatedBObj = c_bobj;
	s_sobj->pPairedSObj = c_sobj;
	c_sobj->pPairedSObj = s_sobj;

	s_sobj->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == s_sobj->sock)
		goto error;

	struct sockaddr_in taddr;
	taddr.sin_family = AF_INET;
	taddr.sin_port = htons(0);
	taddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == bind(s_sobj->sock, (sockaddr*)&taddr, sizeof(taddr)))
		goto error;

	if (!BindSObjWithCompPort(s_sobj))
		goto error;

	c_bobj->SetIoRequestFunction(GET_ConnectServerFailed, GET_ConnectServerSuccess);
	s_sobj->sAddrInfo = sAddrInfo;
	DWORD dwBytes = 0;
	if (!lpfnConnectEx(s_sobj->sock, (sockaddr*)sAddrInfo->ai_addr, sAddrInfo->ai_addrlen, c_bobj->data, c_bobj->dwRecvedCount, &dwBytes, &c_bobj->ol))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
			goto error;
	}

	return;

error:
	if (NULL != sAddrInfo)
		FreeAddrInfo(sAddrInfo);

	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
	if (NULL != s_sobj)
	{
		if (INVALID_SOCKET != s_sobj->sock)
			PCloseSocket(s_sobj);
		freeSObj(s_sobj);
	}
	if (NULL != s_bobj)
		freeBObj(s_bobj);
	return;
}

void ParsingRequestHeader(SOCKET_OBJ* c_sobj, BUFFER_OBJ* c_bobj)
{
	if (c_bobj->data[0] == 'C')
	{
		Request_CONNECT(c_sobj, c_bobj);
	}
	else if (c_bobj->data[0] == 'P')
	{
		Request_GET(c_sobj, c_bobj, 5);
	}
	else if (c_bobj->data[0] == 'G')
	{
		Request_GET(c_sobj, c_bobj, 4);
	}
}

void AcceptCompFailed(void* _lobj, void* _c_bobj)
{
	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(lobj->sListenSock, &c_bobj->ol, &dwTranstion, FALSE, &dwFlags))
		printf("AcceptCompFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

	lobj->DeleteBObjFromAccpetExPendingList(c_bobj);
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void AcceptCompSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return AcceptCompFailed(_lobj, _c_bobj);

	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

	lobj->DeleteBObjFromAccpetExPendingList(c_bobj);

	if (!BindSObjWithCompPort(c_sobj))
	{
		printf("BindSObjWithCompPort failed\n");
		goto error;
	}

	setsockopt(c_sobj->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&lobj->sListenSock, sizeof(lobj->sListenSock));

	c_bobj->dwRecvedCount += dwTranstion;

	SOCKADDR* localAddr = NULL,
		*remoteAddr = NULL;
	int localAddrlen,
		remoteAddrlen;

	lpfnGetAcceptExSockaddrs(c_bobj->data, c_bobj->datalen - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&localAddr, &localAddrlen,
		&remoteAddr, &remoteAddrlen);

#ifdef _DEBUG
	EnterCriticalSection(&g_csLog);
	printf("accept: %s", c_bobj->data);
	LeaveCriticalSection(&g_csLog);
#endif // _DEBUG

	if (NULL == strstr(c_bobj->data, "\r\n\r\n"))
	{
		c_bobj->SetIoRequestFunction(RecvRequestHeaderFailed, RecvRequestHeaderSuccess);
		if (!PostRecv(c_sobj, c_bobj))
		{
			printf("AcceptCompSuccess PostRecv failed error: %d\n", WSAGetLastError());
			goto error;
		}
		return;
	}
	else
	{
		ParsingRequestHeader(c_sobj, c_bobj);
	}

	return;

error:
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void RecvRequestHeaderFailed(void* _c_sobj, void* _c_bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(c_sobj->sock, &c_bobj->ol, &dwTranstion, FALSE, &dwFlags))
		printf("RecvRequestHeaderFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void RecvRequestHeaderSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return RecvRequestHeaderFailed(_c_sobj, _c_bobj);

	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;

	c_bobj->dwRecvedCount += dwTranstion;

	if (NULL == strstr(c_bobj->data, "\r\n\r\n"))
	{
		if (!PostRecv(c_sobj, c_bobj))
		{
			printf("RecvRequestHeaderSuccess PostRecv failed error: %d\n", WSAGetLastError());
			goto error;
		}
		return;
	}
	else
	{
		ParsingRequestHeader(c_sobj, c_bobj);
	}

	return;

error:
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void CONNECT_ConnectServerFailed(void* _s_sobj, void* _s_bobj)
{
	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;
	SOCKET_OBJ* c_sobj = s_sobj->pPairedSObj;
	BUFFER_OBJ* c_bobj = c_sobj->pRelatedBObj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
}

void CONNECT_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj)
{
	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;
	SOCKET_OBJ* c_sobj = s_sobj->pPairedSObj;
	BUFFER_OBJ* c_bobj = c_sobj->pRelatedBObj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	int nSeconds, 
		nBytes = sizeof(nSeconds), 
		nErr = 0;

	nErr = getsockopt(s_sobj->sock, SOL_SOCKET, SO_CONNECT_TIME, (char*)&nSeconds, &nBytes);
	if (SOCKET_ERROR == nErr)
		goto error;

	if (0xffffffff == nSeconds)
		goto error;

	const char pResponse[] = "HTTP/1.0 200 Connection established\r\nProxy-agent: HTTP Proxy Lite /0.2\r\n\r\n";
	strcpy_s(c_bobj->data, c_bobj->datalen, pResponse);
	c_bobj->dwRecvedCount = strlen(pResponse);

	c_bobj->SetIoRequestFunction(SendReponseFailed, SendReponseSuccess);
	if (!PostSend(c_sobj, c_bobj))
		goto error;

	return;

error:
	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
	return;
}

void SendReponseFailed(void* _c_sobj, void* _c_bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* s_sobj = c_sobj->pPairedSObj;
	BUFFER_OBJ* s_bobj = s_sobj->pRelatedBObj;
	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
}

void SendReponseSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return SendReponseFailed(_c_sobj, _c_bobj);

	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* s_sobj = c_sobj->pPairedSObj;
	BUFFER_OBJ* s_bobj = s_sobj->pRelatedBObj;

	c_bobj->dwSendedCount += dwTranstion;
	if (c_bobj->dwSendedCount < c_bobj->dwRecvedCount)
	{
		if (!PostSend(c_sobj, c_bobj))
			goto error;
		return;
	}

	s_sobj->pRef = &c_sobj->nRef;
	c_sobj->pRef = s_sobj->pRef;

	c_bobj->dwRecvedCount = 0;
	c_bobj->dwSendedCount = 0;
	c_bobj->SetIoRequestFunction(RecvCompFailed, RecvCompSuccess);
	InterlockedIncrement(c_sobj->pRef);
	if (!PostRecv(c_sobj, c_bobj))
	{
		InterlockedDecrement(c_sobj->pRef);
		goto error;
	}

	s_bobj->dwRecvedCount = 0;
	s_bobj->dwSendedCount = 0;
	s_bobj->SetIoRequestFunction(RecvCompFailed, RecvCompSuccess);
	InterlockedIncrement(s_sobj->pRef);
	if (!PostRecv(s_sobj, s_bobj))
	{
		PCloseSocket(c_sobj);
		if (0 == InterlockedDecrement(s_sobj->pRef))
			goto error;
	}

	return;

error:
	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
	return;
}

void RecvCompFailed(void* _sobj, void* _bobj)
{
	SOCKET_OBJ* pCurrentSObj = (SOCKET_OBJ*)_sobj;
	SOCKET_OBJ* pPairedSObj = pCurrentSObj->pPairedSObj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(pCurrentSObj->sock, &((BUFFER_OBJ*)_bobj)->ol, &dwTranstion, FALSE, &dwFlags))
		printf("RecvCompFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

//	shutdown(pCurrentSObj->sock, SD_BOTH);
	shutdown(pPairedSObj->sock, SD_BOTH);

	if (0 == InterlockedDecrement(pCurrentSObj->pRef))
	{
		BUFFER_OBJ* pCurrentBObj = pCurrentSObj->pRelatedBObj;
		BUFFER_OBJ* pPairedBObj = pPairedSObj->pRelatedBObj;
		PCloseSocket(pCurrentSObj);
		PCloseSocket(pPairedSObj);
		freeSObj(pCurrentSObj);
		freeSObj(pPairedSObj);
		freeBObj(pCurrentBObj);
		freeBObj(pPairedBObj);
	}
}

void RecvCompSuccess(DWORD dwTransion, void* _sobj, void* _bobj)
{
	if (dwTransion <= 0)
		return RecvCompFailed(_sobj, _bobj);

	SOCKET_OBJ* pCurrentSObj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* pCurrentBObj = (BUFFER_OBJ*)_bobj;

	pCurrentBObj->dwRecvedCount += dwTransion;
	pCurrentBObj->SetIoRequestFunction(SendCompFailed, SendCompSuccess);

	SOCKET_OBJ* pPairedSObj = pCurrentSObj->pPairedSObj;
	if (!PostSend(pPairedSObj, pCurrentBObj))
	{
		PCloseSocket(pCurrentSObj);
		PCloseSocket(pPairedSObj);
		if (0 == InterlockedDecrement(pPairedSObj->pRef))
			goto error;
	}

	return;

error:
	BUFFER_OBJ* pPairedBObj = pPairedSObj->pRelatedBObj;
	PCloseSocket(pCurrentSObj);
	PCloseSocket(pPairedSObj);
	freeSObj(pCurrentSObj);
	freeSObj(pPairedSObj);
	freeBObj(pCurrentBObj);
	freeBObj(pPairedBObj);
}

void SendCompFailed(void* _sobj, void* _bobj)
{
	SOCKET_OBJ* pCurrentSObj = (SOCKET_OBJ*)_sobj;
	SOCKET_OBJ* pPairedSObj = pCurrentSObj->pPairedSObj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(pCurrentSObj->sock, &((BUFFER_OBJ*)_bobj)->ol, &dwTranstion, FALSE, &dwFlags))
		printf("RecvCompFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

//	shutdown(pCurrentSObj->sock, SD_BOTH);
	shutdown(pPairedSObj->sock, SD_BOTH);

	if (0 == InterlockedDecrement(pCurrentSObj->pRef))
	{
		BUFFER_OBJ* pCurrentBObj = pCurrentSObj->pRelatedBObj;
		BUFFER_OBJ* pPairedBObj = pPairedSObj->pRelatedBObj;
		PCloseSocket(pCurrentSObj);
		PCloseSocket(pPairedSObj);
		freeSObj(pCurrentSObj);
		freeSObj(pPairedSObj);
		freeBObj(pCurrentBObj);
		freeBObj(pPairedBObj);
	}
}

void SendCompSuccess(DWORD dwTransion, void* _sobj, void* _bobj)
{
	if (dwTransion <= 0)
		return SendCompFailed(_sobj, _bobj);

	SOCKET_OBJ* pCurrentSObj = (SOCKET_OBJ*)_sobj;
	BUFFER_OBJ* pCurrentBObj = (BUFFER_OBJ*)_bobj;

	SOCKET_OBJ* pPairedSObj = pCurrentSObj->pPairedSObj;

	pCurrentBObj->dwSendedCount += dwTransion;
	if (pCurrentBObj->dwSendedCount < pCurrentBObj->dwRecvedCount)
	{
		if (!PostSend(pCurrentSObj, pCurrentBObj))
		{
			PCloseSocket(pCurrentSObj);
			PCloseSocket(pPairedSObj);
			if (0 == InterlockedDecrement(pCurrentSObj->pRef))
				goto error;
		}
		return;
	}

	pCurrentBObj->dwRecvedCount = 0;
	pCurrentBObj->dwSendedCount = 0;
	pCurrentBObj->SetIoRequestFunction(RecvCompFailed, RecvCompSuccess);
	if (!PostRecv(pPairedSObj, pCurrentBObj))
	{
		PCloseSocket(pCurrentSObj);
		PCloseSocket(pPairedSObj);
		if (0 == InterlockedDecrement(pPairedSObj->pRef))
			goto error;
	}

	return;

error:
	BUFFER_OBJ* pPairedBObj = pPairedSObj->pRelatedBObj;
	freeSObj(pCurrentSObj);
	freeSObj(pPairedSObj);
	freeBObj(pCurrentBObj);
	freeBObj(pPairedBObj);
}

void GET_ConnectServerFailed(void* _s_sobj, void* _s_bobj)
{
	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;
	SOCKET_OBJ* c_sobj = s_sobj->pPairedSObj;
	BUFFER_OBJ* c_bobj = c_sobj->pRelatedBObj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
}

void GET_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj)
{
	if (dwTranstion <= 0)
		return GET_ConnectServerFailed(_s_sobj, _s_bobj);

	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;
	SOCKET_OBJ* c_sobj = s_sobj->pPairedSObj;
	BUFFER_OBJ* c_bobj = c_sobj->pRelatedBObj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	s_bobj->dwSendedCount += dwTranstion;
	if (c_bobj->dwSendedCount < c_bobj->dwRecvedCount)
	{
		if (!PostSend(s_sobj, s_bobj))
			goto error;
		return;
	}

	s_sobj->pRef = &c_sobj->nRef;
	c_sobj->pRef = s_sobj->pRef;

	c_bobj->dwRecvedCount = 0;
	c_bobj->dwSendedCount = 0;
	c_bobj->SetIoRequestFunction(RecvCompFailed, RecvCompSuccess);
	InterlockedIncrement(c_sobj->pRef);
	if (!PostRecv(c_sobj, c_bobj))
	{
		InterlockedDecrement(c_sobj->pRef);
		goto error;
	}

	s_bobj->dwRecvedCount = 0;
	s_bobj->dwSendedCount = 0;
	s_bobj->SetIoRequestFunction(RecvCompFailed, RecvCompSuccess);
	InterlockedIncrement(s_sobj->pRef);
	if (!PostRecv(s_sobj, s_bobj))
	{
		PCloseSocket(c_sobj);
		if (0 == InterlockedDecrement(s_sobj->pRef))
			goto error;
	}

	return;

error:
	PCloseSocket(s_sobj);
	PCloseSocket(c_sobj);
	freeSObj(s_sobj);
	freeSObj(c_sobj);
	freeBObj(s_bobj);
	freeBObj(c_bobj);
	return;
}

void Accept6086CompFailed(void* _lobj, void* _c_bobj)
{
	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(lobj->sListenSock, &c_bobj->ol, &dwTranstion, FALSE, &dwFlags))
		printf("Accept6086CompFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

	lobj->DeleteBObjFromAccpetExPendingList(c_bobj);
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void Accept6086CompSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return Accept6086CompFailed(_lobj, _c_bobj);

	LISTEN_OBJ* lobj = (LISTEN_OBJ*)_lobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;
	SOCKET_OBJ* c_sobj = c_bobj->pRelatedSObj;

	lobj->DeleteBObjFromAccpetExPendingList(c_bobj);

	if (!BindSObjWithCompPort(c_sobj))
	{
		printf("BindSObjWithCompPort failed\n");
		goto error;
	}

	setsockopt(c_sobj->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&lobj->sListenSock, sizeof(lobj->sListenSock));

	c_bobj->dwRecvedCount += dwTranstion;

	SOCKADDR* localAddr = NULL,
		*remoteAddr = NULL;
	int localAddrlen,
		remoteAddrlen;

	lpfnGetAcceptExSockaddrs(c_bobj->data, c_bobj->datalen - ((sizeof(sockaddr_in) + 16) * 2),
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&localAddr, &localAddrlen,
		&remoteAddr, &remoteAddrlen);

#ifdef _DEBUG
	EnterCriticalSection(&g_csLog);
	printf("accept: %s", c_bobj->data);
	LeaveCriticalSection(&g_csLog);
#endif // _DEBUG

	char* pHeader = NULL;
	if (NULL == (pHeader = strstr(c_bobj->data, "\r\n\r\n")))
	{
		c_bobj->SetIoRequestFunction(Recv6086RequestHeaderFailed, Recv6086RequestHeaderSuccess);
		if (!PostRecv(c_sobj, c_bobj))
		{
			printf("Accept6086CompSuccess PostRecv failed error: %d\n", WSAGetLastError());
			goto error;
		}
		return;
	}
	else
	{
		int HeaderLen = pHeader - c_bobj->data;
		HeaderLen += strlen("\r\n\r\n");
		int len = strlen("Content-Length: ");
		char* pContentLength = StrStrI(c_bobj->data, "Content-Length: ");
		if (NULL == pContentLength)
			goto error;
		char* pContentLengthEnd = strstr(pContentLength, "\r\n");
		if (NULL == pContentLengthEnd)
			goto error;
		int nLengthLen = pContentLengthEnd - pContentLength - len;
		char Length[8] = { 0 };
		memcpy_s(Length, 8, pContentLength + len, nLengthLen);
		len = atoi(Length);
		printf("%d\n", len);
		if ((HeaderLen + len) > (int)c_bobj->dwRecvedCount)
		{
			c_bobj->SetIoRequestFunction(Recv6086RequestHeaderFailed, Recv6086RequestHeaderSuccess);
			if (!PostRecv(c_sobj, c_bobj))
			{
				printf("Accept6086CompSuccess PostRecv failed error: %d\n", WSAGetLastError());
				goto error;
			}
			return;
		}
		else if ((HeaderLen + len) == (int)c_bobj->dwRecvedCount)
		{
			send(c_sobj->sock, "ok", 2, 0);
			PCloseSocket(c_sobj);
			freeSObj(c_sobj);
			freeBObj(c_bobj);
			closesocket(g_lobj6086->sListenSock);
			WaitForSingleObject(g_hDoingNetWork, INFINITE);
			PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, NULL, NULL);
			printf("ok\n");
		}
		else if ((HeaderLen + len) < (int)c_bobj->dwRecvedCount)
			printf("error\n");
	}

	return;

error:
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void Recv6086RequestHeaderFailed(void* _c_sobj, void* _c_bobj)
{
	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;

#ifdef _DEBUG
	DWORD dwTranstion = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(c_sobj->sock, &c_bobj->ol, &dwTranstion, FALSE, &dwFlags))
		printf("Recv6086RequestHeaderFailed error: %d\n", WSAGetLastError());
#endif // _DEBUG

	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void Recv6086RequestHeaderSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj)
{
	if (dwTranstion <= 0)
		return Recv6086RequestHeaderFailed(_c_sobj, _c_bobj);

	SOCKET_OBJ* c_sobj = (SOCKET_OBJ*)_c_sobj;
	BUFFER_OBJ* c_bobj = (BUFFER_OBJ*)_c_bobj;

	c_bobj->dwRecvedCount += dwTranstion;

	char* pHeader = NULL;
	if (NULL == (pHeader = strstr(c_bobj->data, "\r\n\r\n")))
	{
		if (!PostRecv(c_sobj, c_bobj))
		{
			printf("Recv6086RequestHeaderSuccess PostRecv failed error: %d\n", WSAGetLastError());
			goto error;
		}
		return;
	}
	else
	{
		int HeaderLen = pHeader - c_bobj->data;
		HeaderLen += strlen("\r\n\r\n");
		int len = strlen("Content-Length: ");
		char* pContentLength = StrStrI(c_bobj->data, "Content-Length: ");
		if (NULL == pContentLength)
			goto error;
		char* pContentLengthEnd = strstr(pContentLength, "\r\n");
		if (NULL == pContentLengthEnd)
			goto error;
		int nLengthLen = pContentLengthEnd - pContentLength - len;
		char Length[8] = { 0 };
		memcpy_s(Length, 8, pContentLength + len, nLengthLen);
		len = atoi(Length);
		printf("%d\n", len);
		if ((HeaderLen + len) > (int)c_bobj->dwRecvedCount)
		{
			if (!PostRecv(c_sobj, c_bobj))
			{
				printf("Recv6086RequestHeaderSuccess PostRecv failed error: %d\n", WSAGetLastError());
				goto error;
			}
			return;
		}
		else if ((HeaderLen + len) == (int)c_bobj->dwRecvedCount)
		{
			send(c_sobj->sock, "ok", 2, 0);
			PCloseSocket(c_sobj);
			freeSObj(c_sobj);
			freeBObj(c_bobj);
			closesocket(g_lobj6086->sListenSock);
			WaitForSingleObject(g_hDoingNetWork, INFINITE);
			PostThreadMessage(g_switch_threadId, SWITCH_REDIAL, NULL, NULL);
			printf("ok\n");
		}
		else if ((HeaderLen + len) < (int)c_bobj->dwRecvedCount)
			printf("error\n");
	}

	return;

error:
	PCloseSocket(c_sobj);
	freeSObj(c_sobj);
	freeBObj(c_bobj);
}

void Server_CONNECT(u_short nPort)
{
	SOCKET_OBJ* s_sobj = NULL;
	BUFFER_OBJ* s_bobj = NULL;
	ADDRINFOT hints, *sAddrInfo = NULL;
	
	int err = 0;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

#ifdef PROXY_DEBUG
	err = GetAddrInfo("127.0.0.1", "5001", &hints, &sAddrInfo);
#else
	char szPort[8] = { 0 };
	_itoa_s(nPort, szPort, 10);
	err = GetAddrInfo(vip[pArrayIndex[nIndex]], szPort, &hints, &sAddrInfo);
#endif // PROXY_DEBUG
	if (0 != err)
		goto error;

	s_sobj = allocSObj();
	if (NULL == s_sobj)
		goto error;

	s_bobj = allocBObj(g_dwPageSize);
	if (NULL == s_bobj)
		goto error;

	s_sobj->pRelatedBObj = s_bobj;
	s_bobj->pRelatedSObj = s_sobj;

	s_sobj->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == s_sobj->sock)
		goto error;

	struct sockaddr_in taddr;
	taddr.sin_family = AF_INET;
	taddr.sin_port = htons(0);
	taddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == bind(s_sobj->sock, (sockaddr*)&taddr, sizeof(taddr)))
		goto error;

	if (!BindSObjWithCompPort(s_sobj))
		goto error;

	s_bobj->SetIoRequestFunction(SERVER_ConnectServerFailed, SERVER_ConnectServerSuccess);
	s_sobj->sAddrInfo = sAddrInfo;
	DWORD dwBytes = 0;
	if (!lpfnConnectEx(s_sobj->sock, (sockaddr*)sAddrInfo->ai_addr, sAddrInfo->ai_addrlen, NULL, 0, &dwBytes, &s_bobj->ol))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
			goto error;
	}

	return;

error:
	if (NULL != sAddrInfo)
		FreeAddrInfo(sAddrInfo);

	if (NULL != s_sobj)
	{
		if (INVALID_SOCKET != s_sobj->sock)
			PCloseSocket(s_sobj);
		freeSObj(s_sobj);
	}
	if (NULL != s_bobj)
		freeBObj(s_bobj);
	return;
}

void SERVER_ConnectServerFailed(void* _s_sobj, void* _s_bobj)
{
	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	PCloseSocket(s_sobj);
	freeSObj(s_sobj);
	freeBObj(s_bobj);
}

void SERVER_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj)
{
	SOCKET_OBJ* s_sobj = (SOCKET_OBJ*)_s_sobj;
	BUFFER_OBJ* s_bobj = (BUFFER_OBJ*)_s_bobj;

	FreeAddrInfo(s_sobj->sAddrInfo);

	int nSeconds,
		nBytes = sizeof(nSeconds),
		nErr = 0;

	nErr = getsockopt(s_sobj->sock, SOL_SOCKET, SO_CONNECT_TIME, (char*)&nSeconds, &nBytes);
	if (SOCKET_ERROR == nErr)
		goto error;

	if (0xffffffff == nSeconds)
		goto error;

	s_bobj->dwRecvedCount = 0;
	s_bobj->dwSendedCount = 0;
	s_bobj->SetIoRequestFunction(RecvRequestHeaderFailed, RecvRequestHeaderSuccess);
	if (!PostRecv(s_sobj,s_bobj))
	{
		printf("SERVER_ConnectServerSuccess PostRecv failed error: %d\n", WSAGetLastError());
		goto error;
	}

	return;

error:
	PCloseSocket(s_sobj);
	freeSObj(s_sobj);
	freeBObj(s_bobj);
	return;
}