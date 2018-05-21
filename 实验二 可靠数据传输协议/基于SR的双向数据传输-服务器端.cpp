#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdlib.h> 
#include <time.h> 
#include <WinSock2.h> 
#include <fstream> 
#include<io.h>   //C语言头文件
#include<iostream>   //for system();

#pragma comment(lib,"ws2_32.lib") 
using namespace std;
#define SERVER_PORT  12340  //端口号 
#define SERVER_IP    "127.0.0.1"  //IP 地址 
const int BUFFER_LENGTH = 1026;    //缓冲区大小，（以太网中 UDP 的数据帧中包长度应小于 1480 字节） 
const int SEND_WIND_SIZE = 10;//发送窗口大小为 10，GBN 中应满足  W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
//本例取序列号 0...19 共 20 个 
//如果将窗口大小设为 1，则为停-等协议 

const int SEQ_SIZE = 20; //序列号的个数，从 0~19 共计 20 个 
//由于发送数据第一个字节如果值为 0，则数据会发送失败
//因此接收端序列号为 1~20，与发送端一一对应 

int ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack 
int curSeq;//当前数据包的 seq 
int curAck;//当前等待确认的 ack 
int totalSeq;//收到的包的总数 
int totalPacket;//需要发送的包总数 
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
//************************************ 
// Method:        getCurTime 
// FullName:    getCurTime 
// Access:        public   
// Returns:      void 
// Qualifier:  获取当前系统时间，结果存入 ptime 中 
// Parameter: char * ptime 
//************************************ 
void getCurTime(char *ptime){
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm *p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d",
		p->tm_year + 1900,
		p->tm_mon,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	strcpy_s(ptime, sizeof(buffer), buffer);
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
	if (step >= SEND_WIND_SIZE){
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
	for (int i = 0; i< SEND_WIND_SIZE; ++i){
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
	if (index == 0){
		printf("sadas");
	}
	if (curAck <= index && (curAck + SEND_WIND_SIZE >= SEQ_SIZE ? true : index<curAck + SEND_WIND_SIZE)){

		ack[index] = 3;
		while (ack[curAck] == 3){
			ack[curAck] = 1;
			curAck = (curAck + 1) % SEQ_SIZE;

		}
	}
}

//主函数 
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
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else{
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) &iMode);//非阻塞设置 
	SOCKADDR_IN addrServer;   //服务器地址 
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP); 
	//addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//两者均可 
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err){
		err = GetLastError();
		printf("Could  not  bind  the  port  %d  for  socket.Error  code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}

	SOCKADDR_IN addrClient;   //客户端地址 
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //数据发送接收缓冲区 
	ZeroMemory(buffer, sizeof(buffer));
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

	totalPacket = length_lvxiya / 1024 + 1;
	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i){
		ack[i] = 1;
	}
	
	int ret;
	int interval = 1;
	float packetLossRatio = 0;  //默认包丢失率 0.2 
	float ackLossRatio = 0;  //默认 ACK 丢失率 0.2 
	//用时间作为随机种子，放在循环的最外面 
	srand((unsigned)time(NULL));

	while (true){
		//非阻塞接收，若没有收到数据，返回值为-1 
		recvSize =
			recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		if (recvSize < 0){
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0){
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0){
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
		}
		else if (strcmp(buffer, "-testgbn") == 0){
			//进入 gbn 测试阶段 
			//首先 server（server 处于 0 状态）向 client 发送 205 状态码（server进入 1 状态） 
			//server  等待 client 回复 200 状态码，如果收到（server 进入 2 状态），	则开始传输文件，否则延时等待直至超时\
																//在文件传输阶段，server 发送窗口大小设为 
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			//加入了一个握手阶段 
			//首先服务器向客户端发送一个 205 大小的状态码（我自己定义的）表示服务器准备好了，可以发送数据
			//客户端收到 205 之后回复一个 200 大小的状态码，表示客户端准备好了，可以接收数据了
			//服务器收到 200 状态码之后，就开始使用 GBN 发送数据了 
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The  loss  ratio  of  packet  is  %.2f,the  loss  ratio  of  ack is %.2f\n", packetLossRatio, ackLossRatio);
			//----------------------------------------------------
			int waitCount = 0;//---------------
			BOOL b;
			unsigned short seq = 0;//包的序列号 
			unsigned short recvSeq = 0;//已确认的最大序列号 
			unsigned short waitSeq = 1;//等待的序列号 ，窗口大小为10，这个为最小的值
			char buffer_1[SEND_WIND_SIZE][BUFFER_LENGTH];//接收到的缓冲区数据----------------add bylvxiya
			int i_state = 0;
			for (i_state = 0; i_state < SEND_WIND_SIZE; i_state++)
			{
				ZeroMemory(buffer_1[i_state], sizeof(buffer_1[i_state]));
			}

			BOOL ack_send[SEND_WIND_SIZE];//ack发送情况的记录，对应1-20的ack,刚开始全为false
			int success_number = 0;// 窗口内成功接收的个数
			for (i_state = 0; i_state < SEND_WIND_SIZE; i_state++){//记录哪一个成功接收了
				ack_send[i_state] = false;
			}
			std::ofstream out_result;
			out_result.open("result.txt", std::ios::out | std::ios::trunc);
			if (!out_result.is_open()){
				printf("文件打开失败！！！\n");
				continue;
			}
			
			//------------------------------------------------------------------------
			while (runFlag){
				int recv_lvxy = 0;
				switch (stage){
				case 0://发送 205 阶段 
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0,
						(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://等待接收 200 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
					
					recv_lvxy =
						recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recv_lvxy < 0){
						++waitCount;
						if (waitCount > 20){
							runFlag = false;
							printf("Timeout error\n");
							break;
						}
						Sleep(500);
						continue;
					}
					else{
						if ((unsigned char)buffer[0] == 200){
							printf("Begin a file transfer\n");
							printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
							curSeq = 0;
							curAck = 0;
							totalSeq = 0;
							waitCount = 0;
							stage = 2;
							
						}
					}
					break;
				case 2://数据传输阶段 					
					if (seqIsAvailable() && totalSeq - SEND_WIND_SIZE <= totalPacket){
						
						/*if (!recvSeq){
							b = lossInLossRatio(ackLossRatio);
							//发送给客户端的序列号从 1 开始 
							buffer[0] = curSeq + 1;
							if (b){
								printf("The  packet from client  of  %d  loss\n", (unsigned char)buffer[0]);
								break;
							}

							buffer[1] = seq;//-+++++++++++++++++++++待定，是不是0

							ack[curSeq] = 0;

							//数据发送的过程中应该判断是否传输完成 
							//为简化过程此处并未实现 
							memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
							printf("SERVER：send a packet with a seq of %d and a ack of %d\n", curSeq + 1, seq);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0,
								(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
						}
						recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
						if (recvSize < 0){
						//	printf("%d", WSAGetLastError());
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
							if (seq >= waitSeq && (waitSeq + SEND_WIND_SIZE > SEQ_SIZE ? true : seq < (waitSeq + SEND_WIND_SIZE)))
							{
								memcpy(buffer_1[seq - waitSeq], &buffer[2], sizeof(buffer));
								ack_send[seq - waitSeq] = true;
								int ack_s = 0;
								while (ack_send[ack_s] && ack_s < SEND_WIND_SIZE){
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
									for (int i = 0; i < SEND_WIND_SIZE; i++){
										if (ack_s + i < SEND_WIND_SIZE)
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
							if ((unsigned short)buffer[0]){
								ackHandler(buffer[1]);
								waitCount = 0;
							}
							buffer[1] = seq;

							buffer[0] = curSeq + 1;
							b = lossInLossRatio(ackLossRatio);
							if (b){
								printf("The  packet from Client  of  %d and ACK for server of %d loss\n", (unsigned char)buffer[0], seq);
								break;
							}
							ack[curSeq] = 0;
							//数据发送的过程中应该判断是否传输完成 
							//为简化过程此处并未实现 
							memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
							printf("send a packet from Client  of  %d and ACK for server of %d \n", curSeq + 1, seq);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0,
								(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							//Sleep(500);
							//break;
						
							*/
						
						
						b = lossInLossRatio(ackLossRatio);
						//发送给客户端的序列号从 1 开始 
						buffer[0] = curSeq + 1;
						if (b){
							printf("The  packet from client  of  %d  loss\n", (unsigned char)buffer[0]);
							break;
						}
						
						buffer[1] = seq;//-+++++++++++++++++++++待定，是不是0
						
						ack[curSeq] = 0;
						
							//数据发送的过程中应该判断是否传输完成 
							//为简化过程此处并未实现 
						memcpy(&buffer[2], data + 1024 * (curSeq + (totalSeq / SEND_WIND_SIZE)*SEND_WIND_SIZE), 1024);
						printf("SERVER：send a packet with a seq of %d and a ack of %d\n", curSeq + 1, seq);
						sendto(sockServer, buffer, BUFFER_LENGTH, 0,
							(SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						++curSeq;
						curSeq %= SEQ_SIZE;
						++totalSeq;
						//Sleep(500);

						recv_lvxy =
							recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
						//recv_lvxy =
							//recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
						if (recv_lvxy < 0){
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
							//注意这时ack--(unsigned short)buffer[1];的话要看看是不是0，如果是0的话表示对方没有收过数据，是不能ackHandel的；
							if ((unsigned short)buffer[1]!=0){
								ackHandler(buffer[1]);
								waitCount = 0;
							}
							//--------------------------------——————————————————
							seq = (unsigned short)buffer[0];
							printf("client packet: %d, ACK for SERVER: %d\n", seq, buffer[1]);
							if (seq >= waitSeq && (waitSeq + SEND_WIND_SIZE > SEQ_SIZE ? true : seq < (waitSeq + SEND_WIND_SIZE))){//在接收窗口范围内
								memcpy(buffer_1[seq - waitSeq], &buffer[2], sizeof(buffer));
								ack_send[seq - waitSeq] = true;
								int ack_s = 0;
								while (ack_send[ack_s] && ack_s < SEND_WIND_SIZE){
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
									for (int i = 0; i < SEND_WIND_SIZE; i++){
										if (ack_s + i < SEND_WIND_SIZE)
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
							buffer[1] = seq;
						}
						
					}
					else if (curSeq>0&&curSeq - curAck >= 0 ? curSeq - curAck <= SEND_WIND_SIZE : curSeq - curAck + SEQ_SIZE <= SEND_WIND_SIZE && totalSeq - SEND_WIND_SIZE <= totalPacket){
						
						curSeq++;
						curSeq %= SEQ_SIZE;
					}
					else{
						waitCount++;
						
						if (waitCount > 20)
						{
							timeoutHandler();
							waitCount = 0;
							//recvSeq = 0;
						}
						
					}
					/*
					else if (totalSeq - SEND_WIND_SIZE > totalPacket){
						memcpy(buffer, "good bye\0", 9);
						runFlag = false;
						break;
					}*/ 
					Sleep(500);
					break;
				}
			}
			success:out_result.close();
		}
		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient,
			sizeof(SOCKADDR));
		Sleep(500);
		
	}
	//关闭套接字，卸载库 
	closesocket(sockServer);
	WSACleanup();
	return 0;
}