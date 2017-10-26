#pragma once

extern LPFN_ACCEPTEX lpfnAccpetEx;
extern LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs;
extern LPFN_CONNECTEX lpfnConnectEx;

extern CRITICAL_SECTION g_csSObj;
extern CRITICAL_SECTION g_csBObj;

BOOL Init();

void PCloseSocket(SOCKET_OBJ* obj);

BOOL InitListenSock(LISTEN_OBJ* lobj, u_short port);


// n = 0 5005     n = 1 6086
BOOL PostAcceptEx(LISTEN_OBJ* lobj, int n);

BOOL PostRecv(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj);

BOOL PostSend(SOCKET_OBJ* _sobj, BUFFER_OBJ* _bobj);

BOOL ConnectToDisiServer(SOCKET& sock5001, const char* ServerIP, unsigned short ServerPort);

void Server_CONNECT(u_short nPort);