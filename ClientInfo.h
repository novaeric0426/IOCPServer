#pragma once

#include "Define.h"
#include<stdio.h>
#include<iostream>
#include<string>
#include<mutex>
#include<queue>

class ClientInfo
{
private:
	INT32 mIndex = 0;
	char nickname[20] = { 0, };
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;
	SOCKET mSock;			//Client�� ����Ǵ� ����
	stOverlappedEx mRecvOverlappedEx;	//RECV Overlapped I/O �۾��� ���� ����
	std::queue<stOverlappedEx*> mSendDataqueue;

	INT64 mIsConnect = 0;
	UINT64 mLatestClosedTimeSec = 0;

	stOverlappedEx mAcceptContext;
	char mAcceptBuf[64];

	char mRecvBuf[MAX_SOCKBUF]; //������ ����

	std::mutex mSendLock;
	UINT64 mSendPos = 0;
	bool mIsSending = false;
	char mSendBuf[MAX_SOCK_SENDBUF];
	char mSendingBuf[MAX_SOCK_SENDBUF];

public:
	ClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}
	~ClientInfo() = default;
	void Init(const UINT32 index, HANDLE iocpHandle_) 
	{
		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return mIsConnect ==1; }

	SOCKET GetSock() { return mSock; }

	UINT64 GetLatestClosedTimeSec() { return mLatestClosedTimeSec; }

	char* RecvBuffer() { return mRecvBuf; }
	char* GetNickName() { return nickname; }

	void SetNickname(const char* nickname_) { memcpy(nickname, nickname_, strlen(nickname_)); }

	void ShowNickname() { printf("%s\n", nickname); }

	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_)
	{
		mSock = socket_;
		mIsConnect = 1;

		Clear();

		//I/O Completion Port ��ü�� ������ ���� ��Ų��.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}
		return BindRecv(); //�����Ű�ڸ��� Recv Overlapped IO �ɾ ���� ���
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		shutdown(mSock, SD_BOTH);

		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		mIsConnect = 0;
		mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear()
	{
		mSendPos = 0;
		mIsSending = false;
	}

	bool PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_)
	{
		printf_s("PostAccept. client Index: %d\n", GetIndex());

		mLatestClosedTimeSec = UINT32_MAX;

		mSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
			NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == mSock)
		{
			printf_s("client Socket WSASocket Error : %d\n", GetLastError());
			return false;
		}

		ZeroMemory(&mAcceptContext, sizeof(stOverlappedEx));

		DWORD bytes = 0;
		DWORD flags = 0;
		mAcceptContext.m_wsaBuf.len = 0;
		mAcceptContext.m_wsaBuf.buf = nullptr;
		mAcceptContext.m_eOperation = IOOperation::ACCEPT;
		mAcceptContext.SessionIndex = mIndex;

		if (FALSE == AcceptEx(listenSock_, mSock, mAcceptBuf, 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf_s("AcceptEx Error : %d\n", GetLastError());
				return false;
			}
		}

		return true;
	}

	bool AcceptCompletion()
	{
		printf_s("AcceptCompletion : SessionIndex(%d)\n", mIndex);

		if (OnConnect(mIOCPHandle, mSock) == false)
		{
			return false;
		}

		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)mSock);

		return true;
	}

	//CompletionPort ��ü�� ���ϰ� CompletionKey�� �����Ű�� ������ �Ѵ�.
	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket�� pClientInfo�� CompletionPort ��ü�� �����Ų��.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(),
			iocpHandle_,(ULONG_PTR)(this),0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}
		return true;
	}

	//WSARecv Overlapped I/O �۾��� ��Ų��.
	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock,
			&(mRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		//socket_error �̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[Error] WSARecv() function failed : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	//1���� �����忡���� ȣ���ؾ� �Ѵ�!
	bool SendMsg(char* pMsg_, const UINT32 dataSize_)
	{
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));
		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataqueue.push(sendOverlappedEx);

		if (mSendDataqueue.size() == 1)
		{
			SendIO();
		}

		return true;
	}

	bool SendIO()
	{
		auto sendOverlappedEx = mSendDataqueue.front();

		DWORD dwRecvNumBytes = 0;
		int nRet = WSASend(mSock,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		printf("[�۽� �Ϸ�] bytes : %d\n", dataSize_);

		std::lock_guard<std::mutex> guard(mSendLock);

		delete[] mSendDataqueue.front()->m_wsaBuf.buf;
		delete mSendDataqueue.front();

		mSendDataqueue.pop();

		if (mSendDataqueue.empty() == false)
		{
			SendIO();
		}
	}

};