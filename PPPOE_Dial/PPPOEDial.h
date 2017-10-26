#pragma once

BOOL FindRasphone();

BOOL DoAdsl();

extern HANDLE hRedialingEvent;
extern HANDLE hRedialStartEvent;
extern HANDLE hRedialCompEvent;
extern HANDLE hRedialThreadStart;

unsigned int _stdcall redial_thread(LPVOID pVoid);