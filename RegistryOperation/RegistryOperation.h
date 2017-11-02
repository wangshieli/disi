#pragma once

#define SProxy_Tool "Software\\SProxy_Tool"
#define SProxy (SProxy_Tool##"\\Proxy")
#define CommandExecute (SProxy_Tool##"\\PWorkUnit")
#define ManageModule (SProxy_Tool##"\\ManageModule")
#define Chromium (SProxy_Tool##"\\Chromium")
#define SMonitor	(SProxy_Tool##"\\Monitor")

BOOL PRegCreateKey(const char* lpSubKey, PHKEY phSubKey);

BOOL SetRegValue(HKEY hKey, const char* ItemName, char* pData);

BOOL GetRegValue(HKEY hKey, const char* ItemName, char* pData);

BOOL RegselfInfo(const char* pItem);

LRESULT ModifyRiskFileType();
