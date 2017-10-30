#pragma once

#define SProxy_Tool "Software\\SProxy_Tool"
#define SProxy (SProxy_Tool##"\\Proxy")
#define CommandExecute (SProxy_Tool##"\\CommandExecute")
#define ManageModule (SProxy_Tool##"\\ManageModule")
#define Chromium (SProxy_Tool##"\\Chromium")

BOOL PRegCreateKey(const char* lpSubKey, PHKEY phSubKey);

BOOL SetRegValue(HKEY hKey, const char* ItemName, char* pData);
