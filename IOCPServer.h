#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include<winsock2.h>
#include<WS2tcpip.h>
#include "Define.h"
#include "ClientInfo.h"

#include<thread>
#include<vector>




class IOCPServer
{
private:
	//클라이언트 정보 저장 구조체
	std::vector<ClientInfo*> mClientInfos;

	//클라이언트의 접속을 받기 위한 리슨 소켓
	SOCKET mListenSocket = INVALID_SOCKET;

	//접속 되어있는 클라이언트 수
	int mClientCnt = 0;

	//IO Worker 스레드
	std::vector<std::thread> mIOWorkerThreads;

	//Accept 스레드
	std::thread mAccepterThread;

	//CompletionPort 객체 핸들
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	//작업 스레드 동작 플래그
	bool mIsWorkerRun = true;

	//접속 스레드 동작 플래그
	bool mIsAccepterRun = true;

	//소켓 버퍼
	char mSocketBuf[1024] = { 0, };

public:
	IOCPServer(void) {}

	~IOCPServer(void)
	{
		//윈속의 사용을 끝낸다.
		WSACleanup();
	}

	//소켓 초기화 함수
	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[Error] WSAStartup() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//연결지향형 TCP, Overlapped I/O 소켓을 생성
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			printf("[Error] Socket() function failed : %d\n", WSAGetLastError());
			return false;
		}

		printf("Socket Initialize Success!\n");
		return true;
	}

	//-----서버용 함수-----//
	//서버의 주소정보를 소켓과 연결시키고 접속 요청을 받기 위해
	//소켓을 등록하는 함수
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort); // 서버 포트 설정
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		//위에서 지정한 서버 주소 정보와 cIOCompletionPort 소켓을 연결한다.
		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet)
		{
			printf("[Error] bind() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//접속 요청을 받아들이기 위해 cIOCompletionPort소켓을 등록하고
		//접속대기큐를 5개로 설정한다.
		nRet = listen(mListenSocket, 5);
		if (0 != nRet)
		{
			printf("[Error] listen() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//CompletionPort객체 생성 요청을 한다.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return false;
		}

		auto hIOCPHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
		if (nullptr == hIOCPHandle)
		{
			printf("[에러] listen socket IOCP bind 실패 : %d\n", WSAGetLastError());
			return false;
		}

		printf("Server Registered Complete...\n");
		return true;
	}

	// 접속 요청을 수락하고 메시지를 받아서 처리하는 함수
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);

		bool bRet = CreateWorkerThread();
		if (false == bRet)
		{
			return false;
		}

		bRet = CreateAccepterThread();
		if (false == bRet)
		{
			return false;
		}

		printf("Server Start!\n");
		return true;
	}

	// 생성된 스레드들을 파괴
	void DestroyThread()
	{
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads)
		{
			if (th.joinable())
			{
				th.join();
			}
		}

		//Accepter 스레드를 종료한다.
		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}
	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(pData, dataSize_);
	}

	void SetClientNickname(const UINT32 sessionIndex_, const char* nickname)
	{
		mClientInfos[sessionIndex_]->SetNickname(nickname);
	}

	void ShowClientNickname()
	{
		for (auto client : mClientInfos)
		{
			if (client->IsConnected() == true)
			{
				client->ShowNickname();
			}
		}
	}

	std::string GetJoinedClientNickname()
	{
		std::string temp = "";
		for (auto client : mClientInfos)
		{
			if (client->IsConnected() == true)
			{
				temp += client->GetNickName(); temp += ",";
			}
		}
		return temp;
	}



	// 네트워크 이벤트를 처리하여 애플리케이션에 신호를 보내는 함수
	virtual void OnConnect(const UINT32 clientIndex_){}
	virtual void OnClose(const UINT32 clientIndex_){}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}


