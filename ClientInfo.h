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
	SOCKET mSock;			//Client와 연결되는 소켓
	stOverlappedEx mRecvOverlappedEx;	//RECV Overlapped I/O 작업을 위한 변수
	std::queue<stOverlappedEx*> mSendDataqueue;

	INT64 mIsConnect = 0;
	UINT64 mLatestClosedTimeSec = 0;

	stOverlappedEx mAcceptContext;
	char mAcceptBuf[64];

	char mRecvBuf[MAX_SOCKBUF]; //데이터 버퍼

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

		//I/O Completion Port 객체와 소켓을 연결 시킨다.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}
		return BindRecv(); //연결시키자마자 Recv Overlapped IO 걸어서 수신 대기
	}

	void Close(bool bIsForce = false)
	{
		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER로 설정

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
		printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)mSock);

		return true;
	}

	//CompletionPort 객체와 소켓과 CompletionKey를 연결시키는 역할을 한다.
	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket과 pClientInfo를 CompletionPort 객체와 연결시킨다.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(),
			iocpHandle_,(ULONG_PTR)(this),0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return false;
		}
		return true;
	}

	//WSARecv Overlapped I/O 작업을 시킨다.
	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O를 위해 각 정보를 셋팅해 준다.
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

		//socket_error 이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[Error] WSARecv() function failed : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	//1개의 스레드에서만 호출해야 한다!
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

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	void SendCompleted(const UINT32 dataSize_)
	{
		printf("[송신 완료] bytes : %d\n", dataSize_);

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