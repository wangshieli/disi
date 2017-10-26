#include "../ShareApi/ShareApi.h"

#include "ModifyDNSServers.h"

int IpStr2Hex(const char* Ip)
{
	int a, b, c, d;
	sscanf_s(Ip, "%d.%d.%d.%d", &a, &b, &c, &d);
	
	a <<= 24;
	b <<= 16;
	c <<= 8;
	a = a + b + c + d;

	return a;
}

void SelectSysListControl(HWND hwnd)
{
	int count, i;
	char item[512] = { 0 }, subitem[512] = { 0 };

	LVITEM lvi, *_lvi;
	char *_item, *_subitem;
	DWORD pid;
	HANDLE process;

	count = (int)SendMessage(hwnd, LVM_GETITEMCOUNT, 0, 0);

	GetWindowThreadProcessId(hwnd, &pid);
	process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ |
		PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);

	_lvi = (LVITEM*)VirtualAllocEx(process, NULL, sizeof(LVITEM),
		MEM_COMMIT, PAGE_READWRITE);
	_item = (char*)VirtualAllocEx(process, NULL, 512, MEM_COMMIT,
		PAGE_READWRITE);
	_subitem = (char*)VirtualAllocEx(process, NULL, 512, MEM_COMMIT,
		PAGE_READWRITE);

	lvi.cchTextMax = 512;

	int nTcpIpIndex = 0;
	for (i = 0; i<count; i++) {
		lvi.iSubItem = 0;
		lvi.pszText = _item;
		WriteProcessMemory(process, _lvi, &lvi, sizeof(LVITEM), NULL);
		SendMessage(hwnd, LVM_GETITEMTEXT, (WPARAM)i, (LPARAM)_lvi);

		if (strcmp(item, "Internet 协议 (TCP/IP)") == 0)
		{
			nTcpIpIndex = i;
			break;
		}
	}

	lvi.mask = LVIF_STATE;
	lvi.iSubItem = 0;
	lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
	lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
	WriteProcessMemory(process, _lvi, &lvi, sizeof(LVITEM), NULL);
	SendMessage(hwnd, LVM_SETITEMSTATE, (WPARAM)nTcpIpIndex, (LPARAM)_lvi);

	VirtualFreeEx(process, _lvi, 0, MEM_RELEASE);
	VirtualFreeEx(process, _item, 0, MEM_RELEASE);
	VirtualFreeEx(process, _subitem, 0, MEM_RELEASE);
}

HWND FindWindowByName(const char* szText)
{
	int nWaitTime = 0;
	HWND hwnd = NULL;
	do
	{
		Sleep(1000 * 2);
		nWaitTime += 2;
		hwnd = FindWindow(NULL, szText);
	} while (NULL == hwnd && nWaitTime < 30);

	return hwnd;
}

BOOL SwitchSysTabCtrl(HWND hwnd)
{
	HWND hTabCtrl = FindWindowEx(hwnd, NULL, "SysTabControl32", NULL);
	if (NULL == hTabCtrl)
		return FALSE;

	int nTabCtrlCount = TabCtrl_GetItemCount(hTabCtrl);
	if (0 == nTabCtrlCount)
		return FALSE;;

	for (int i = 0; i < nTabCtrlCount; i++)
		TabCtrl_SetCurFocus(hTabCtrl, i);

	return TRUE;
}

BOOL ModifyDnsServer(const char* dns1, const char* dns2)
{
	while (true)
	{
		CloseRasphone();
		WinExec("rasphone -d adsl", SW_NORMAL);

		Sleep(1000 * 4);
		HWND hAdsl = NULL;
		hAdsl = FindWindow(NULL, "连接 adsl");
		if (NULL == hAdsl)
			continue;

		HWND hProButton = FindWindowEx(hAdsl, NULL, "Button", "属性(&O)");
		if (NULL == hProButton)
			goto error;

		PostMessage(hProButton, BM_CLICK, 0, 0);

		HWND hAdsl_pro = NULL;
		hAdsl_pro = FindWindowByName("adsl 属性");
		if (NULL == hAdsl_pro)
			goto error;

		if (!SwitchSysTabCtrl(hAdsl_pro))
			goto error;

		HWND hNet = FindWindowEx(hAdsl_pro, NULL, "#32770", "网络");
		if (NULL == hNet)
			goto error;

		HWND hListCtrl = FindWindowEx(hNet, NULL, "SysListView32", NULL);
		if (NULL == hListCtrl)
			goto error;

		SelectSysListControl(hListCtrl);

		HWND hSetButton = FindWindowEx(hNet, NULL, "Button", "属性(&R)");
		if (NULL == hSetButton)
			goto error;
		PostMessage(hSetButton, BM_CLICK, 0, 0);

		HWND hTcpIp = NULL;
		hTcpIp = FindWindowByName("Internet 协议 (TCP/IP) 属性");
		if (NULL == hTcpIp)
			goto error;

		if (!SwitchSysTabCtrl(hTcpIp))
			goto error;

		HWND hRoutineCard = FindWindowEx(hTcpIp, NULL, "#32770", "常规");
		if (NULL == hRoutineCard)
			goto error;

		HWND hDnsButton = FindWindowEx(hRoutineCard, NULL, "Button", "使用下面的 DNS 服务器地址(&E):");
		if (NULL == hDnsButton)
			goto error;

		SendMessage(hDnsButton, BM_CLICK, 0, 0);
		Sleep(1000 * 1);

		HWND ip = NULL;
		if (NULL != dns1)
		{
			ip = FindWindowEx(hRoutineCard, NULL, "static", "首选 DNS 服务器(&P):");
			if (NULL == ip)
				goto error;
			ip = FindWindowEx(hRoutineCard, ip, "SysIPAddress32", NULL);
			if (NULL == ip)
				goto error;
			SendMessage(ip, IPM_SETADDRESS, 0, (LPARAM)IpStr2Hex(dns1));
			Sleep(1000);
		}

		if (NULL != dns2)
		{
			ip = FindWindowEx(hRoutineCard, ip, "static", "备用 DNS 服务器(&A):");
			if (NULL == ip)
				goto error;
			ip = FindWindowEx(hRoutineCard, ip, "SysIPAddress32", NULL);
			if (NULL == ip)
				goto error;
			SendMessage(ip, IPM_SETADDRESS, 0, (LPARAM)IpStr2Hex(dns2));
			Sleep(1000);
		}

		HWND hSureOnTcpIp = FindWindowEx(hTcpIp, NULL, "Button", "确定");
		if (NULL == hSureOnTcpIp)
			goto error;
		SendMessage(hSureOnTcpIp, BM_CLICK, 0, 0);
		while (NULL != hTcpIp)
		{
			Sleep(1000);
			hTcpIp = FindWindow(NULL, "Internet 协议 (TCP/IP) 属性");
		}

		HWND hSureOnAdslPro = FindWindowEx(hAdsl_pro, NULL, "Button", "确定");
		if (NULL == hSureOnAdslPro)
			goto error;
		SendMessage(hSureOnAdslPro, BM_CLICK, 0, 0);
		while (NULL != hAdsl_pro)
		{
			Sleep(1000);
			hAdsl_pro = FindWindow(NULL, "adsl 属性");
		}

		printf_s("修改dns成功\n");

		return TRUE;
	}

error:
	printf("修改dns失败\n");
	return FALSE;
}

#ifndef USE_MODIFY_DNS
int main()
{
	getchar();
	return 0;
}
#endif // !USE_MODIFY_DNS

