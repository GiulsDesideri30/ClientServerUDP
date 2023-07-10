#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"


bool *ackList;
time_t *pktTimeList;
bool *pktSentList;
int lastAckRecv, lastPktSent;
pthread_mutex_t windowParamMutex = PTHREAD_MUTEX_INITIALIZER;
struct sockaddr_in ack_sender_addr;


//calcolo se il pacchetto deve essere inviato o no
bool lossPkt(int prob){
    int r;
    r = rand() % 100;
    if(r <= prob-1) return true;
    return false;


}


// funzione di ascolto degli ack eseguita dal thread
void *listenAck(void *socketId){
    ACK ack;
    int ackSize;
    int ackSeqNum;

    while(true){
        socklen_t ack_sender_addr_size;
        ackSize = recvfrom((int)socketId, &ack, sizeof(ack), MSG_WAITALL, (struct sockaddr *)&ack_sender_addr, &ack_sender_addr_size);
        if(ackSize < 0){
            perror("ackSize recvfrom(): ");
            exit(-1);
        }
        printf("Thread: received ack number %d\n",ack.seq_num);
        ackSeqNum = ack.seq_num;

        pthread_mutex_lock(&windowParamMutex);

        if((ackSeqNum > lastAckRecv) && (ackSeqNum <= lastPktSent)){
            ackList[ackSeqNum - (lastAckRecv + 1)] = true;
        }
        pthread_mutex_unlock(&windowParamMutex);
    }
}



PKT createPkt(int seqNum, char *payload, int payloadSize, bool isLast){
    PKT pkt;
    pkt.header_pkt.seqNum = seqNum;
    strncpy(pkt.payload, payload, payloadSize);
    pkt.header_pkt.size = payloadSize;
    pkt.header_pkt.isLastPacket = isLast;

    return pkt;
}



ACK createAck(int seqNum){
    ACK ack;
    ack.seq_num = seqNum;
    return ack;
}

void sendFile(int socketId, struct sockaddr_in receiver_addr, char* path, int winSize, int probLoss, int timeout){
    int prob = probLoss;
    int winLen = winSize;
    int chunkSize, dataSize;
    int maxChunkSize = MAX_CHUNK_SIZE*8;
    char chunk[maxChunkSize]; //TODO: max_chunk_size = MAX_CHUNK_SIZE*(int)atoi(argv[3])
    PKT packet;
    char data[MAX_CHUNK_SIZE];
    bool isRead = false;
    bool sendDone = false;
    ssize_t  len;


    FILE *file;
    if((file = fopen(path, "rb")) == NULL){
        perror("file fopen() error: ");
        exit(-1);
    }

   // int s = sizeof(file);
  //  int nump = s/MAX_DATA_SIZE;

   // int chunkNumber = 0;
    pthread_t ackThread;
    ack_sender_addr = receiver_addr;

    if(pthread_create(&ackThread,NULL,listenAck,(void *) socketId) != 0){
        perror("pthread_create() error: ");
        exit(-1);
    }



    while(!isRead){

        chunkSize = fread(chunk, 1, maxChunkSize, file);
        if(chunkSize == maxChunkSize && feof(file))
            isRead = true;

        if(chunkSize < maxChunkSize){
            if(feof(file)) isRead = true;
            if(ferror(file)){
                printf("Error reading\n");
                exit(-1);
            }
        }

        pthread_mutex_lock(&windowParamMutex);

        //inizializzazione della finestra
        int seqCount = chunkSize / MAX_CHUNK_SIZE + ((chunkSize % MAX_CHUNK_SIZE == 0) ? 0 : 1);
        int seqNum;
        ackList = malloc(winLen*sizeof(bool));
        pktSentList = malloc(winLen*sizeof(bool));
        pktTimeList = malloc(winLen*sizeof(time_t));
        for(int i=0; i<winLen; i++){
            ackList[i] = false;
            pktSentList[i] = false;
        }

        lastAckRecv = -1;
        lastPktSent = winLen + lastAckRecv;

        pthread_mutex_unlock(&windowParamMutex);

        // invio chunk
        sendDone = false;
        while(!sendDone){
            pthread_mutex_lock(&windowParamMutex);
            //provo a shiftare la finestra
            if(ackList[0]){
                int shift = 1;

                for(int i=1; i<winLen; i++){
                    if(!ackList[i]) break;
                    shift++;
                }

                for(int i=0; i<winLen; i++){
                    ackList[i] = ackList[i+shift];
                    pktSentList[i] = pktSentList[i+shift];
                    pktTimeList[i] = pktTimeList[i+shift];
                }

                for(int i=winLen-shift; i<winLen; i++){
                    ackList[i] = false;
                    pktSentList[i] = false;
                }

                lastAckRecv += shift;
                lastPktSent = winLen + lastAckRecv;
            }

            pthread_mutex_unlock(&windowParamMutex);

            // invio pacchetti mancanti( mai inviati, non confermati da ack, o timeout scaduto)

            for(int i=0; i<winLen; i++){
                seqNum = lastAckRecv + i + 1;
                if(seqNum < seqCount){
                    pthread_mutex_lock(&windowParamMutex);


                    if(!pktSentList[i] || (!ackList[i] && (difftime(time(0), pktTimeList[i])>timeout))){
                        printf("Resending pkt: %d\n", i);
                        int chunkShift = seqNum * MAX_CHUNK_SIZE; // era MAX_DATA_SIZE
                        dataSize = (chunkSize - chunkShift < MAX_CHUNK_SIZE) ? (chunkSize - chunkShift) : MAX_CHUNK_SIZE;
                        memcpy(data, chunk+chunkShift, dataSize);

                        bool lastPkt = false;
                        if((seqNum == seqCount-1) && isRead){
                            lastPkt = true;
                            printf("Packet %d is last packet\n", seqNum);

                        }

                        packet = createPkt(seqNum, data, dataSize, lastPkt);
                        if(!lossPkt(prob)) {
                            if (sendto(socketId, &packet, sizeof(packet), 0, (struct sockaddr *) &receiver_addr,
                                       sizeof(receiver_addr)) < 0) {
                                perror("eot sendto(): ");
                                exit(-1);
                            }
                        }
                        printf("packet %d sent\n", packet.header_pkt.seqNum);

                        pktSentList[i] = true;
                        pktTimeList[i] = time(0);
                    }

                    pthread_mutex_unlock(&windowParamMutex);
                }
            }
            if(lastAckRecv >= seqCount - 1) sendDone = true;
        }
       // chunkNumber++;
        if(isRead) break;
    }

    fclose(file);
    free(ackList);
    free(pktSentList);
    free(pktTimeList);
    pthread_detach(ackThread);
}

