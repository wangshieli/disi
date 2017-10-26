#include "Proxy.h"
#include "ObjPool.h"
#include <vector>

using std::vector;

CRITICAL_SECTION g_csSObj;
CRITICAL_SECTION g_csBObj;

vector<SOCKET_OBJ*> g_VSObj;
vector<BUFFER_OBJ*> g_VBObj;

SOCKET_OBJ* allocSObj()
{
	SOCKET_OBJ* obj = NULL;

	EnterCriticalSection(&g_csSObj);
	if (g_VSObj.empty())
	{
		obj = (SOCKET_OBJ*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SIZE_OF_SOCKET_OBJ);
	}
	else
	{
		obj = g_VSObj.back();
		g_VSObj.pop_back();
	}
	LeaveCriticalSection(&g_csSObj);

	if (obj)
		obj->init();

	return obj;
}

void freeSObj(SOCKET_OBJ* obj)
{
	EnterCriticalSection(&g_csSObj);
	if (g_VSObj.size() < 400)
		g_VSObj.push_back(obj);
	else
	{
		HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, obj);
	}
	LeaveCriticalSection(&g_csSObj);
}

BUFFER_OBJ* allocBObj(DWORD nSize)
{
	BUFFER_OBJ* obj = NULL;
	EnterCriticalSection(&g_csBObj);
	if (g_VBObj.empty())
	{
		obj = (BUFFER_OBJ*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, nSize + SIZE_OF_BUFFER_OBJ_T);
	}
	else
	{
		obj = g_VBObj.back();
		g_VBObj.pop_back();
	}
	LeaveCriticalSection(&g_csBObj);

	if (obj)
		obj->init(nSize);

	return obj;
}

void freeBObj(BUFFER_OBJ* obj)
{
	EnterCriticalSection(&g_csBObj);
	if (g_VBObj.size() < 400)
		g_VBObj.push_back(obj);
	else
	{
		HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, obj);
	}
	LeaveCriticalSection(&g_csBObj);
}