
// GBN_client.cpp :  定义控制台应用程序的入口点。 
// 
//#include "stdafx.h" 
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h> 
#include <fstream> 
#include<io.h>   //C语言头文件
#include<iostream>   //for system();
using namespace std;
#pragma comment(lib,"ws2_32.lib") 

#define SERVER_PORT  12340 //接收数据的端口号 
#define SERVER_PORT_recv  10240
#define SERVER_IP    "127.0.0.1" //  服务器的 IP 地址 

const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//接收端序列号个数，为 1~20 
const int RECV_WIND_SIZE = 10;//接收窗口的大小，他要小于等于序号大小的一半
//由于发送数据第一个字节如果值为 0，则数据会发送失败
//因此接收端序列号为 1~20，与发送端一一对应 

int ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack 
int curSeq;//当前数据包的 seq 
int curAck;//当前等待确认的 ack 
int totalSeq;//收到的包的总数 
int totalPacket;//需要发送的包总数 

/****************************************************************/
/*            -time  从服务器端获取当前时间
-quit  退出客户端
-testgbn [X]  测试 GBN 协议实现可靠数据传输
[X] [0,1]  模拟数据包丢失的概率
[Y] [0,1]  模拟 ACK 丢失的概率
*/
/****************************************************************/
void printTips(){
	printf("*****************************************\n");
	printf("|          -time to get current time                  |\n");
	printf("|          -quit to exit client                            |\n");
	printf("|          -testgbn [X] [Y] to test the gbn    |\n");
	printf("*****************************************\n");
}
//************************************ 
// Method:        seqIsAvailable 
// FullName:    seqIsAvailable 
// Access:        public   
// Returns:      bool 
// Qualifier:  当前序列号  curSeq  是否可用 
//************************************ 
bool seqIsAvailable(){
	int step;
	
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//序列号是否在当前发送窗口之内 
	if (step >= RECV_WIND_SIZE){
		return false;
	}
	if (ack[curSeq] == 1 || ack[curSeq] == 2){
		return true;
	}
	return false;
}
//************************************ 
// Method:        timeoutHandler 
// FullName:    timeoutHandler 
// Access:        public   
// Returns:      void 
// Qualifier:  超时重传处理函数，滑动窗口内的数据帧都要重传 
//************************************ 
void timeoutHandler(){
	printf("Timer out error.\n");
	int index, number = 0;
	for (int i = 0; i< RECV_WIND_SIZE; ++i){
		index = (i + curAck) % SEQ_SIZE;
		if (ack[index] == 0)
		{
			ack[index] = 2;
			number++;
		}
	}
	totalSeq = totalSeq - number;
	curSeq = curAck;
}

//************************************ 
// Method:        ackHandler 
// FullName:    ackHandler 
// Access:        public   
// Returns:      void 
// Qualifier:  收到 ack，累积确认，取数据帧的第一个字节 
//由于发送数据时，第一个字节（序列号）为 0（ASCII）时发送失败，因此加一了，此处需要减一还原
// Parameter: char c 
//************************************ 
void ackHandler(char c){
	unsigned char index = (unsigned char)c - 1; //序列号减一 
	printf("Recv a ack of %d\n", index);
	if (curAck <= index && (curAck + RECV_WIND_SIZE >= SEQ_SIZE ? true : index<curAck + RECV_WIND_SIZE)){

		ack[index] = 3;
		while (ack[curAck] == 3){
			ack[curAck] = 1;
			curAck = (curAck + 1) % SEQ_SIZE;

		}
	}
}
//************************************ 
// Method:        lossInLossRatio 
// FullName:    lossInLossRatio 
// Access:        public   
// Returns:      BOOL 
//  Qualifier:  根据丢失率随机生成一个数字，判断是否丢失,丢失则返回TRUE，否则返回 FALSE
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio){
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound){
		return TRUE;
	}
	return FALSE;
}

