#include "sysInclude.h"
#include <iostream>
#include <vector>
using namespace std;

extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address( );

typedef unsigned int uint;

uint get8(char *buffer)
{
  return (uint)buffer[0] & 0xFF;
}

uint get16(char *buffer)
{
  return (get8(buffer + 0) << 8) + get8(buffer + 1);
}

uint get32(char *buffer)
{
  return (get16(buffer + 0) << 16) + get16(buffer + 2);
}

char setChar(uint i)
{
  return (unsigned char)(i&0xFF);
}

uint getLow(uint IP, uint masklen)
{
  masklen = 32 - masklen;
  IP >>= masklen;
  IP <<= masklen;
  return IP;
}

uint getHigh(uint IP, uint masklen)
{
  masklen = 32 - masklen;
  IP |= (1 << masklen) -1;
  return IP;
}

struct route
{
  uint low,high;
  uint masklen;
  uint nextIP;
  route(uint low, uint high, uint masklen, uint nextIP)
  {
    this->low = low;
    this->high = high;
    this->masklen = masklen;
    this->nextIP = nextIP;
  }
};

vector<route> vec;

void stud_Route_Init()
{
  vec.clear();
	return;
}

void stud_route_add(stud_route_msg *proute)
{
  //printf("dest=%u\nmasklen=%u\nnexthop=%u\n", proute->dest, proute->masklen, proute->nexthop);
  //std::cout<<htonl(proute->dest)<<" "<<htonl(proute->masklen)<<" "<<htonl(proute->nexthop)<<std::endl;
  uint dest = htonl(proute->dest);
  uint masklen = htonl(proute->masklen);
  uint nextIP = htonl(proute->nexthop);
  uint low = getLow(dest, masklen);
  uint high = getHigh(dest, masklen);
  vec.push_back(route(low, high, masklen, nextIP));
  return;
}


bool getNextIP(uint destIP, uint &nextIP)
{
  uint len = 0;
  bool ret = false;
  for(uint i = 0; i < vec.size(); i++)
    if(vec[i].low <= destIP && vec[i].high >=destIP)
      if(vec[i].masklen >= len)
      {
        len = vec[i].masklen;
        nextIP = vec[i].nextIP;
        ret = true;
      }
  return ret;
}

int stud_fwd_deal(char *pBuffer, int length)
{
  // dest IP
  uint destIP;
  destIP = get32(pBuffer + 16);
  uint localIP;
  localIP = getIpv4Address();
  if(destIP == 0xFFFFFFFF || destIP == localIP)
  {
    fwd_LocalRcv(pBuffer, length);
    return 0;
  }
  // dest IP end

  uint nextIP;
  if(getNextIP(destIP, nextIP))
  {
    // ttl
    uint ttl;
    ttl = get8(pBuffer + 8);
    if(ttl == 0)
    {
      fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
      return 1;
    }
    ttl = ttl - 1;
    pBuffer[8] = setChar(ttl);
    // ttl end

    // check sum
    uint checkSum;
    checkSum = 0;
    int i;
    for(i = 0; i < 20; i+=2)
      if(i != 10)
        checkSum += get16(pBuffer + i);

    while(checkSum > 0xFFFF)
      checkSum = (checkSum >> 16) + (checkSum & 0xFFFF);
    checkSum = ((~checkSum) & 0xFFFF);
    pBuffer[10] = setChar(checkSum >> 8);
    pBuffer[11] = setChar(checkSum & 0xFF);
    // check sum end

    fwd_SendtoLower(pBuffer, length, nextIP);
    return 0;
  }
  else
  {
    fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
    return 1;
  }
}

