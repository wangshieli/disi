#pragma once

#include "../ShareApi/ShareApi.h"
#include "cJSON.h"
#include "md5.h"

#define PROXY_VERSION "proxy-v0.1.33"

typedef void(*PTIoRequestSuccess)(DWORD dwTranstion, void* key, void* buf);
typedef void(*PTIoRequestFailed)(void* key, void* buf);

#define HTTP_TAG "http://"
#define HTTP_TAG_LEN strlen(HTTP_TAG)

typedef struct _buffer_obj
{
public:
	WSAOVERLAPPED ol;
	PTIoRequestFailed pfnFailed;
	PTIoRequestSuccess pfnSuccess;
	struct _buffer_obj *pNext, *pPrev;
	struct _socket_obj* pRelatedSObj;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	int datalen;
	char data[1];
public:
	void init(DWORD usefull_space)
	{
		ZeroMemory(&ol, sizeof(ol));
		pfnFailed = NULL;
		pfnSuccess = NULL;
		pNext = pPrev = NULL;
		pRelatedSObj = NULL;
		dwRecvedCount = 0;
		dwSendedCount = 0;
		datalen = usefull_space;
		ZeroMemory(data, usefull_space);
	}

	void SetIoRequestFunction(PTIoRequestFailed _pfnFailed, PTIoRequestSuccess _pfnSuccess)
	{
		pfnFailed = _pfnFailed;
		pfnSuccess = _pfnSuccess;
	}
}BUFFER_OBJ;
#define SIZE_OF_BUFFER_OBJ sizeof(BUFFER_OBJ)

typedef struct _buffer_obj_t
{
	WSAOVERLAPPED ol;
	PTIoRequestFailed pfnFailed;
	PTIoRequestSuccess pfnSuccess;
	struct _buffer_obj *pNext, *pPrev;
	struct _socket_obj* pRelatedSObj;
	WSABUF wsaBuf;
	DWORD dwRecvedCount;
	DWORD dwSendedCount;
	int datalen;
//	char data[1];
}BUFFER_OBJ_T;
#define SIZE_OF_BUFFER_OBJ_T sizeof(BUFFER_OBJ_T)

typedef struct _socket_obj
{
public:
	SOCKET sock;
	struct _buffer_obj* pRelatedBObj;
	struct _socket_obj* pPairedSObj;
	volatile long nRef;
	volatile long* pRef;
	ADDRINFOT* sAddrInfo;
public:
	void init()
	{
		sock = INVALID_SOCKET;
		pRelatedBObj = NULL;
		pPairedSObj = NULL;
		nRef = 0;
		pRef = NULL;
		sAddrInfo = NULL;
	}
}SOCKET_OBJ;
#define SIZE_OF_SOCKET_OBJ sizeof(SOCKET_OBJ)

typedef struct _listen_obj
{
public:
	SOCKET sListenSock;
	CRITICAL_SECTION cs;
	struct _buffer_obj* pAcceptExPendingList;
	DWORD dwAcceptExPendingCount;
	HANDLE hPostAcceptExEvent;
public:
	void init()
	{
		sListenSock = INVALID_SOCKET;
		InitializeCriticalSection(&cs);
		pAcceptExPendingList = NULL;
		dwAcceptExPendingCount = 0;
		hPostAcceptExEvent = NULL;
	}

	void AddBObj2AccpetExPendingList(struct _buffer_obj* obj)
	{
		EnterCriticalSection(&cs);
		obj->pNext = pAcceptExPendingList;
		if (pAcceptExPendingList)
			pAcceptExPendingList->pPrev = obj;
		obj->pPrev = NULL;
		pAcceptExPendingList = obj;
		++dwAcceptExPendingCount;
		LeaveCriticalSection(&cs);
	}

	void DeleteBObjFromAccpetExPendingList(struct _buffer_obj* obj)
	{
		EnterCriticalSection(&cs);
		if (obj == pAcceptExPendingList)
		{
			pAcceptExPendingList = pAcceptExPendingList->pNext;
			if (NULL != pAcceptExPendingList)
				pAcceptExPendingList->pPrev = NULL;
		}
		else
		{
			obj->pPrev->pNext = obj->pNext;
			if (NULL != obj->pNext)
				obj->pNext->pPrev = obj->pNext;
		}
		--dwAcceptExPendingCount;
		LeaveCriticalSection(&cs);
	}
}LISTEN_OBJ;

extern HANDLE hCompPort;
extern DWORD g_dwPageSize;

#ifdef _DEBUG
extern CRITICAL_SECTION g_csLog;
#endif // _DEBUG

extern LISTEN_OBJ* LSock_Array[];
extern HANDLE hEvent_Array[];
extern DWORD g_dwListenPortCount;

extern LISTEN_OBJ* g_lobj;
extern LISTEN_OBJ* g_lobj6086;

extern SOCKET g_5001socket;
extern SOCKET g_6086socket;