#include "ShareApi.h"

char g_username[16];
char g_password[16];
HANDLE g_hDoingNetWork = NULL;
char g_AdslIp[16];
BOOL bSwithMode1 = TRUE;
unsigned int g_switch_threadId = 0;
HANDLE g_hSwitchThreadStart = NULL;
char* g_pClient_id = NULL;
int g_nPort = 0;
int nIndex = 0;
int nCountOfArray = 0;
int *pArrayIndex = NULL;
vector<char*> vip;

BOOL ExcCmd(const char* cmd, char** out)
{
	FILE* pipe = _popen(cmd, "r");
	if (!pipe)
		return FALSE;

	int nTotal = 0;
	int len = 0;
	char buffer[128];
	char* pData = (char*)malloc(512);
	ZeroMemory(pData, 512);
	int nReallocSize = 512;

	while (!feof(pipe))
	{
		if (fgets(buffer, 128, pipe))
		{
			len = strlen(buffer) + 1;
			nTotal += len;
			if (nTotal > nReallocSize)
			{
				nReallocSize = MY_ALIGN((nTotal + 1), 8);
				pData = (char*)realloc(pData, nReallocSize);
			}
			strcat_s(pData, nReallocSize, buffer);
		}
	}

	_pclose(pipe);
	*out = pData;
	return TRUE;
}

BOOL CloseTheSpecifiedProcess(const char* ProcessName)
{
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hProcessSnap)
	{
		printf_s("创建 \"%s\" 系统进程映射失败 error = %d\n", ProcessName, GetLastError());
		return FALSE;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);

	BOOL bFind = Process32First(hProcessSnap, &pe32);
	while (bFind)
	{
		if (0 == strcmp(pe32.szExeFile, ProcessName))
		{
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
			TerminateProcess(hProcess, 0);
			CloseHandle(hProcess);
		}
		bFind = Process32Next(hProcessSnap, &pe32);
	}

	CloseHandle(hProcessSnap);

	return TRUE;
}

BOOL CloseRasphone()
{
	char* pAdslExit = NULL;
	char* pCmdInfo = NULL;

	do
	{
		if (NULL != pCmdInfo)
		{
			free(pCmdInfo);
			pCmdInfo = NULL;
		}

		if (!ExcCmd("rasdial adsl /disconnect", &pCmdInfo))
			continue;
		pAdslExit = strstr(pCmdInfo, "没有连接");
		if (NULL == pAdslExit)
			pAdslExit = strstr(pCmdInfo, "No connections");
	} while (NULL == pAdslExit);

	if (NULL != pCmdInfo)
	{
		free(pCmdInfo);
		pCmdInfo = NULL;
	}

	return CloseTheSpecifiedProcess("rasphone.exe");
}

BOOL GetAdslInfo(char* ip)
{
	PIP_ADAPTER_INFO pAdslAdapterInfo_ = NULL;
	PIP_ADAPTER_INFO pAdslAdapterInfo = (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO));

	unsigned long nInfoSize = 0;
	ULONG uLong = GetAdaptersInfo(pAdslAdapterInfo, &nInfoSize);
	if (ERROR_BUFFER_OVERFLOW == uLong)
	{
		free(pAdslAdapterInfo);
		pAdslAdapterInfo = (PIP_ADAPTER_INFO)malloc(nInfoSize);
		uLong = GetAdaptersInfo(pAdslAdapterInfo, &nInfoSize);
	}

	if (ERROR_SUCCESS == uLong)
	{
		pAdslAdapterInfo_ = pAdslAdapterInfo;
		while (pAdslAdapterInfo)
		{
			if (pAdslAdapterInfo->Type == MIB_IF_TYPE_PPP)
			{
				IP_ADDR_STRING* pAdslIpAddrString = &(pAdslAdapterInfo->IpAddressList);
				memcpy_s(ip, 16, pAdslIpAddrString->IpAddress.String, 16);
				if (pAdslAdapterInfo_)
				{
					free(pAdslAdapterInfo_);
					pAdslAdapterInfo_ = NULL;
				}
				return TRUE;
			}
			pAdslAdapterInfo = pAdslAdapterInfo->Next;
		}
	}

	if (pAdslAdapterInfo_)
	{
		free(pAdslAdapterInfo_);
		pAdslAdapterInfo_ = NULL;
	}

	return FALSE;
}

