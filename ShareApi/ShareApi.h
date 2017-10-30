#pragma once

#include <WinSock2.h>

#include <MSWSock.h>
#include <MSTcpIP.h>
#include <WS2tcpip.h>

#include <stdio.h>

#include <IPHlpApi.h>
#include <TlHelp32.h>
#include <ShObjIdl.h>
#include <ShlGuid.h>
#include <ShlObj.h>
#include <atlconv.h>
#include <Shlwapi.h>

#include <GPEdit.h>

#include <CommCtrl.h>

#include <string>

#include <process.h>

#include <io.h>
#include <vector>

using namespace std;

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "IPHlpApi.lib")
#pragma comment(lib, "Shlwapi.lib")

#define PROXY_TEST 0

#define SWITCH_MODE1	WM_USER + 1
#define SWITCH_MODE2	WM_USER + 2
#define SWITCH_REDIAL	WM_USER + 3

#define LOG_MESSAGE		WM_USER + 4

#if PROXY_TEST
#define PROXY_DEBUG
#endif

#define MY_ALIGN(size, boundary) (((size) + ((boundary) - 1 )) & ~((boundary) - 1))

#define NP_THE_ONE_INSTANCE "Global\\np_The_One_Instance_Event"

BOOL ExcCmd(const char* cmd, char** out);

BOOL CloseTheSpecifiedProcess(const char* ProcessName);

BOOL CloseRasphone();

BOOL GetAdslInfo(char* ip);

BOOL CreateShortcuts(const char* exePath, const char* pArguments, const char* pWorkingDirectory, const char* lnkNmae);

wchar_t* ConvertUtf8ToUnicode_(const char* utf8);

BOOL DeleteDirectoryByFullName(const char* pFilename);

BOOL CheckReservedIp(const char* ip);

void CleanString(char* str);

BOOL GetClient_id(char** pClient_id);

void GetRandIndex(int arrayIndex[], int nCount);

void GetUsernameAndPassword();

BOOL CompareVersion(char* pl, char* ps);

void UrlFormating(char *purl, char* pt, char *pu);

int GetListenPort();

BOOL GetServerIpFromHost();

BOOL ComputerRestart();

BOOL ProxyRestart();

int ConnectToDisiServer(SOCKET& sock5001, const char* ServerIP, unsigned short ServerPort);

unsigned int _stdcall log_thread(LPVOID);

ADDRINFOT* ResolveIp(const char* _host, const char* _port);

extern HANDLE hTheOneInstance;
extern char g_username[];
extern char g_password[];
extern HANDLE g_hDoingNetWork;
extern char g_AdslIp[];
extern BOOL bSwithMode1;
extern unsigned int g_switch_threadId;
extern char* g_pClient_id;
extern HANDLE g_hSwitchThreadStart;
extern int g_nPort;
extern int nIndex;
extern int nCountOfArray;
extern int *pArrayIndex;
extern vector<char*> vip;
extern unsigned int g_log_thread_id;