#include "../ShareApi/ShareApi.h"
#include "../CurlClient/CurlClient.h"

HANDLE hCheckNetEvent = NULL;
HANDLE hCheckVersionEvent = NULL;

void _stdcall ontimer_checkversion(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	if (WaitForSingleObject(hCheckVersionEvent, 0) == WAIT_TIMEOUT)
		return;
}

void _stdcall ontimer_checknet(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	if (WaitForSingleObject(hCheckNetEvent, 0) == WAIT_TIMEOUT)
		return;
}

unsigned int _stdcall ontimer_thread(void* pVoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	hCheckNetEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	hCheckVersionEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

	SetTimer(NULL, 1, 1000 * 60, ontimer_checknet);
	SetTimer(NULL, 2, 1000 * 60 * 10, ontimer_checkversion);

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

#ifndef USE_ONTIMER_CPP
int main()
{
	getchar();
	return 0;
}
#endif // !USE_ONTIMER_CPP