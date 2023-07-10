#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "common.h"



int socketID;
struct sockaddr_in server_addr, client_addr;
int len;
PKT request, response;
int winSize, probLoss, timeout;


void executeCommand(char command, char* filename);
char *getFileList();
void sendRequestAck();



int main() {
    int n;
    // creo la socket
    if ((socketID = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() in server failed\n");
        exit(-1);
    }

    // inizializzo la socket
    memset((void *) &server_addr, 0, sizeof(server_addr));
    memset((void *) &client_addr, 0, sizeof(client_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);



    if ((bind(socketID, (const struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
        perror("bind() failed\n");
        exit(-1);
    }

    // ricevo comando ed eseguo
    len = sizeof(client_addr);
    n = recvfrom(socketID, &request, sizeof(request), MSG_WAITALL, (struct sockaddr *) &client_addr, &len);
    if (n < 0) {
        perror("recvfrom(): ");
        exit(-1);
    }

    winSize = request.header_pkt.winSize;
    probLoss = request.header_pkt.probability;
    timeout = request.header_pkt.timeout;


    executeCommand(request.header_pkt.command[0], request.payload);

    printf("Shutting down.\n");
    return 0;
}



char *getFileList(){
    char* file_list;
    char* list_command;
    FILE *pipe;
    int input;
    size_t input_length = 0;

    //inizializzo la lista con dimensione scelta
    size_t file_list_size = INITIAL_FILE_LIST_SIZE;
    file_list = malloc(file_list_size);

    list_command = malloc(40*sizeof(char));
    if(sprintf(list_command, "ls ServerFile -p | grep -v /") == -1){
        perror("sprintf ");
        exit(-1);
    }


    if((pipe = popen(list_command, "r")) == NULL){
        perror("popen: ");
        exit(-1);
    }


    do {
        input = getc(pipe);
        if (input == EOF) {
            if (feof(pipe)) input = '\0';
            else {
                int error = ferror(pipe);//togliere
                perror("error\n");
                exit(-1);
            }
        }
        input_length++;


        if(input_length > file_list_size){
            char *tmp;
            file_list_size <<= 1;
            if((tmp = realloc(file_list, file_list_size)) == NULL){
                perror("realloc: ");
                exit(-1);
            }
            file_list = tmp;
        }

        file_list[input_length-1] = (char) input;


    }while(input!='\0');


    if(pclose(pipe) == -1){
        perror("pclose: ");
        exit(-1);
    }

    return file_list;

}


//invia l'ack di risposta al comando richiesto
void sendRequestAck(){
    ACK ack;
    ack.confirmOperation = true;
    if(sendto(socketID, &ack, sizeof(ack), MSG_CONFIRM, (const struct sockaddr *) &client_addr, len) < 0){
        perror("sendto(): ");
        exit(-1);
    }
}



void executeCommand(char command, char* filename){
    PKT packet;
    char dir[30]  = "ServerFile/";
    char *path;

    switch (command) {
        case LIST:
            sendRequestAck();
            char *list = getFileList();
            strcpy(packet.payload,list);
            packet.header_pkt.size = sizeof(packet.payload);
            if(sendto(socketID, &packet, sizeof(packet), MSG_CONFIRM, (const struct sockaddr *) &client_addr, len) < 0){
                perror("sendto(): ");
                exit(-1);
            }
            break;

        case GET:
            sendRequestAck();
            path = strcat(dir, filename);
            sendFile(socketID, client_addr, path, winSize, probLoss, timeout);
            break;

        case PUT:
            sendRequestAck();
            path = strcat(dir, filename);
            receiveFile(socketID,client_addr, path, winSize);
            break;
        default:
            printf("Unknown command");

    }

    return ;
}






