#pragma once

SOCKET_OBJ* allocSObj();

void freeSObj(SOCKET_OBJ* obj);

BUFFER_OBJ* allocBObj(DWORD nSize);

void freeBObj(BUFFER_OBJ* obj);