private:
	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; i++)
		{
			auto client = new ClientInfo();
			client->Init(i, mIOCPHandle);

			mClientInfos.push_back(client);
		}
	}

	//WaitingThread Queue에서 대기할 스레드들을 생성
	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		//WaitingThread Queue에 대기 상태로 넣을 스레드들 생성 권장 개수 : (cpu개수 *2) +1
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		printf("WorkerThead Start!\n");
		return true;
	}

	//accept 요청을 처리하는 스레드 생성
	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("AccepterThread Start!\n");
		return true;
	}


	//사용하지 않는 클라이언트 정보 구조체를 반환한다.
	ClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client->IsConnected()==false)
			{
				return client;
			}
		}

		return nullptr;
	}

	ClientInfo* GetClientInfo(const UINT32 sessionIndex)
	{
		return mClientInfos[sessionIndex];
	}
	

	//Overlapped I/O작업에 대한 완료 통보를 받아
	//그에 해당하는 처리를 하는 함수
	void WorkerThread()
	{
		//CompletionKey를 받을 포인터 변수 -> 이 프로그램에서는 각 client별로 구분
		ClientInfo* pClientInfo = NULL;
		//함수 호출 성공 여부
		BOOL bSuccess = TRUE;
		//Overlapped I/O 작업에서 전송된 데이터 크기
		DWORD dwIoSize = 0;
		//I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			//////////////////////////////////////////////////////
			//이 함수로 인해 스레드들은 WaitingThread Queue에
			//대기 상태로 들어가게 된다.
			//완료된 Overlapped I/O 작업이 발생하면 IOCP Queue에서
			//완료된 작업을 가져와 뒤 처리를 한다.
			//그리고 PostQueuedCompletionStatus()함수에의해 사용자
			//메시지가 도착되면 스레드를 종료한다.
			//////////////////////////////////////////////////////
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,									//실제로 전송된 바이트
				(PULONG_PTR)&pClientInfo,			//CompletionKey
				&lpOverlapped,								// Overlapped IO 객체
				INFINITE);										//대기할 시간

			//사용자 스레드 종료 메시지 처리..
			if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped)
			{
				mIsWorkerRun = false;
				continue;
			}

			if (NULL == lpOverlapped)
			{
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			//client가 접속을 끊었을때...
			if (FALSE == bSuccess || (0 == dwIoSize && IOOperation::ACCEPT != pOverlappedEx->m_eOperation))
			{
				//printf("socket(%d) 접속 끊김\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			if (IOOperation::ACCEPT == pOverlappedEx->m_eOperation)
			{
				pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex);
				if (pClientInfo->AcceptCompletion())
				{
					//클라이언트 갯수 증가
					++mClientCnt;

					OnConnect(pClientInfo->GetIndex());
				}
				else
				{
					CloseSocket(pClientInfo, true);
				}
			}

			//Overlapped I/O Recv작업 결과 뒤 처리
			else if (IOOperation::RECV == pOverlappedEx->m_eOperation)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			//Overlapped I/O Send작업 결과 뒤 처리
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
			{
				pClientInfo->SendCompleted(dwIoSize);
			}
			//예외 상황
			else
			{
				printf("Exception happened in socket(%d)\n", pClientInfo->GetIndex());
			}
		}
	}

	//사용자의 접속을 받는 스레드
	void AccepterThread()
	{
		while (mIsAccepterRun)
		{
			auto curTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			for (auto client : mClientInfos)
			{
				if (client->IsConnected())
				{
					continue;
				}

				if ((UINT64)curTimeSec < client->GetLatestClosedTimeSec())
				{
					continue;
				}

				auto diff = curTimeSec - client->GetLatestClosedTimeSec();
				if (diff <= RE_USE_SESSION_WAIT_TIMESEC)
				{
					continue;
				}

				client->PostAccept(mListenSocket, curTimeSec);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(32));
		}
	}


	//소켓의 연결을 종료 시킨다.
	void CloseSocket(ClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(clientIndex);
	}
};