int main(int argc, char* argv[])
{
	//加载套接字库（必须） 
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0){
		//找不到 winsock.dll 
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else{
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(socketClient, FIONBIO, (u_long FAR*) &iMode);//非阻塞设置 
	
	

	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	
	
	//接收缓冲区 
	char buffer[BUFFER_LENGTH];//接收数据的缓冲
	//char buffer_send1[BUFFER_LENGTH];//发送数据的缓冲
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//将测试数据读入内存 
	std::ifstream icin;
	icin.open("../test.txt");

	char data[1024 * 113];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * 113);
	icin.close();
	int handle;
	handle = _open("../test.txt", 0x0100); //open file for read
	long length_lvxiya = _filelength(handle); //get length of file
	for (int i = 0; i < SEQ_SIZE; ++i){
		ack[i] = 1;
	}
	totalPacket = length_lvxiya / 1024 + 1;
	//为了测试与服务器的连接，可以使用  -time  命令从服务器端获得当前时间
	//使用  -testgbn [X] [Y]  测试 GBN  其中[X]表示数据包丢失概率 
	//                    [Y]表示 ACK 丢包概率 
	printTips();

	int ret;
	int recvSize = 0;
	int interval = 1;//收到数据包之后返回 ack 的间隔，默认为 1 表示每个都返回 ack，0 或者负数均表示所有的都不返回 ack
	char cmd[128];
	float packetLossRatio = 0.2;  //默认包丢失率 0.2 
	float ackLossRatio = 0.2;  //默认 ACK 丢失率 0.2 
	//用时间作为随机种子，放在循环的最外面 
	srand((unsigned)time(NULL));
	while (true){
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//开始 GBN 测试，使用 GBN 协议实现 UDP 可靠文件传输 
		if (!strcmp(cmd, "-testgbn")){
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//状态码 
			unsigned short seq;//包的序列号 
			unsigned short recvSeq;//已确认的最大序列号 
			unsigned short waitSeq;//等待的序列号 ，窗口大小为10，这个为最小的值
			char buffer_1[RECV_WIND_SIZE][BUFFER_LENGTH];//接收到的缓冲区数据----------------add bylvxiya
			int i_state = 0;
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++){
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}

			BOOL ack_send[RECV_WIND_SIZE];//ack发送情况的记录，对应1-20的ack,刚开始全为false
			int success_number = 0;// 窗口内成功接收的个数
			for (i_state = 0; i_state < RECV_WIND_SIZE; i_state++){//记录哪一个成功接收了
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()){
				printf("文件打开失败！！！\n");
				continue;
			}
			//---------------------------------
			sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0,
				(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			
			while (true)
			{
				//recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage)
				{
				case 0://等待握手阶段 
					do
					{
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
					}while (recvSize < 0);
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205)
					{
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0,
							(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
						curAck = 0;
						totalSeq = 0;
						waitCount = 0;
						curSeq = 0;
					}
					break;
				case 1://等待接收数据阶段 
					if (seqIsAvailable() && totalSeq - RECV_WIND_SIZE <= totalPacket)
					{
						recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
						if (recvSize < 0){
							waitCount++;
							//20 次等待 ack 则超时重传 
							if (waitCount > 20)
							{
								timeoutHandler();
								waitCount = 0;
								
							}
						}
						else{
							b = lossInLossRatio(packetLossRatio);
							if (b){
								printf("The packet from Client with a seq of %d and ACK for SERVER of %d loss\n", buffer[0], buffer[1]);
								break;
							}
							seq = (unsigned short)buffer[0];
							
							printf("recv packet: %d, ACK1 for SERVER:%d\n", seq, (unsigned short)buffer[1]);
							if (seq >= waitSeq && (waitSeq + RECV_WIND_SIZE > SEQ_SIZE ? true : seq < (waitSeq + RECV_WIND_SIZE)))
							{
								memcpy(buffer_1[seq - waitSeq], &buffer[2], sizeof(buffer));
								ack_send[seq - waitSeq] = true;
								int ack_s = 0;
								while (ack_send[ack_s] && ack_s < RECV_WIND_SIZE){
									//向上层传输数据							
									out_result << buffer_1[ack_s];
									//printf("%s",buffer_1[ack_s - 1]);
									ZeroMemory(buffer_1[ack_s], sizeof(buffer_1[ack_s]));
									waitSeq++;
									if (waitSeq == 21){
										waitSeq = 1;
									}
									ack_s = ack_s + 1;
								}
								if (ack_s > 0){
									for (int i = 0; i < RECV_WIND_SIZE; i++){
										if (ack_s + i < RECV_WIND_SIZE)
										{
											ack_send[i] = ack_send[i + ack_s];
											memcpy(buffer_1[i], buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
											ZeroMemory(buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
										}
										else
										{
											ack_send[i] = false;
											ZeroMemory(buffer_1[i], sizeof(buffer_1[i]));
										}

									}
								}
								recvSeq = seq;
							}
							//注意这时ack--(unsigned short)buffer[1];的话要看看是不是0，如果是0的话表示对方没有收过数据，是不能ackHandel的；
							if ((unsigned short)buffer[1]){
								ackHandler(buffer[1]);
								waitCount = 0;
							}
							buffer[1] = seq;
							
							buffer[0] = curSeq + 1;
							b = lossInLossRatio(ackLossRatio);
							if (b){
								printf("The  packet from Client  of  %d and ACK for server of %d loss\n", (unsigned char)buffer[0],seq);
								break;
							}
							ack[curSeq] = 0;
							//数据发送的过程中应该判断是否传输完成 
							//为简化过程此处并未实现 
							memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / RECV_WIND_SIZE)*RECV_WIND_SIZE), 1024);
							printf("send a packet from Client  of  %d and ACK for server of %d \n", curSeq+1, seq);
							sendto(socketClient, buffer, sizeof(buffer), 0,
								(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
							if (buffer[1] == 1){
								printf("dsa");
							}
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							//Sleep(500);
							//break;
						}
					}
					//memcpy(buffer_send1,buffer,sizeof(buffer));
					
					else if (curSeq - curAck >= 0 ? curSeq - curAck <= RECV_WIND_SIZE : curSeq - curAck + SEQ_SIZE <= RECV_WIND_SIZE && totalSeq - RECV_WIND_SIZE <= totalPacket){
						curSeq++;
						curSeq %= SEQ_SIZE;
					}
					else{
						waitCount++;
						if (waitCount > 18)
						{
							timeoutHandler();
							waitCount = 0;
						}
					}
					Sleep(500);
					break;
				}
				//Sleep(500);
			}
			out_result.close();
		}
		sendto(socketClient, buffer, sizeof(buffer), 0,
			(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		do
		{
			ret =
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		} while (ret < 0);
		
			
		
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")){
			break;
		}
		printTips();
	}
	//关闭套接字 
	closesocket(socketClient);
	WSACleanup();
	return 0;
}