BOOL CreateShortcuts(const char* exePath, const char* pArguments, const char* pWorkingDirectory, const char* lnkNmae)
{
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
		return FALSE;

	IShellLink* pisl;
	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pisl);
	if (FAILED(hr))
	{
		CoUninitialize();
		return FALSE;
	}
		
	IPersistFile* pIPF;
	pisl->SetPath(exePath);
	pisl->SetArguments(pArguments);
	pisl->SetWorkingDirectory(pWorkingDirectory);

	hr = pisl->QueryInterface(IID_IPersistFile, (void**)&pIPF);
	if (FAILED(hr))
	{
		pisl->Release();
		CoUninitialize();
		return FALSE;
	}
	
	USES_CONVERSION;
	LPCOLESTR lpOleStr = A2COLE(lnkNmae);
	pIPF->Save(lpOleStr, FALSE);
	pIPF->Release();
	pisl->Release();
	CoUninitialize();

	return TRUE;
}

wchar_t* ConvertUtf8ToUnicode_(const char* utf8)
{
	if (!utf8)
	{
		wchar_t* buf = (wchar_t*)malloc(2);
		memset(buf, 0, 2);
		return buf;
	}
	int nLen = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCSTR)utf8, -1, NULL, 0);
	//返回需要的unicode长度  
	WCHAR * wszUNICODE = new WCHAR[nLen + 1];
	memset(wszUNICODE, 0, nLen * 2 + 2);
	nLen = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)utf8, -1, wszUNICODE, nLen);    //把utf8转成unicode
	return wszUNICODE;
}

BOOL DeleteDirectoryByFullName(const char* pFilename)
{
	char* pCmdRetinfo = NULL;
	char cCmd[MAX_PATH] = { 0 };
	sprintf_s(cCmd, "rd /s /q %s", pFilename);
	if (!ExcCmd(cCmd, &pCmdRetinfo))
	{
		printf("执行命令： %s 时失败\n", cCmd);
		return FALSE;
	}

	if (NULL != pCmdRetinfo)
	{
		printf("%s\n", pCmdRetinfo);
		free(pCmdRetinfo);
	}

	return TRUE;
}

BOOL CheckReservedIp(const char* ip)
{
	static const char *address_blocks[] = {
		"0.0.0.0/8",
		"10.0.0.0/8",
		"100.64.0.0/10",
		"127.0.0.0/8",
		"169.254.0.0/16",
		"172.16.0.0/12",
		"192.0.0.0/24",
		"192.0.2.0/24",
		"192.88.99.0/24",
		"192.168.0.0/16",
		"198.18.0.0/15",
		"198.51.100.0/24",
		"203.0.113.0/24",
		"224.0.0.0/4",
		"240.0.0.0/4",
		"255.255.255.255/32"
	};

	static const int blk_nums = sizeof(address_blocks) / sizeof(*address_blocks);
	static char buf[40];

	for (int i = 0; i < blk_nums; ++i) {
		strncpy_s(buf, address_blocks[i], sizeof(buf));
		char *pos = strchr(buf, '/');
		if (pos == NULL)
		{
			return FALSE;
		}
		*pos = '\0';
		ULONG val = htonl(inet_addr(buf)); // big endian
		int msk = atoi(pos + 1);
		int offset = 32 - msk;

		ULONG tar = htonl(inet_addr(ip));

		if ((val >> offset) == (tar >> offset))
			return TRUE;
	}

	return FALSE;
}

