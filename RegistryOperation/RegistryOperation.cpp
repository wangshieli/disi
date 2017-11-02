#define INITGUID
#include "../ShareApi/ShareApi.h"
#include "RegistryOperation.h"

DWORD dwType = REG_BINARY | REG_DWORD | REG_EXPAND_SZ | REG_MULTI_SZ | REG_NONE | REG_SZ;

// Software\\SProxy_Tool
// Software\\SProxy_Tool\\Proxy
// Software\\SProxy_Tool\\CommandExecute
// Software\\SProxy_Tool\\ManageModule
BOOL PRegCreateKey(const char* lpSubKey, PHKEY phSubKey)
{
	DWORD dw = 0;
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, REG_NONE, REG_OPTION_NON_VOLATILE,
		KEY_WRITE | KEY_READ, NULL, phSubKey, &dw) != ERROR_SUCCESS)
	{
		printf("创建 %s 键值失败\n", lpSubKey);
		return FALSE;
	}

	return TRUE;
}

BOOL InitRegKey(PHKEY phProxy_Tool, PHKEY phProxy, PHKEY phCommExe, PHKEY phManage, PHKEY phChromium)
{
	return PRegCreateKey(SProxy_Tool, phProxy_Tool) &&
		PRegCreateKey(SProxy, phProxy) &&
		PRegCreateKey(CommandExecute, phCommExe) &&
		PRegCreateKey(ManageModule, phManage) &&
		PRegCreateKey(Chromium, phChromium);
}

BOOL GetRegValue(HKEY hKey, const char* ItemName, char* pData)
{
	DWORD err = ERROR_SUCCESS;
	DWORD len = MAX_PATH;
	err = RegQueryValueEx(hKey, ItemName, NULL, NULL, (LPBYTE)pData, &len);
	if (err != ERROR_SUCCESS)
	{
		if (ERROR_MORE_DATA == err)
			printf("the lpData buffer is too small to receive the data\n");
		else if (ERROR_FILE_NOT_FOUND == err)
			printf("the lpValueName registry value does not exist\n");
		else
			printf("RegQueryValueEx fails\n");

		return FALSE;
	}
	return TRUE;
}

BOOL SetRegValue(HKEY hKey, const char* ItemName, char* pData)
{
	int len = strlen(pData) + 1;
	if (ERROR_SUCCESS != RegSetValueEx(hKey, ItemName, 0, REG_SZ, (const unsigned char*)pData, len))
	{
		printf("RegSetValueEx fails\n");
		return FALSE;
	}

	return TRUE;
}

BOOL RegselfInfo(const char* pItem)
{
	HKEY hKey = NULL;
	if (!PRegCreateKey(pItem, &hKey))
		return FALSE;

	char filepath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, filepath, MAX_PATH);
	if (!SetRegValue(hKey, "path", filepath))
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	if (!SetRegValue(hKey, "version", VERSION))
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);
	return TRUE;
}

LRESULT ModifyRiskFileType()
{
	HKEY hCheckKey;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER,
		"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Associations",
		0,
		KEY_READ,
		&hCheckKey))
	{
		char Data[MAX_PATH] = { 0 };
		if (GetRegValue(hCheckKey, "ModRiskFileTypes", Data))
		{
			if (strstr(Data, ".exe") != NULL)
			{
				RegCloseKey(hCheckKey);
				return S_OK;
			}
		}

		RegCloseKey(hCheckKey);
	}

	::CoInitialize(NULL);
	LRESULT hr = S_OK;
	HKEY hGPOKey = NULL;
	HKEY hKey = NULL;
	GUID RegisterGuid = REGISTRY_EXTENSION_GUID;
	IGroupPolicyObject* pGPO = NULL;
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER, IID_IGroupPolicyObject, (LPVOID*)&pGPO);
	if (FAILED(hr))
		goto error;

	DWORD dwSection = GPO_SECTION_USER;
	hr = pGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);
	if (FAILED(hr))
		goto error;

	hr = pGPO->GetRegistryKey(dwSection, &hGPOKey);
	if (FAILED(hr))
		goto error;

	hr = RegCreateKeyEx(hGPOKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Associations", 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
	if (ERROR_SUCCESS != hr)
		goto error;

	const char* pHeadInfo = ".exe";
	hr = RegSetValueEx(hKey, "ModRiskFileTypes", NULL, REG_SZ, (const unsigned char*)pHeadInfo, strlen(pHeadInfo) + 1);
	if (ERROR_SUCCESS != hr)
		goto error;

	GUID guid;
	CoCreateGuid(&guid);

	hr = pGPO->Save(FALSE, TRUE, &RegisterGuid, &guid);

error:
	printf("error = %d\n", GetLastError());
	if (NULL != pGPO)
		pGPO->Release();
	if (NULL != hGPOKey)
		RegCloseKey(hGPOKey);
	if (NULL != hKey)
		RegCloseKey(hKey);
	::CoUninitialize();
	return hr;
}

