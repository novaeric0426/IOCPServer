#pragma once
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#define MAX_SOCKBUF 1024 //패킷 크기
#define MAX_WORKERTHREAD 4 //쓰레드 풀에 넣을 쓰레드 수
#define MAX_SOCK_SENDBUF 1024 //보내는 소켓 버퍼의 크기
const UINT64 RE_USE_SESSION_WAIT_TIMESEC = 3;

enum class IOOperation
{
	ACCEPT,
	RECV,
	SEND
};

//WSAOVERLAPPED  구조체를 확장 시켜서 필요한 정보를 더 넣음
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped; //Overlapped I/O 구조체
	SOCKET m_socketClient;					   //클라이언트 소켓
	WSABUF m_wsaBuf;						   //Overlapped I/O 작업 버퍼
	IOOperation m_eOperation;			   //작업 동작 종류
	UINT32 SessionIndex = 0;
};