void receiveFile(int socketId, struct sockaddr_in sender_addr, char* path, int winSize){
    int winLen = winSize;
    int i;
    int maxBufferSize = MAX_CHUNK_SIZE * 8;
    FILE *file;

    if((file = fopen(path, "wb")) == NULL){
        perror("file fopen() error: ");
        exit(-1);
    }

    char buffer[maxBufferSize];



    //inizializzo variabili finestra scorrevole
    PKT packet;
    //char data[MAX_DATA_SIZE];
   // char ack[ACK_SIZE];
    int packetSize;
    int dataSize;
    int seqNum;
    int lastPktRecv;
    int lastAckSent;
    bool eot;

    //ricevo pacchetti fino a EOT
    bool recvDone = false;
    int bufferNum = 0;
    while (!recvDone) {

        int bufferSize = maxBufferSize;
        memset(buffer, 0, bufferSize);

        int maxCount = (int) maxBufferSize / MAX_DATA_SIZE;
        bool windowRecv[winLen];
        for (i = 0; i < winLen; i++) {
            windowRecv[i] = false;
        }
        lastPktRecv = -1;
        lastAckSent = lastPktRecv + winLen;

        while (true) {
            socklen_t sender_addr_size = sizeof(sender_addr);

            packetSize = recvfrom(socketId, &packet, sizeof(packet), MSG_WAITALL,
                                      (struct sockaddr *) &sender_addr, &sender_addr_size);


            if (packetSize < 1) {
                perror("recvfrom() nella receive(): \n");
                exit(-1);
            }

            seqNum = packet.header_pkt.seqNum;
            eot = packet.header_pkt.isLastPacket;
            dataSize = packet.header_pkt.size;

            // creo ack di risposta e invio
            ACK ack = createAck(seqNum);

            if (sendto(socketId, &ack, sizeof(ack), MSG_CONFIRM, (struct sockaddr *) &sender_addr,
                       sizeof(sender_addr)) < 0) {
                perror("sendto error\n");
                exit(-1);
            }
            printf("Ack %d sent.\n",seqNum);

            if (seqNum <= lastAckSent) {
                int bufferShift = seqNum * MAX_DATA_SIZE;

                if (seqNum == lastPktRecv + 1) {
                    memcpy(buffer + bufferShift, packet.payload, dataSize);
                    //shifto la finestra
                    int shift = 1;
                    for (i = 1; i < winLen; i++) {
                        if (!windowRecv[i]) break;
                        shift += 1;
                    }

                    for (i = 0; i < winLen-shift; i++) {
                        windowRecv[i] = windowRecv[i + shift];
                    }

                    /***** EDIT: corretto errore sul ciclo for per scorrimento finestra *****/
                    for (i = winLen - shift; i < winLen; i++) {
                        windowRecv[i] = false;
                    }
                    lastPktRecv += shift;
                    lastAckSent = lastPktRecv + winLen;
                } else if (seqNum > lastPktRecv + 1) {
                    if (!windowRecv[seqNum - (lastPktRecv + 1)]) {
                        memcpy(buffer + bufferShift, packet.payload, dataSize);
                        windowRecv[seqNum - (lastPktRecv + 1)] = true;
                    }
                }

                if (eot) {
                    bufferSize = bufferShift + dataSize;
                    maxCount = seqNum + 1;
                    recvDone = true;
                }
            }
            if (lastPktRecv >= maxCount - 1) break;
        }

        fwrite(buffer, 1, bufferSize, file);
        bufferNum += 1;
    }

    fclose(file);
}