BOOL PCheckKey(const char* KeyName)
{
	HKEY hSubKey = NULL;
	if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, KeyName, 0, KEY_WRITE | KEY_READ, &hSubKey) != ERROR_SUCCESS)
	{
		printf("创建 %s 键值失败\n", KeyName);
		return FALSE;
	}
	RegCloseKey(hSubKey);
	return TRUE;
}


// 便利注册表
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724256(v=vs.85).aspx

BOOL QueryKey(HKEY hKey)
{
	char achClass[MAX_PATH] = "";
	DWORD cchClassName = MAX_PATH;
	DWORD    cSubKeys = 0;
	DWORD    cbMaxSubKey;
	DWORD    cchMaxClass;
	DWORD    cValues;
	DWORD    cchMaxValue;
	DWORD    cbMaxValueData;
	DWORD    cbSecurityDescriptor;
	FILETIME ftLastWriteTime;

	char achValue[MAX_PATH];
	DWORD cchValue = MAX_PATH;

	if (ERROR_SUCCESS != RegQueryInfoKey(
		hKey,
		achClass,
		&cchClassName,
		NULL,
		&cSubKeys,
		&cbMaxSubKey,
		&cchMaxClass,
		&cValues,
		&cchMaxValue,
		&cbMaxValueData,
		&cbSecurityDescriptor,
		&ftLastWriteTime))
		return FALSE;

	if (cValues)
	{
		DWORD err = 0;
		for (DWORD i = 0; i < cValues; i++)
		{
			cchValue = MAX_PATH;
			achValue[0] = '\0';
			err = RegEnumValue(hKey, i, achValue, &cchValue, NULL, NULL, NULL, NULL);
			if (ERROR_SUCCESS == ERROR_SUCCESS)
			{
				char szData[MAX_PATH] = { 0 };
				DWORD dwDataLen = MAX_PATH;
				err = RegQueryValueEx(hKey, achValue, 0, &dwType, (LPBYTE)szData, &dwDataLen);
			}
		}
	}

	return TRUE;
}

LRESULT AddChromeWhiteList()
{
	::CoInitialize(NULL);
	LRESULT hr = S_OK;
	HKEY hGPOKey = NULL;
	HKEY hKey = NULL;
	GUID RegisterGuid = REGISTRY_EXTENSION_GUID;
	IGroupPolicyObject* pGPO = NULL;
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER, IID_IGroupPolicyObject, (LPVOID*)&pGPO);
	if (FAILED(hr))
		goto error;

	DWORD dwSection = GPO_SECTION_MACHINE;
	hr = pGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);
	if (FAILED(hr))
		goto error;

	hr = pGPO->GetRegistryKey(dwSection, &hGPOKey);
	if (FAILED(hr))
		goto error;

	//status = RegOpenKeyEx(hGPOKey, "Software\\Policies\\Google\\Chrome\\ExtensionInstallWhitelist", 0, KEY_WRITE, &hKey);
	//if (ERROR_SUCCESS != status)
	//{

	//}

	hr = RegCreateKeyEx(hGPOKey, "Software\\Policies\\Google\\Chrome\\ExtensionInstallWhitelist", 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
	if (ERROR_SUCCESS != hr)
		goto error;

	const char* pHeadInfo = " ";
	hr = RegSetValueEx(hKey, "**delvals.", NULL, REG_SZ, (const unsigned char*)pHeadInfo, strlen(pHeadInfo) + 1);
	if (ERROR_SUCCESS != hr)
		goto error;

	GUID guid;
	CoCreateGuid(&guid);

	hr = pGPO->Save(TRUE, TRUE, &RegisterGuid, &guid);
	
error:
	if (NULL != pGPO)
		pGPO->Release();
	if (NULL != hGPOKey)
		RegCloseKey(hGPOKey);
	if (NULL != hKey)
		RegCloseKey(hKey);
	::CoUninitialize();
	return hr;
}

#ifndef USE_REG_API
int main()
{
	HKEY hProxy_Tool = NULL;
	HKEY hProxy = NULL;
	HKEY hCommExe = NULL;
	HKEY hManage = NULL;
	HKEY hChromium = NULL;
	if (!InitRegKey(&hProxy_Tool, &hProxy, &hCommExe, &hManage, &hChromium))
	{
		printf("初始化注册表失败\n");
	}
	if (NULL != hProxy_Tool)
		RegCloseKey(hProxy_Tool);
	if (NULL != hProxy)
		RegCloseKey(hProxy);
	if (NULL != hCommExe)
		RegCloseKey(hCommExe);
	if (NULL != hManage)
		RegCloseKey(hManage);
	if (NULL != hChromium)
		RegCloseKey(hChromium);
	getchar();

	HKEY hTestKey;

	if (RegOpenKeyEx(HKEY_CURRENT_USER,
		"SOFTWARE\\Microsoft",
		0,
		KEY_READ,
		&hTestKey) == ERROR_SUCCESS
		)
	{
		QueryKey(hTestKey);
	}

	RegCloseKey(hTestKey);
	return 0;
}
#endif