void CleanString(char* str)
{
	char *start = str - 1;
	char *end = str;
	char *p = str;
	while (*p)
	{
		switch (*p)
		{
		case ' ':
		case '\r':
		case '\n':
		{
			if (start + 1 == p)
				start = p;
		}
		break;
		default:
			break;
		}
		++p;
	}
	//现在来到了字符串的尾部 反向向前
	--p;
	++start;
	if (*start == 0)
	{
		//已经到字符串的末尾了
		*str = 0;
		return;
	}
	end = p + 1;
	while (p > start)
	{
		switch (*p)
		{
		case ' ':
		case '\r':
		case '\n':
		{
			if (end - 1 == p)
				end = p;
		}
		break;
		default:
			break;
		}
		--p;
	}
	memmove(str, start, end - start);
	*(str + (int)end - (int)start) = 0;
}

BOOL GetClient_id(char** pClient_id)
{
	if (*pClient_id != NULL)
	{
		return TRUE;
	}
	FILE* fHostId = NULL;
	fopen_s(&fHostId, "C:\\client_id.txt", "r");
	if (NULL == fHostId)
	{
		printf("fopen C:\\client_id.txt failed wiht error code : %d", WSAGetLastError());
		return FALSE;
	}

	fseek(fHostId, 0, SEEK_END);
	int FileLen = ftell(fHostId);
	fseek(fHostId, 0, SEEK_SET);

	char* p = NULL;
	p = new char[FileLen + 1];// ftell返回的字节数不包含'\0'
	memset(p, 0x00, FileLen + 1);

	int nReadBytes = fread(p, FileLen + 1, 1, fHostId);

	CleanString(p);

	fclose(fHostId);

	*pClient_id = p;

	return TRUE;
}

DWORD dwRandIndex = 0;
void GetRandIndex(int arrayIndex[], int nCount)
{
	for (int i = 0; i < nCount; i++)
	{
		arrayIndex[i] = -1;
	}

	for (int i = 0; i < nCount;)
	{
		try
		{
			time_t t;
			srand((unsigned)(time(&t) + dwRandIndex++));
			int a = rand() % (nCount);
			for (int j = 0; j < i; j++)
			{
				if (arrayIndex[j] == a)
					throw new int(0);
			}
			arrayIndex[i] = a;
			i++;
		}
		catch (int *err)
		{
			delete err;
			continue;
		}
	}
}

char password[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'g',
	'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
	'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', '+', '-', '*', '^', '%', '$', '#', '@',
	'!', '?', '(', ')', '[', ']', '{', '}', '=', '_'
};

char username[] = {
	'w', 'x', 'y', 'z', 'A', 'B', 'C',
	'k', 'l', 'm', 'n', 'o', 'p', 'q',
	'r', 's', 't', 'O', 'P', 'Q', 'R',
	'S', 'T', 'U', 'V', 'W', 'X', '0',
	'1', '2', '6', '7', '8', '9', 'a',
	'b', 'c', 'd', 'e', 'f', 'g', 'h',
	'i', 'g', 'u', 'v', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'Y', 'Z', '3', '4', '5'
};

#define COUNT_OF_USERNAME sizeof(username)/sizeof(char)
#define COUNT_OF_PASSWORD sizeof(password)/sizeof(char)
int ArrayIndexOfUsername[COUNT_OF_USERNAME];
int ArrayIndexOfPassword[COUNT_OF_PASSWORD];

void GetUsernameAndPassword()
{
	GetRandIndex(ArrayIndexOfUsername, COUNT_OF_USERNAME);
	int i = 0;
	for (i = 0; i < 8; i++)
	{
		g_username[i] = username[ArrayIndexOfUsername[i]];
	}
	i++;
	g_username[i] = '\0';
	GetRandIndex(ArrayIndexOfPassword, COUNT_OF_PASSWORD);
	for (i = 0; i < 12; i++)
	{
		g_password[i] = password[ArrayIndexOfPassword[i]];
	}
	i++;
	g_password[i] = '\0';
}

void spliteVersion(char *p, int* pout)
{
	char *p1 = strstr(p, ".");
	if (p1 != NULL)
	{
		int n = p1 - p;
		char buf[8] = { 0 };
		memcpy(buf, p, n);
		pout[0] = atoi(buf);
		pout += 1;
		p = p1 + 1;
		spliteVersion(p, pout);
	}
	else
	{
		pout[0] = atoi(p);
	}
}

