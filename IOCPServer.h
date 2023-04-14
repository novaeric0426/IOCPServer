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
	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<ClientInfo*> mClientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ� ���� ���� ����
	SOCKET mListenSocket = INVALID_SOCKET;

	//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	int mClientCnt = 0;

	//IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	//Accept ������
	std::thread mAccepterThread;

	//CompletionPort ��ü �ڵ�
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	//�۾� ������ ���� �÷���
	bool mIsWorkerRun = true;

	//���� ������ ���� �÷���
	bool mIsAccepterRun = true;

	//���� ����
	char mSocketBuf[1024] = { 0, };

public:
	IOCPServer(void) {}

	~IOCPServer(void)
	{
		//������ ����� ������.
		WSACleanup();
	}

	//���� �ʱ�ȭ �Լ�
	bool InitSocket()
	{
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[Error] WSAStartup() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//���������� TCP, Overlapped I/O ������ ����
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			printf("[Error] Socket() function failed : %d\n", WSAGetLastError());
			return false;
		}

		printf("Socket Initialize Success!\n");
		return true;
	}

	//-----������ �Լ�-----//
	//������ �ּ������� ���ϰ� �����Ű�� ���� ��û�� �ޱ� ����
	//������ ����ϴ� �Լ�
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort); // ���� ��Ʈ ����
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		//������ ������ ���� �ּ� ������ cIOCompletionPort ������ �����Ѵ�.
		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet)
		{
			printf("[Error] bind() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//���� ��û�� �޾Ƶ��̱� ���� cIOCompletionPort������ ����ϰ�
		//���Ӵ��ť�� 5���� �����Ѵ�.
		nRet = listen(mListenSocket, 5);
		if (0 != nRet)
		{
			printf("[Error] listen() function failed : %d\n", WSAGetLastError());
			return false;
		}

		//CompletionPort��ü ���� ��û�� �Ѵ�.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}

		auto hIOCPHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
		if (nullptr == hIOCPHandle)
		{
			printf("[����] listen socket IOCP bind ���� : %d\n", WSAGetLastError());
			return false;
		}

		printf("Server Registered Complete...\n");
		return true;
	}

	// ���� ��û�� �����ϰ� �޽����� �޾Ƽ� ó���ϴ� �Լ�
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

	// ������ ��������� �ı�
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

		//Accepter �����带 �����Ѵ�.
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



	// ��Ʈ��ũ �̺�Ʈ�� ó���Ͽ� ���ø����̼ǿ� ��ȣ�� ������ �Լ�
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

	//WaitingThread Queue���� ����� ��������� ����
	bool CreateWorkerThread()
	{
		unsigned int uiThreadId = 0;
		//WaitingThread Queue�� ��� ���·� ���� ������� ���� ���� ���� : (cpu���� *2) +1
		for (int i = 0; i < MAX_WORKERTHREAD; i++)
		{
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}

		printf("WorkerThead Start!\n");
		return true;
	}

	//accept ��û�� ó���ϴ� ������ ����
	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() {AccepterThread(); });

		printf("AccepterThread Start!\n");
		return true;
	}


	//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü�� ��ȯ�Ѵ�.
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
	

	//Overlapped I/O�۾��� ���� �Ϸ� �뺸�� �޾�
	//�׿� �ش��ϴ� ó���� �ϴ� �Լ�
	void WorkerThread()
	{
		//CompletionKey�� ���� ������ ���� -> �� ���α׷������� �� client���� ����
		ClientInfo* pClientInfo = NULL;
		//�Լ� ȣ�� ���� ����
		BOOL bSuccess = TRUE;
		//Overlapped I/O �۾����� ���۵� ������ ũ��
		DWORD dwIoSize = 0;
		//I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			//////////////////////////////////////////////////////
			//�� �Լ��� ���� ��������� WaitingThread Queue��
			//��� ���·� ���� �ȴ�.
			//�Ϸ�� Overlapped I/O �۾��� �߻��ϸ� IOCP Queue����
			//�Ϸ�� �۾��� ������ �� ó���� �Ѵ�.
			//�׸��� PostQueuedCompletionStatus()�Լ������� �����
			//�޽����� �����Ǹ� �����带 �����Ѵ�.
			//////////////////////////////////////////////////////
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,									//������ ���۵� ����Ʈ
				(PULONG_PTR)&pClientInfo,			//CompletionKey
				&lpOverlapped,								// Overlapped IO ��ü
				INFINITE);										//����� �ð�

			//����� ������ ���� �޽��� ó��..
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

			//client�� ������ ��������...
			if (FALSE == bSuccess || (0 == dwIoSize && IOOperation::ACCEPT != pOverlappedEx->m_eOperation))
			{
				//printf("socket(%d) ���� ����\n", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			if (IOOperation::ACCEPT == pOverlappedEx->m_eOperation)
			{
				pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex);
				if (pClientInfo->AcceptCompletion())
				{
					//Ŭ���̾�Ʈ ���� ����
					++mClientCnt;

					OnConnect(pClientInfo->GetIndex());
				}
				else
				{
					CloseSocket(pClientInfo, true);
				}
			}

			//Overlapped I/O Recv�۾� ��� �� ó��
			else if (IOOperation::RECV == pOverlappedEx->m_eOperation)
			{
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			//Overlapped I/O Send�۾� ��� �� ó��
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
			{
				pClientInfo->SendCompleted(dwIoSize);
			}
			//���� ��Ȳ
			else
			{
				printf("Exception happened in socket(%d)\n", pClientInfo->GetIndex());
			}
		}
	}

	//������� ������ �޴� ������
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


	//������ ������ ���� ��Ų��.
	void CloseSocket(ClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(clientIndex);
	}
};