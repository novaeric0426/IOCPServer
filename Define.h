#pragma once
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#define MAX_SOCKBUF 1024 //��Ŷ ũ��
#define MAX_WORKERTHREAD 4 //������ Ǯ�� ���� ������ ��
#define MAX_SOCK_SENDBUF 1024 //������ ���� ������ ũ��
const UINT64 RE_USE_SESSION_WAIT_TIMESEC = 3;

enum class IOOperation
{
	ACCEPT,
	RECV,
	SEND
};

//WSAOVERLAPPED  ����ü�� Ȯ�� ���Ѽ� �ʿ��� ������ �� ����
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped; //Overlapped I/O ����ü
	SOCKET m_socketClient;					   //Ŭ���̾�Ʈ ����
	WSABUF m_wsaBuf;						   //Overlapped I/O �۾� ����
	IOOperation m_eOperation;			   //�۾� ���� ����
	UINT32 SessionIndex = 0;
};