int compV(int* l, int* s, int* times)
{
	*times -= 1;
	if (l[0] > s[0])
	{
		return 0;
	}
	if (l[0] < s[0])
	{
		return -1;
	}
	if (l[0] == s[0])
	{
		if (*times > 0)
		{
			s += 1;
			l += 1;
			return compV(l, s, times);
		}
		else
		{
			return 0;
		}
	}
	return 0;
}

BOOL CompareVersion(char* pl, char* ps)
{
	int* plint = new int[3];
	int* psint = new int[3];
	spliteVersion(pl, plint);
	spliteVersion(ps, psint);
	int times = 3;
	if (compV(plint, psint, &times) == 0)
	{
		delete[] plint;
		delete[] psint;
		return TRUE;
	}
	else
	{
		delete[] plint;
		delete[] psint;
		return FALSE;
	}
	return TRUE;
}

void UrlFormating(char *purl, char* pt, char *pu)
{
	char *p = strstr(purl, pt);
	if (p != NULL)
	{
		int n = p - purl;
		memcpy(pu, purl, n);
		pu += n;
		purl += n + 1;
		UrlFormating(purl, pt, pu);
	}
	else
	{
		strcat_s(pu, MAX_PATH, purl);
	}
}

void GetGuid(char* buf)
{
	GUID guid;
	CoCreateGuid(&guid);
	sprintf_s(buf, 128, "%08X%04X%04x%02X%02X%02X%02X%02X%02X%02X%02X"
		, guid.Data1
		, guid.Data2
		, guid.Data3
		, guid.Data4[0], guid.Data4[1]
		, guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5]
		, guid.Data4[6], guid.Data4[7]
	);
}

int GetListenPort()
{
	int nPort = 0;
#ifdef PROXY_DEBUG
	nPort = 5005;
#else
	char szBuf[256] = { 0 };
	GetGuid(szBuf);

	char szAllDigit[256] = { 0 };
	int nDigitCount = 0;
	for (size_t i = 0; i < strlen(szBuf); i++)
	{
		if (isdigit(szBuf[i]))
		{
			if (nDigitCount == 0 && szBuf[i] == '0')
				continue;
			szAllDigit[nDigitCount++] = szBuf[i];
		}
		else
			continue;
	}

	while (nDigitCount < 6)
	{
		szAllDigit[nDigitCount] = '3';
	}

	char szPort[32] = { 0 };
	memcpy(szPort, szAllDigit, 5);
	nPort = atoi(szPort);
	if (nPort > 63000)
		nPort /= 2;
#endif // PROXY_DEBUG

	return nPort;
}

#define HOST_URL "pservers.disi.se"

BOOL GetServerIpFromHost()
{
	nCountOfArray = 0;
	if (NULL != pArrayIndex)
	{
		delete pArrayIndex;
		pArrayIndex = NULL;
	}

	for (size_t i = 0; i < vip.size(); i++)
	{
		if (NULL == vip[i])
			continue;
		delete vip[i];
		vip[i] = NULL;
	}
	vip.clear();

	HOSTENT *hostent = gethostbyname(HOST_URL);
	if (NULL == hostent)
	{
		printf("解析服务器域名失败:%d\n", WSAGetLastError());
		return FALSE;
	}

	for (int i = 0; hostent->h_addr_list[i]; i++)
	{
		in_addr inad = *((in_addr*)hostent->h_addr_list[i]);
		char *pip = new char[16];
		memcpy(pip, inet_ntoa(inad), strlen(inet_ntoa(inad)) + 1);
		vip.push_back(pip);
	}

	nCountOfArray = vip.size();
	if (nCountOfArray == 0)
	{
		printf("没有解析出任何ip数据\n");
		return FALSE;
	}

	pArrayIndex = new int[nCountOfArray];
	if (NULL == pArrayIndex)
	{
		printf("ip索引分配失败\n");
		return FALSE;
	}

	return TRUE;
}

#ifndef USE_SHARE_API
int main()
{
	getchar();
	return 0;
}
#endif // !USE_SHARE_API
