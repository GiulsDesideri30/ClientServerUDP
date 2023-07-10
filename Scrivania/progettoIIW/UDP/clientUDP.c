#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"


PKT requestPkt, responsePkt;
int socketID;
struct sockaddr_in server_addr, client_addr;
int server_addr_size;
int probLoss;
int window;
int timeout;

void executeCommand(char command);
bool stopAndWait();


int main(int argc,char *argv[]){
    if(argc != 4){
        printf("Usage: loss_probability window_size ack_timeout\n");
        exit(-1);
    }

    probLoss = atoi(argv[1]);
    window = atoi(argv[2]);
    timeout = atoi(argv[3]);


    if(probLoss < 0 || probLoss > 100){
        printf("Prob number is between 0 and 100\n");
        exit(-1);
    }

    // creo la socket del client
    if((socketID = socket(AF_INET,SOCK_DGRAM, 0)) < 0){
     perror("socket() failed\n");
     exit(-1);
    }

    // inserisco le info del server
    memset((void *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //INADDR_ANY --> posso ricevere pacchetti verso la porta richiesta da tutte le interfacce

    printf("\n (P))UT \n (G)ET \n (L)IST \n\n\n");
    scanf("%s", requestPkt.header_pkt.command);
    printf("\n\n\n");

    requestPkt.header_pkt.winSize = window;
    requestPkt.header_pkt.probability = probLoss;
    requestPkt.header_pkt.timeout = timeout;

    executeCommand(requestPkt.header_pkt.command[0]);

    printf("Closing socket and shutting down..\n");
    close(socketID);
    return 0;
}






void executeCommand(char command){
    char fileName[32];
    int n;
    char *path;
    char dir[30] = "ClientFile/";


    switch (command) {
        case LIST:
            if(stopAndWait()){
                n = recvfrom(socketID, &responsePkt, sizeof(responsePkt),MSG_WAITALL, (struct sockaddr *) &server_addr, &server_addr_size);
                if(n < 0) {
                    perror("recvfrom(): ");
                    exit(-1);
                }
                printf("print list: \n%s\n", responsePkt.payload);
            }else{
                printf("Cannot send request to the server. Shutting down.\n");
                return;
            }
            break;

        case GET:
            printf("Please insert file name: ");
            scanf("%s", fileName);
            strncpy(requestPkt.payload, fileName, sizeof(fileName));
            if(stopAndWait()){
                path = strcat(dir,fileName);
                receiveFile(socketID, server_addr, path, window);
            }else{
                printf("Cannot send request to the server. Shutting down.\n");
            }

            break;

        case PUT:
            printf("Please insert file name: ");
            scanf("%s", fileName);
            strcpy(requestPkt.payload, fileName);
            if(stopAndWait()){
                path = strcat(dir, fileName);
                sendFile(socketID, server_addr, path, window, probLoss, timeout);
            }else{
                printf("Cannot send request to the server. Shutting down.\n");
            }
            break;
        default:
            printf("Unknown command");
    }

    return;
}


//stopAndWait() usata per inviare pacchetto di comando e attendere la conferma che sia stato ricevuto
bool stopAndWait(){
    int n;
    ACK ack;

    if(sendto(socketID, &requestPkt, sizeof(requestPkt),MSG_CONFIRM, (const struct sockaddr *) &server_addr,sizeof(server_addr)) < 0){
        perror("sendto: ");
        exit(-1);
    }
    n = recvfrom(socketID, &ack, sizeof(ack),MSG_WAITALL, (struct sockaddr *) &server_addr, &server_addr_size);
    if(n < 0){
        perror("recvfrom(): ");
        return false;
    }else{
        if(ack.confirmOperation) return true;
        return false;
    }

}


