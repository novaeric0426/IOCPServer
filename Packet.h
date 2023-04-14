#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "flatbuffers/flatbuffers.h"

//클라이언트가 보낸 패킷을 저장하는 구조체 (보낼 때는 그냥 받은 패킷에서 정보만 빼서 바로 SendMsg함)
struct PacketData
{
	UINT32 SessionIndex = 0;
	UINT32 DataSize = 0;
	char* pPacketData = nullptr;

	void Set(PacketData& value)
	{
		


		SessionIndex = value.SessionIndex;
		DataSize = value.DataSize;

		pPacketData = new char[value.DataSize];
		CopyMemory(pPacketData, value.pPacketData, value.DataSize);
	}

	void Set(UINT32 sessionIndex_, UINT32 dataSize_, char* pData)
	{
		SessionIndex = sessionIndex_;
		DataSize = dataSize_;
		
		pPacketData = new char[dataSize_];
		CopyMemory(pPacketData, pData, dataSize_);
	}

	void Release()
	{
		delete pPacketData;
	}
};