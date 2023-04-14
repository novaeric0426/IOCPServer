#include "BlackJackServer.h"
#include<string>
#include<iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 3;		//�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��

int main()
{
	BlackJackServer server;

	//������ �ʱ�ȭ
	server.InitSocket();

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);
	printf("�ƹ� Ű�� ���� ������ ����մϴ�\n");

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
}