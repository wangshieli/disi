// test_unzip.cpp : 定义控制台应用程序的入口点。
//

#include "zip_util.h"
#include <string>
#include <shlwapi.h>
#include <tchar.h>
#include "..\dounzip.h"

static std::wstring GetExeDir()  
{  
    static wchar_t szbuf[MAX_PATH];  
    ::GetModuleFileNameW(NULL,szbuf,MAX_PATH);  
    ::PathRemoveFileSpecW(szbuf);  
      
    std::wstring path;  
    path.append(szbuf);
	if(path.at(path.size()-1)!= L'\\')
	{
		path.append(L"\\");
	}
    return path;  
}  

// 提取代码: zlib-1.2.5\src\contrib\minizip\miniunz.c
//           zlib-1.2.5\src\contrib\minizip\unzip.h;unzip.c;iowin32.h;iowin32.c;ioapi.h;ioapi.c
bool dounzip(std::wstring _dir, std::wstring _output)
{
//	std::wstring dir = GetExeDir();
//	std::wstring output = dir;
//	output.append(L"unzip");
	CreateDirectoryW(_output.c_str(),NULL);
//	dir.append(L"..\\lib\\zlib125.zip");
	
//	ZipUtil::UnzipFile(dir,output);
	//std::wstring str1(L"C:\\chrome.zip");
	
	return ZipUtil::UnzipFile(_dir, _output);
}

