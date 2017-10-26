#pragma once

BOOL BindSObjWithCompPort(SOCKET_OBJ* obj);

void ParsingRequestHeader(SOCKET_OBJ* c_sobj, BUFFER_OBJ* c_bobj);

void AcceptCompFailed(void* _lobj, void* _c_bobj);
void AcceptCompSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);

void RecvRequestHeaderFailed(void* _c_sobj, void* _c_bobj);
void RecvRequestHeaderSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj);

void CONNECT_ConnectServerFailed(void* _s_sobj, void* _s_bobj);
void CONNECT_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj);

void SendReponseFailed(void* _c_sobj, void* _c_bobj);
void SendReponseSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj);

void RecvCompFailed(void* _sobj, void* _bobj);
void RecvCompSuccess(DWORD dwTransion, void* _sobj, void* _bobj);

void SendCompFailed(void* _sobj, void* _bobj);
void SendCompSuccess(DWORD dwTransion, void* _sobj, void* _bobj);

void GET_ConnectServerFailed(void* _s_sobj, void* _s_bobj);
void GET_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj);

void Accept6086CompFailed(void* _lobj, void* _c_bobj);
void Accept6086CompSuccess(DWORD dwTranstion, void* _lobj, void* _c_bobj);

void Recv6086RequestHeaderFailed(void* _c_sobj, void* _c_bobj);
void Recv6086RequestHeaderSuccess(DWORD dwTranstion, void* _c_sobj, void* _c_bobj);

void SERVER_ConnectServerFailed(void* _s_sobj, void* _s_bobj);
void SERVER_ConnectServerSuccess(DWORD dwTranstion, void* _s_sobj, void* _s_bobj);