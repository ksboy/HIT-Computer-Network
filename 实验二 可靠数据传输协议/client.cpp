#include <stdlib.h>
#include <fstream>
#include "head.h"
#include <WinSock2.h>
#include <time.h>
#include <cstring>
#include <string>
#include <cstdlib>
#include <iostream>
#include <cstdio>
using namespace std;

#pragma comment(lib,"ws2_32.lib")
const int serverPort = 6666;
const string serverIP = "127.0.0.1";
const int BUFFER_LENGTH = 1032;

const int seqSize = 256;//接收端序列号个数，为 1~10

bool lossInLossRatio (float lossRatio)
{
  int lossBound = (int) (lossRatio * 100);
  int r = rand() % 101;
  if(r <= lossBound)
    return TRUE;
  return FALSE;
}

string fileStr;

int main(int argc, char* argv[])
{
	//freopen("clientOutput.txt","w",stdout);
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay\n");
	}

	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(serverIP.c_str());
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(serverPort);

	char buffer[BUFFER_LENGTH];
	memset(buffer, 0, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	int ret;
	int interval = 1;
	string cmd;
	float packetLossRatio = 0.1;
	float ackLossRatio = 0.2;
	srand((unsigned)time(NULL));

	while (true)
	{
		cin >> cmd;
		if (cmd == "get")
			break;
	}

	printf("Begin to test GBN protocol, please don't abort the process");
	printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
	bool b;
	int seq; //包的序列号
	int recvSeq = -1; //已确认的序列号
	int waitSeq = 0; //等待的序列号

	sendto(socketClient, "get", strlen("get") + 1, 0,
		(SOCKADDR*)&addrServer, sizeof(SOCKADDR));

	while (true)
	{
		printf("\n");
		int recvSize;
		recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);

		if (recvSize <= 0)
		{
			printf("recv <0\n");
			continue;
		}

		Header h;
		memcpy(h.Buffer, buffer, recvSize);
		printf("[recv from server]\n");
		h.show();

		b = lossInLossRatio(packetLossRatio);
		if (buffer[3] != '1' && b)
		{
			printf("The packet with a seq of %d loss\n", seq);
			continue;
		}

		seq = h.getSeq();
		printf("recv a packet with a seq of %d\n", seq);

		if (waitSeq == seq)
		{
			++waitSeq;
			if (waitSeq == seqSize)
				waitSeq = 0;
			int len = h.getLength();
			len -= 8;
			char *oneRecv = new char[len + 1];
			printf("[len] %d\n", len);
			memcpy(oneRecv, buffer + 8, len);
			oneRecv[len] = '\0';
			fileStr += oneRecv;
			printf("file size=%u\n", fileStr.length());
			//printf("[recv] %s [/recv]\n",oneRecv);
			recvSeq = seq;
		}
		else
		{
			printf("[DROP!! want=%d, recv=%d!]\n", waitSeq, seq);
			Sleep(200);
			continue;
		}
		b = lossInLossRatio(ackLossRatio);
		if (buffer[3] != '1' && b)
		{
			printf("The ack of %d loss\n", recvSeq);
			Sleep(200);
			continue;
		}
		Header sendHeader;
		sendHeader.setType("ACK0");
		sendHeader.setSeq(recvSeq);
		sendHeader.setLength(8);
		sendto(socketClient, sendHeader.Buffer, 8, 0,
			(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		printf("send a ack of %d\n", recvSeq);

		if (buffer[3] == '1')
			break;
	}
	Sleep(500);
	//cout<<"filestr="<<fileStr<<endl;
	ofstream fout;
	fout.open("client.txt");
	fout << fileStr;
	fout.close();
	//关闭套接字
	closesocket(socketClient);
	WSACleanup();
	return 0;
}

