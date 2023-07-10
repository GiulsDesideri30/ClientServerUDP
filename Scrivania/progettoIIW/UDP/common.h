#ifndef PROGETTO_IIW_COMMON_H
#define PROGETTO_IIW_COMMON_H


#include <bits/types/clock_t.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>

#define MAX_PACKET_SIZE 522  //in bytes
#define MAX_CHUNK_SIZE 512
#define MAX_DATA_SIZE 512

#define PORT 8080
#define GET 'G'
#define PUT 'P'
#define LIST 'L'
#define INITIAL_FILE_LIST_SIZE 300


typedef struct header_pkt{
    int seqNum; //offset of first byte
    int size;   // size of payload
    bool isLastPacket; // 1 --> yes
    char command[20];
    int winSize;
    int probability;
    int timeout;
}PKT_HEADER;

typedef struct pkt{
    PKT_HEADER header_pkt;
    char payload[MAX_DATA_SIZE+1]; //modificato post consegna
}PKT;

typedef struct ack{
    int seq_num;
    bool confirmOperation;
}ACK;




void *listenAck(void *socketId);

PKT createPkt(int seqNum, char *payload, int payloadSize, bool isLast);

ACK createAck(int seqNum);

void sendFile(int socketId, struct sockaddr_in receiver_addr, char* path, int winSize, int probLoss, int timeout);

void receiveFile(int socketId, struct sockaddr_in sender_addr, char* path, int winSize);

bool lossPkt(int prob);


#endif //PROGETTO_IIW_COMMON_H
