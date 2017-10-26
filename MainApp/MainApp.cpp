#include <Windows.h>
#include <stdio.h>
#include <process.h>

unsigned int _stdcall test(LPVOID pVoid);

unsigned int _stdcall closetest(LPVOID pVoid);

HANDLE hTest = NULL;

int main()
{
	hTest = (HANDLE)_beginthreadex(NULL, 0, test, NULL, 0, NULL);
	HANDLE hCloseTest = (HANDLE)_beginthreadex(NULL, 0, closetest, NULL, 0, NULL);
	DWORD dw = WaitForSingleObject(hTest, INFINITE);
	printf("%d\n", dw);

	getchar();
	return 0;
}

unsigned int _stdcall test(LPVOID pVoid)
{
	do
	{
		printf("thread alive\n");
		Sleep(1000);
	} while (true);
	return 0;
}

unsigned int _stdcall closetest(LPVOID pVoid)
{
	Sleep(1000 * 10);

	TerminateThread(hTest, 1);
//	exit(0);
	return 0;
}