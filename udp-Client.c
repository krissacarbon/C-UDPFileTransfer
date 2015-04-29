/*
 File: udp-Server.c
 By: Gerard Andrew C. Ursabia
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <math.h>
#include <time.h>
#include "port.h"

#define BUFLEN 2048
#define MSGS 1	/* number of messages to send */
#define MAX_TASKS 5

//socket interaction functions
void recvMsgs(socklen_t sock, struct sockaddr_in addr, socklen_t len);
void sendAck(socklen_t sock, struct sockaddr_in addr, socklen_t len);

//packet related functions
void insertPacketsToLinkList(unsigned char data[], int start, int end, int chunk);
void insertAckToQueue(int num);
void traverseCurrAck(int n);
void display();
void tokenizePacket(char *data, char *delim, char *dataUntokened);

//util functions
int strtoint_n(char* str, int n);
int strtoint(char* str);
void tostring(char str[], int num);
void delay(int milliseconds);

//structs al FIFO
struct node
{
    unsigned char data[BUFLEN];
    int startSeqNumber;
    int endSeqNumber;
    struct node *next;
}*head, *tail;

struct ack
{
    int ackNum;
    struct ack *next;
}*first, *last, *curr;

struct tasklist {
    int task_count;
    void (*task_function[MAX_TASKS])(socklen_t sock, struct sockaddr_in addr, socklen_t len);
};

//declarations
char filename[BUFLEN], windowsize[BUFLEN], chunksize[BUFLEN], isn[BUFLEN], timeout[BUFLEN], delayAck[BUFLEN], addrPort[BUFLEN], config[BUFLEN];
int delayInMilSec;
char buf[BUFLEN], bufSend[BUFLEN];	/* message buffer */
int recvlen; /* recv message length */
int complete_flag=0;

