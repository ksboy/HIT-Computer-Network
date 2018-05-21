#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include "head.h"
using namespace std;

#pragma comment(lib,"ws2_32.lib")
const int SERVER_PORT = 6666;     //server port
const int BUFFER_LENGTH = 1032;   // 8byte header + 1024byte body, udp tot Buffer < 1480

const int windowSize = 10;       // W + 1 <= totSeqNumber    if W = 1 then wait-stop protocol
const int seqSize = 256;          // if seq = 0, don't send 
bool inWindow[seqSize];           // seq is in window
Header *window[seqSize];          // store the send data, for resend
int lastNeedSeq;                  // last not ack seq
int lastSendSeq;                  // last send seq

bool seqIsAvailable ()
{
  if (lastSendSeq == -1)
    return true;
  int step;
  step = lastSendSeq - lastNeedSeq;
  step = step >= 0 ? step : step + seqSize;
  if (step +1 >= windowSize)
    return false;
  return true;
}


void ackHandler (int ack, int &ackBlock)
{
  //printf("[Recv a ack of %d, lastseq=%d]\n", ack, lastNeedSeq);
  if (lastNeedSeq <= ack)
  {
    for(int i = lastNeedSeq; i <= ack; ++i)
    {
      inWindow[i] = false;
      delete window[i];
      ackBlock++;
    }
    lastNeedSeq = (ack + 1) % seqSize;
  }
  else
  {
    for (int i = lastNeedSeq; i< seqSize; ++i)
    {
      inWindow[i] = false;
      delete window[i];
      ackBlock++;
    }
    for (int i = 0; i <= ack; ++i)
    {
      inWindow[i] = false;
      delete window[i];
      ackBlock++;
    }
    lastNeedSeq = ack + 1;
  }
}


int main(int argc, char* argv[])
{
	//freopen("serverOutput.txt","w",stdout);
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
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

	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//no block
	int iMode = 1;
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) &iMode);

	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err)
	{
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is %d\n",
			SERVER_PORT, err);
		WSACleanup();
		return -1;
	}

	SOCKADDR_IN addrClient;
	int length = sizeof(SOCKADDR);
	char recvBuffer[BUFFER_LENGTH];

	char *sendBuffer;
	std::ifstream fin;
	fin.open("server.txt");
	fin.seekg(0, ios::end);
	int fileSize = (int)fin.tellg();
	cout << fileSize << " " << fin.tellg() << endl;
	fin.seekg(0, ios::beg);
	sendBuffer = new char[fileSize + 1];
	fin.read(sendBuffer, fileSize);
	sendBuffer[fileSize] = 0;
	printf("FileSize=%d\n", fileSize);

	//printf("[send buffer]%s[/sendbuffer]\n",sendBuffer);
	int recvSize;
	while (1)
	{
		recvSize = recvfrom(sockServer, recvBuffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		if (recvSize < 0)
			Sleep(200);
		else
			break;
	}

	printf("[recv from client]: %s\n", recvBuffer);
	if (strcmp(recvBuffer, "get") == 0)
	{
		int sendBase = 0;
		int sendSum = fileSize / 1024 + (fileSize % 1024 != 0);
		printf("[send sum=%d]\n", sendSum);
		int waitCount = 0;

		int sendNum = 0;
		int ackNum = 0;
		lastSendSeq = -1;
		lastNeedSeq = 0;

		for (int i = 0; i < seqSize; ++i)
			inWindow[i] = FALSE;

		while (ackNum < sendSum)
		{
			while (sendNum < sendSum && seqIsAvailable())    //send
			{
				lastSendSeq = (lastSendSeq + 1) % seqSize;
				//printf("[available sendSeq=%d]\n", lastSendSeq);
				inWindow[lastSendSeq] = true;
				window[lastSendSeq] = new Header();
				Header *header = window[lastSendSeq];
				printf("sendBase=%d, fileSize=%d\n", sendBase, fileSize);
				header->setType("SND0");
				if (sendBase + 1024 >= fileSize)
					header->setType("SND1");

				header->setSeq(lastSendSeq);
				int headerLength = 1024;
				if (fileSize - sendBase < headerLength)
					headerLength = fileSize - sendBase;
				printf("headerLength = %d\n", headerLength);
				printf("headerLength = %u\n", headerLength);
				memcpy(header->Buffer + 8, sendBuffer + sendBase, headerLength);
				sendBase += headerLength;

				headerLength += 8;

				header->setLength(headerLength);
				printf("headerLength = %d\n", headerLength);

				printf("[send a packet with a seq of %d]\n", lastSendSeq);
				header->show();

				sendto(sockServer, header->Buffer, headerLength, 0,
					(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
				sendNum++;
				if (sendBase >= fileSize)
					break;
				Sleep(200);
			}

			recvSize = recvfrom(sockServer, recvBuffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
			if (recvSize < 0)
			{
				printf("[recv error] %d\n", WSAGetLastError());
				waitCount++;
				if (waitCount > 10)
				{
					printf("\n[resend!!]");
					if (lastSendSeq > lastNeedSeq)
					{
						for (int i = lastNeedSeq; i <= lastSendSeq; i++)
						{
							if (!inWindow[i])
								break;
							printf("[resend seq] = %d\n", i);
							window[i]->show();
							sendto(sockServer, window[i]->Buffer, window[i]->getLength(), 0,
								(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							Sleep(200);
						}
					}
					else
					{
						for (int i = lastNeedSeq; i < windowSize; i++)
						{
							if (!inWindow[i])
								break;
							printf("[resend seq] = %d\n", i);
							window[i]->show();
							sendto(sockServer, window[i]->Buffer, window[i]->getLength(), 0,
								(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							Sleep(200);
						}
						for (int i = 0; i <= lastSendSeq; i++)
						{
							if (!inWindow[i])
								break;
							printf("[resend seq] = %d\n", i);
							window[i]->show();
							sendto(sockServer, window[i]->Buffer, window[i]->getLength(), 0,
								(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							Sleep(200);
						}
					}
					printf("\n");
					waitCount = 0;
				}
			}
			else
			{
				Header header;
				memcpy(header.Buffer, recvBuffer, recvSize);
				//header.show();
				int ack = header.getSeq();

				printf("[recv ack, seq=%d]\n", ack);
				ackHandler(ack, ackNum);
			}
			Sleep(200);
		}
	}
	Sleep(200);
	closesocket(sockServer);
	WSACleanup();
	return 0;
}
