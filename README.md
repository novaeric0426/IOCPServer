# IOCPServer
C++을 사용한 IOCP 기반 TCP 소켓 서버 개발

## 프로그램 개요
멀티스레드 기반의 IOCP 서버입니다. 스레드의 종류는 다음과 같습니다.

1. WorkerThread(4개) : Overlapped I/O 작업을 대기하는 스레드, IOOperation(Recv,Send,ACCEPT)의 완료 이벤트를 받아서 작업 결과 처리
2. AccepterThread(1개) : 비동기로 클라이언트의 연결 호출을 대기하는 스레드
3. ProcessThread(1개) : 패킷 수신 시 바로 처리 하는게 아니라 PacketQueue에 저장, 해당 Queue에 비동기로 접근해서 Queue에 패킷 1개이상 존재 할 시 처리

 
주요 함수 기능
=======

main() 함수 내부
-------
  
    BlackJackServer server;
    //소켓을 초기화
    server.InitSocket();

    //소켓과 서버 주소를 연결하고 등록 시킨다.
    server.BindandListen(SERVER_PORT);

    server.Run(MAX_CLIENT);
    printf("아무 키나 누를 때까지 대기합니다\n");

    while (true)
    {
      std::string inputCmd;
      std::getline(std::cin, inputCmd);

      if (inputCmd == "quit")
      {
        break;
      }
      if (inputCmd == "shownickname")
      {
        server.ShowClientNickname();
      }
    }

    server.End();
    return 0;
    
InitSocket() : 클라이언트의 연결을 받아들일 Listening Socket 초기화

BindandListen() : 해당 포트에 Listening Socket 연결

Run() : IOCPServer.h 의 StartServer()를 실행해서 서버 가동. ProcessThread를 시작하여 들어오는 패킷 처리 시작


ProcessPacket()
------

     while (mIsRunProcessThread)
     {
       auto packetData = DequePacketData();
       if (packetData.DataSize != 0)
       {
         DoGameLogic(packetData);
       }
       else
       {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
       }
     }

받은 패킷을 처리하는 메서드, while문 내부의 DoGameLogic에서 패킷을 Deserialize 한 뒤, 각 게임의 로직에 맞는 메서드가 실행된다.

WorkerThread의 내부 주요 로직
----

```
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
```

완료된 Overlapped I/O 작업의 종류에 따라 다른 로직을 실행. BindRecv()는 바로 WSARecv를 실행해 다음 작업을 대기. 
하지만 Send 작업의 결과 처리는 바로 WSASend를 호출하는게 아니라 mSendDataqueue에서 보낼 패킷을 한개씩 꺼내서 관리.
(ClientInfo.h 의 SendMsg,SendIO,SendCompleted 메서드 참고)

Why? -> 한 번의 전송에 한 개의 패킷 데이터가 담기는 것을 보장하기 위해 위와 같이 설계  N-send (x) , 1-send(o)