int main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t fd, i, slen=sizeof(remaddr);
    int msgcnt=0, n=0;		/* # bytes in acknowledgement message */
	char *server = "127.0.0.1";	/* change this to use a different server */
    int isVerbose = 0;
    unsigned char *currData;

    
    /*get arguments*/
    
    for (n=1; n < argc; n++) {
        if (strstr(argv[n], "-w") != NULL) {
            strcpy(windowsize,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-f") != NULL) {
            strcpy(filename,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-c") != NULL) {
            strcpy(chunksize,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-i") != NULL) {
            strcpy(isn,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-t") != NULL) {
            strcpy(timeout,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-d") != NULL) {
            strcpy(delayAck,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-r") != NULL) {
            strcpy(addrPort,argv[n+1]);
            n++;
        }else if(strstr(argv[n], "-v") != NULL) {
            isVerbose = 1;
        }
    }
    
    printf("delayInMilSec is %d\n", delayInMilSec);
    
    strcat(config, "filename=");
    strcat(config, filename);
    strcat(config, ",");
    strcat(config, "windowsize=");
    strcat(config, windowsize);
    strcat(config, ",");
    strcat(config, "chunksize=");
    strcat(config, chunksize);
    strcat(config, ",");
    strcat(config, "isn=");
    strcat(config, isn);
    strcat(config, ",");
    strcat(config, "timeout=");
    strcat(config, timeout);
    strcat(config, ",");
    strcat(config, "addrPort=");
    strcat(config, addrPort);
	/* create a socket */

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
		printf("socket created\n");

	/* bind it to all local addresses and pick any port number */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}       

	/* now define remaddr, the address to whom we want to send messages */
	/* For convenience, the host address is expressed as a numeric IP address */
	/* that we will convert to a binary format via inet_aton */

	memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(SERVICE_PORT);
	if (inet_aton(server, &remaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
    
    /* Send the configurations to server */
    strcpy(buf, config);
    if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
        perror("sendto");
    }
    
    int j;
    struct tasklist tl;
    
    tl.task_count = 2;
    tl.task_function[0] = recvMsgs;
    tl.task_function[1] = sendAck;
    
    while (1){
        
//        /*start listening*/
//        recvMsgs(fd, remaddr, slen);
//
//        /*send acknowledgements*/
//        sendAck(fd, remaddr, slen);
        for (j = 0; j < tl.task_count; j++) {
            tl.task_function[j](fd, remaddr, slen);   // execute the task's function
        }
    
    }
	return 0;
}

void recvMsgs(socklen_t sock, struct sockaddr_in addr, socklen_t len){
    int tempStartInt = 0, chunksizeInt = strtoint(chunksize);
    char temp_data[chunksizeInt], temp_start[11];
    
    recvlen = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&addr, &len);
    
    if (recvlen >= 0){
        buf[recvlen] = 0;	/* expect a printable string - terminate it */
        if (strstr(buf, "ssn") != NULL) {/*receiving data packets*/
            
            printf("received packet : %s \n", buf);
            
            strncpy(temp_data, buf, strtoint(chunksize));
            
            tokenizePacket(buf,"-:",temp_data);
            
            if (tail != NULL) {
                tempStartInt = tail->startSeqNumber+strtoint(chunksize);
            }else {
                tempStartInt = head->startSeqNumber+strtoint(chunksize);
            }
            
            printf("insertAckToQueue : %d \n", tempStartInt);
            insertAckToQueue(tempStartInt);
            
        }else{
            //TODO: finalize completion handling
            printf("received complete message : %s \n", buf);
            complete_flag=1;
            display(); //writes to file
        }
    }

}

void sendAck(socklen_t sock, struct sockaddr_in addr, socklen_t len){
    int x = 0;
    char ackNumString[11];
    
    delayInMilSec = strtoint(delayAck)*1000;
    
    if (complete_flag == 1) {
        return;
    }
    
    do{
        
//        printf("trying to traverse\n");
        traverseCurrAck(1);
        
        strcpy(bufSend,"ACK-");
        tostring(ackNumString, curr->ackNum);
        strcat(bufSend,ackNumString);
        
        for(x=0;x<1;x++)
        {
            printf("%d\n",x+1);
            
            delay(delayInMilSec);
            
            printf("sending \"%s\" to server\n", bufSend);
            
            if (sendto(sock, bufSend, strlen(bufSend), 0, (struct sockaddr *)&addr, len)==-1) {
                perror("sendto");
            }
        }
        
    }while (curr != last);

}
void insertPacketsToLinkList(unsigned char data[], int start, int end, int chunk){
    int i;
    struct node *temp;
    temp=(struct node*)malloc(sizeof(struct node));
//    printf("from buffer:%s\n",data);
    //    strcpy(temp->data,data);
    //    memcpy(temp->data, data, strlen(data)+1);
    for (i=0;i<chunk;i++){
        temp->data[i]=data[i];
    }
    temp->data[i]='\0';
    temp->startSeqNumber=start;
    temp->endSeqNumber=end;
    if (head == NULL) {
        head = temp;
        head->next = NULL;
    }else{
        if (tail == NULL) {
            tail = temp;
            head->next = tail;
            tail->next = NULL;
            
        }else {
            tail->next = temp;
            temp->next = NULL;
            tail = temp;
        }
    }
}

void insertAckToQueue(int num){
    struct ack *temp;
    
//    printf("krissa trying to insert into queue");
    
    temp=(struct ack*)malloc(sizeof(struct ack));
    temp->ackNum=num;
    
    if (first == NULL) {
        first = temp;
        first->next = NULL;
        last=first;//single element would mean it is the first and last
    }else{
        if (last == NULL) { //never going to happen
            last = temp;
            first->next = last;
            last->next = NULL;
        }else {
            last->next = temp;
            temp->next = NULL;
            last = temp;
        }
    }
    
//    printf("last->ackNum in insertAckToQueue %d \n", last->ackNum);
}

void traverseCurrAck(int n){ //traverse base node n times
    int i = 0;
    struct ack *temp;
    
    if (curr == NULL) {
        temp=first;
    }else{
        temp=curr;
        for (i = 0; i<n; i++) {
            temp=temp->next;
        }
    }
    
    curr = temp;
}

void display()
{
    struct node *temp;
    temp=head;
    
    
    FILE *fp;
    
    fp=fopen("test.txt", "wb");
    if(fp == NULL)
        exit(-1);
    
    while(temp!=NULL)
    {
        fprintf(fp, "%s", temp->data);
        temp=temp->next;
    }
    
//    fprintf(fp, "This is a string which is written to a file\n");
    fclose(fp);
}

void tokenizePacket(char *data, char *delim, char *dataUntokened)
{
    char *storage [30];
    char *token;
    char *value;
    char str[1024];
    int pos = 0, i = 0, tokNum = 0, start, end;
    
    /* get the first token */
    token = strtok(data, delim);
    
    /* walk through other tokens */
    while( token != NULL ){
        storage[pos] = token;
//        printf( "tokens at pos %d = %s\n",pos, token );//commented out printf checker
        pos++;
        tokNum++;
        token = strtok(NULL, delim);
    }
//    storage[pos] = NULL;
    
    
    if (strncmp(storage[0], "ssn", 3) == 0) {
        insertPacketsToLinkList(" ",strtoint(storage[1]),strtoint(storage[3]), strtoint(chunksize));
    }else{
        if (strncmp(dataUntokened, "-ssn", 3) == 0 || strncmp(dataUntokened, "ssn", 3)) {
            insertPacketsToLinkList(storage[0],strtoint(storage[2]),strtoint(storage[4]), strtoint(chunksize));
        }else{
            insertPacketsToLinkList(dataUntokened,strtoint(storage[2]),strtoint(storage[4]), strtoint(chunksize));
        }
    }
    
}

int strtoint_n(char* str, int n)
{
    int sign = 1;
    int place = 1;
    int ret = 0;
    
    int i;
    for (i = n-1; i >= 0; i--, place *= 10)
    {
        int c = str[i];
        switch (c)
        {
            case 45:
                if (i == 0) sign = -1;
                else return -1;
                break;
            default:
                if (c >= 48 && c <= 57) ret += (c - 48) * place;
                else return -1;
        }
    }
    
    return sign * ret;
}

int strtoint(char* str)
{
    char* temp = str;
    int n = 0;
    while (*temp != '\0')
    {
        n++;
        temp++;
    }
    return strtoint_n(str, n);
}

void tostring(char str[], int num)
{
    int i, rem, len = 0, n;
    
    n = num;
    while (n != 0)
    {
        len++;
        n /= 10;
    }
    if(len==0){
        str[len]='0';
        len++;
    }
    else
    {
        for (i = 0; i < len; i++)
        {
            rem = num % 10;
            num = num / 10;
            str[len - (i + 1)] = rem + '0';
        }
    }
    str[len] = '\0';
}

void delay(int milliseconds)
{
    long pause;
    clock_t now,then;
    
    pause = milliseconds*(CLOCKS_PER_SEC/1000);
    now = then = clock();
    while( (now-then) < pause )
        now = clock();
}
