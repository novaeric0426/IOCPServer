#pragma once
#include "IOCPServer.h"
#include "Packet.h"
#include "flatbuffers/flatbuffers.h"
#include "GP_generated.h"
#include"Deck.h"

#include<iostream>
#include<string>
#include <vector>
#include<deque>
#include<thread>
#include<mutex>


using namespace GamePacket;
using namespace std;

class BlackJackServer : public IOCPServer
{
private:
	bool mIsRunProcessThread = false;

	std::thread mProcessThread;

	std::mutex mLock;
	std::deque<PacketData> mPacketDataQueue;

	//게임 관련 데이터
	int currentJoinPlayerNum = 0;
	queue<int> deck; //덱 정보
	int cardValues[53]; //카드 숫자 관련 정보
	int roundOver = 0;

public:
	BlackJackServer() = default;

	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		printf("[OnConnect] 클라이언트: Index(%d)\n", clientIndex_);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		printf("[OnClose] 클라이언트: Index(%d)\n", clientIndex_);
		currentJoinPlayerNum--;
	}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		//pData_[size_] = '\0';
		//printf("[OnReceive] 클라이언트: Index(%d), dataSize(%d), ReceivedMessage(%s)\n", clientIndex_, size_, pData_);

		PacketData packet;
		packet.Set(clientIndex_, size_, pData_);

		std::lock_guard<std::mutex> guard(mLock);
		mPacketDataQueue.push_back(packet);
	}

	void Run(const UINT32 maxClient)
	{
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() {ProcessPacket(); });

		StartServer(maxClient);
	}

	void End()
	{
		mIsRunProcessThread = false;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		DestroyThread();
	}

private:

	void ProcessPacket()
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0)
			{
				DoGameLogic(packetData);
				//에코 기능
				//SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	PacketData DequePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty())
		{
			return PacketData();
		}
		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();

		return packetData;
	}

	void SendFbPacket(UINT32 clientIndex, string data_, short rank_, short suit_, string func_)
	{
		flatbuffers::FlatBufferBuilder builder(256);
		auto data = builder.CreateString(data_);
		auto card = CardData(rank_, suit_);
		auto func = builder.CreateString(func_);
		MessageBuilder message_builder(builder);
		message_builder.add_data(data);
		message_builder.add_card(&card);
		message_builder.add_func(func);
		auto msg = message_builder.Finish();
		builder.Finish(msg);
		auto buff = builder.GetBufferPointer();
		auto msgSize = builder.GetSize();

		SendMsg(clientIndex, msgSize, (char*)buff);
	}

	void DoGameLogic(PacketData pkt)
	{
		uint8_t* buffer_pointer = (uint8_t*)pkt.pPacketData;
		auto message = GetMessage(buffer_pointer);
		const char* func = message->func()->c_str();
		std::string str = func;
		if (str.compare("SetName")==0)
		{
			SetClientNickname(pkt.SessionIndex, message->data()->c_str());
			currentJoinPlayerNum++;
			SendFbPacket(pkt.SessionIndex, "Hi!", 0, 0, "SetName");
			Sleep(10);
			SendFbPacket(pkt.SessionIndex, "", (short)pkt.SessionIndex, 0, "GetClientIndex");
		}
		else if (str.compare("GetJoinPlayerNum") == 0)
		{
			SendFbPacket(pkt.SessionIndex, "", currentJoinPlayerNum, 0, "GetJoinPlayerNum");
		}
		else if (str.compare("GetClientIndex") == 0)
		{
			SendFbPacket(pkt.SessionIndex, "", (short)pkt.SessionIndex, 0, "GetClientIndex");
		}
		else if (str.compare("BetDone") == 0)
		{
			currentJoinPlayerNum--;
			if (currentJoinPlayerNum == 0) {
				StartGame();
				currentJoinPlayerNum = 3;
			}
		}
		else if (str.compare("HitCard") == 0)
		{
			for (int i = 0; i < 3; i++) {
				Sleep(15);
				if (i != pkt.SessionIndex) {
					SendFbPacket(i, "", deck.front(), (short)pkt.SessionIndex, "ShowCard");
				}
				else
					SendFbPacket(i, "", deck.front(), (short)pkt.SessionIndex, "HitCard");
				Sleep(15);
			}	
			deck.pop();
		}
		else if (str.compare("CurrentHandDone")==0) {
			roundOver++;
			printf("current roundOver :%d\n", roundOver);
			if (roundOver == 3) {
				for (int i = 0; i < 3; i++) {
					Sleep(10);
					SendFbPacket(i, "", 0, 0, "RoundDone");
					Sleep(10);
				}
				roundOver = 0;
			}
		}
		else if (str.compare("ChoiceDone") == 0) {
			for (int i = 0; i < 3; i++) {
				SendFbPacket(i, "", (short)pkt.SessionIndex, 0, "ChoiceDone");
			}
		}
		else if (str.compare("GetJoinedClientNickname") == 0) {
			SendFbPacket(pkt.SessionIndex, GetJoinedClientNickname(), 0, 0, "GetJoinedClientNickname");
		}
		else if (str.compare("AllPlayerJoined") == 0) {
			for (int i = 0; i < 3; i++) {
				SendFbPacket(i, "", 0, 0, "AllPlayerJoined");
			}
		}
	}

	void StartGame()
	{
		std::cout << "StartGame!\n";
		deck = Deck::GetDeck();
		Deck::SetCardValues(cardValues);


		SendFbPacket(0, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 0, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 1, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 2, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		deck.pop();
		SendFbPacket(0, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		SendFbPacket(1, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		SendFbPacket(2, "first", deck.front(), 3, "SetCard");
		Sleep(50);
		deck.pop();
		Sleep(1000);
		SendFbPacket(0, "", 0, 0, "DealStart");
		Sleep(10);
		SendFbPacket(1, "", 0, 0, "DealStart");
		Sleep(10);
		SendFbPacket(2, "", 0, 0, "DealStart");
		deck.pop();
	}


};