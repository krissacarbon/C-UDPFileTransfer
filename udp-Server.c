/*
File: udp-Server.c
By: Gerard Andrew C. Ursabia
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "port.h"
#include <math.h>

#define BUFSIZE 2048
#define ARGSIZE 200
#define MSGS 1
#define PCKTS 5

int createPackets(const char *name, int chunk);

/*Linked list functions*/
void insertPacketsToLinkList(unsigned char *data, int start, int end,int chunk);
void display();
void traverseBase(int n);
void traverseNsn(int n);

/*Configuration functions*/
void tokenizeConfig(char *config, char *delim);
int strtoint_n(char* str, int n);
int strtoint(char* str);
int tokenizeAck(char *msg, char *delim);

/*Send*/
void sendPackets(int sock, struct sockaddr_in rem, socklen_t len);
void toResendFunc(int sock, struct sockaddr_in rem, socklen_t len);
void tostring(char str[], int num);

struct node
{
    unsigned char data[BUFSIZE];
    int startSeqNumber;
    int endSeqNumber;
    struct node *next;
}*head, *tail, *base, *nsn, *toResend;

char filename[ARGSIZE], windowsize[ARGSIZE], chunksize[ARGSIZE], isn[ARGSIZE], timeout[ARGSIZE], addr[ARGSIZE], port[ARGSIZE];

int windowsizeInt, chunksizeInt, isnInt, timeoutInt, fileSize, ackNum, prevAckNum = 0;
int lastpacket=0, timer=0,lastpktflag=0, timerIsOn = 0;
struct sockaddr_in remaddr;	/* remote address */
socklen_t addrlen = sizeof(remaddr);		/* length of addresses */
struct timeval tv;

int main(int argc, char **argv)
{
    struct sockaddr_in myaddr;	/* our address */
    int recvlen;			/* # bytes received */
    int fd, i;				/* our socket */
    int msgcnt = 0;			/* count # of messages we received */
    char buf[BUFSIZE];	/* receive buffer */
    
    
    /* create a UDP socket */
    
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return 0;
    }
    
    /* bind the socket to any valid IP address and a specific port */
    
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(SERVICE_PORT);
    
    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
    }
    
    printf("waiting on port %d\n", SERVICE_PORT);
    
    for (;;) {
    
        timer++;
        printf("timer %d\n", timer);
        
        if (timer == timeoutInt ) {
            if (base==nsn && nsn != NULL) {
                printf("timeout occured but nothing to resend to client \n");
                timerIsOn = 0;
            }else{
                printf("timeout occured resending unacknowledge packets \n");
                toResendFunc(fd, remaddr, addrlen);
                timer = 0; //TODO: Ask Gerard if this should reset
            }
        }
        
        recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        
        tv.tv_sec = timerIsOn;
        tv.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
            perror("Error");
        }
        
        if (recvlen >= 0) {
            buf[recvlen] = 0;	/* expect a printable string - terminate it */
            printf("received message from client: \"%s\"\n", buf);
            
            //listen for configurations from client
            if (strstr(buf, "filename") != NULL) {
                tokenizeConfig(buf, ",=:"); //get configuration and store
                createPackets(filename,chunksizeInt);// fragmentation of the file requested by client
                
                //initial start sending packets
                base = head;
                nsn = head;
                sendPackets(fd, remaddr, addrlen);
                
            } else if(strstr(buf, "ACK") != NULL){
                timerIsOn = 1; //
                timer = 0;//reset timer
                /*TODO:continue to send packets to maximize window size
                 traverse base based on received acknowledged packet seq of client
                 note: ack seq num is actually the seq number expected by the client to receive
                 n = (ackseqnum - base->startseqnum)/chunksize
                 */
                
                ackNum = tokenizeAck(buf,"-");
                if (ackNum < (fileSize+isnInt)) {
                    traverseBase((ackNum - base->startSeqNumber)/chunksizeInt);
                    
                    if (base==nsn) {
                        timerIsOn = 0;
                    }else {
                        timerIsOn = 1;
                    }
                    
                    sendPackets(fd, remaddr, addrlen);
                }else{
                    printf("filetransfer complete \n");
                    printf("nullifying timeout timer, no packets left to send\n");
                    timerIsOn = 0;
                    sprintf(buf, "complete");
                    if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, addrlen) < 0)
                    perror("sendto");
                    
//                    close(fd);
                
                }
            }
            
        }
    }
    
    
    
    //    display();
    /* never exits */
}

void sendPackets(int sock, struct sockaddr_in rem, socklen_t len){
    int i=0;
    char sendPkt[BUFSIZE];
    char temp_start[11],temp_end[11];
    
    memset(sendPkt, 0, sizeof BUFSIZE);
    while(nsn->startSeqNumber < (base->startSeqNumber + (windowsizeInt * chunksizeInt)) && lastpktflag!=1) {
        /* Puts the required contents of the packet to be sent */
        
        for(i=0;i<chunksizeInt;i++){
            sendPkt[i]=nsn->data[i];
        }
        sendPkt[i]='\0'; //data
        //strcpy(sendPkt,nsn->data);
        strcat(sendPkt,"-ssn:");
        tostring(temp_start,nsn->startSeqNumber); //startSeqNumber
        strcat(sendPkt,temp_start);
        strcat(sendPkt,"-lsn:");
        tostring(temp_end,nsn->endSeqNumber); //endSeqNumber
        strcat(sendPkt,temp_end);
        /*    final sendpkt="<data>-ssn:<startSeqNumber>-lsn:<endSeqNumber>"   */
        
        
        printf("sending data to client: %s ,\n",sendPkt);
        if (base==nsn){
            timer = 0;
        }
        if (sendto(sock, sendPkt, strlen(sendPkt), 0, (struct sockaddr *)&rem, len) < 0)
            perror("sendto");
        
        if(nsn->startSeqNumber==(chunksizeInt*(lastpacket-1)+isnInt)){
            lastpktflag=1;
        }
        else traverseNsn(1);
        printf("lpf: %d ,\n",lastpktflag);
    }
    
    
    //    sprintf(sendPkt, "Packet 0");
    //    if (sendto(sock, sendPkt, strlen(sendPkt), 0, (struct sockaddr *)&rem, len) < 0)
    //        perror("sendto");
    
}

void toResendFunc(int sock, struct sockaddr_in rem, socklen_t len){
    
    toResend = base;
    int i=0;
    char sendPkt[BUFSIZE];
    char temp_start[11],temp_end[11];
    
    while (toResend != nsn) {
        
        /* Puts the required contents of the packet to be sent */
        
        for(i=0;i<chunksizeInt;i++){
            sendPkt[i]=toResend->data[i];
        }
        sendPkt[i]='\0'; //data
        //strcpy(sendPkt,nsn->data);
        strcat(sendPkt,"-ssn:");
        tostring(temp_start,toResend->startSeqNumber); //startSeqNumber
        strcat(sendPkt,temp_start);
        strcat(sendPkt,"-lsn:");
        tostring(temp_end,toResend->endSeqNumber); //endSeqNumber
        strcat(sendPkt,temp_end);
        /*    final sendpkt="<data>-ssn:<startSeqNumber>-lsn:<endSeqNumber>"   */
        
        if (sendto(sock, sendPkt, strlen(sendPkt), 0, (struct sockaddr *)&rem, len) < 0)
            perror("sendto");
        
        toResend = toResend->next;
    }
}

int createPackets(const char *name, int chunk){
    
    FILE *fp;
    
    int i, j, seqNum = isnInt, size = 0, rem = 0;
    
    unsigned char buffer[chunk+1];
    
    unsigned char *ptr, ptr2;
    
    fp = fopen(name, "rb");
    
    fseek(fp, 0, SEEK_END); // seek to end of file
    size = ftell(fp); // get current file pointer
    fseek(fp, 0, SEEK_SET); // seek back to beginning of file
    // proceed with allocating memory and reading the file
    
    printf("\n FILE SIZE : %d bytes\n", size);
    
    fileSize = size;
    
    while(!feof(fp))
    {
        while(fread(buffer, chunk, 1, fp) !=0)
        {
            lastpacket++;
            rem++;
            //            ptr = buffer;
            //commented out printf checkers
            for(i=0; i < chunk; i++)
            {
                printf("%X ", buffer[i]);
                
            }
            //            for(j = 0; j < chunk; j++)
            //            {
            //                if(isprint(buffer[j]))
            //                    printf("%c", buffer[j]);
            //                else
            //                    printf("." );
            //            }
            //             printf(" --> ");
            //            for(j = 0; j < chunk; j++)
            //            {
            //                if(isprint(*(ptr + j) ))
            //                    printf("%c", *(ptr + j ));
            //                else
            //                    printf("." );
            //
            //            }
            
            insertPacketsToLinkList(buffer, seqNum, seqNum+chunk-1,chunk);
            seqNum = seqNum + chunk;
            /*commented out code below prints data*/
            //                        for(j = 0; j < 16; j++)
            //                        {
            //                            if(isprint(buffer[j]))
            //                                printf("%c", buffer[j]);
            //                            else
            //                                printf("." );
            //                        }
        }
        
        /*block below determines last packet with bytes less than chunksize*/
        rem=size-(rem*chunk);
        if (rem > 0) {
            lastpacket++;
            fread(buffer, rem, 1, fp);
            //                ptr=buffer;
            for(i=0; i < chunk; i++)
            {
                if(i>=rem)
                    buffer[i]='\0';
                
            }
            printf("buffer in rem %d: %s\n",rem,buffer);
            for(i=0; i < chunk; i++)
            {
                printf("%X ", buffer[i]);
                
            }
            /*commented out printf checkers*/
            //            for(j = 0; j < rem; j++)
            //            {
            //                if(isprint(*(ptr + j) ))
            //                    printf("%c", *(ptr + j ));
            //                else
            //                    printf("." );
            //
            //            }
            
            insertPacketsToLinkList(buffer, seqNum, seqNum+rem-1,chunk);
            //buffer[rem]='\0';
            seqNum = seqNum + chunk;
            printf("\n");
        }
    }
    
    fclose(fp);
    
    return 0;
}

//void insertPacketsToLinkList(unsigned char data[], int start, int end, int chunk){
//    int i=0;
//    struct node *temp;
//    temp=(struct node*)malloc(sizeof(struct node));
//    printf("from buffer:%s\n",data);
//    //    strcpy(temp->data,data);
//    //    memcpy(temp->data, data, strlen(data)+1);
//    while (data[i]!='\0'){
//        temp->data[i]=data[i];
//        i++;
//    }
//    temp->data[i]='\0';
//    temp->startSeqNumber=start;
//    temp->endSeqNumber=end;
//    if (head == NULL) {
//        head = temp;
//        head->next = NULL;
//    }else{
//        if (tail == NULL) {
//            tail = temp;
//            head->next = tail;
//            tail->next = NULL;
//            
//        }else {
//            tail->next = temp;
//            temp->next = NULL;
//            tail = temp;
//        }
//    }
//    for(i=0; i < chunk; i++)
//    {
//        printf("%X ", *(temp->data+i));
//        
//    }
//    printf("\n");
//}

void insertPacketsToLinkList(unsigned char data[], int start, int end, int chunk){
    int i;
    struct node *temp;
    temp=(struct node*)malloc(sizeof(struct node));
    printf("from buffer:%s\n",data);
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
    for(i=0; i < chunk; i++)
    {
        printf("%X ", *(temp->data+i));
        
    }
    printf("\n");
}

void display()
{
    struct node *temp, *recv;
    temp=head;
    while(temp!=NULL)
    {
        printf("start: %d \n",temp->startSeqNumber);
        printf("end: %d \n",temp->endSeqNumber);
        temp=temp->next;
    }
}

void traverseBase(int n){ //traverse base node n times
    
    struct node *temp;
    int i = 0;
    
    if(base == NULL){
        temp = head;
    }else {
        temp = base;
    }
    
    for (i = 0; i<n; i++) {
        temp=temp->next;
    }
    
    base = temp;
}

void traverseNsn(int n){ //traverse nsn(next seq node) node n times
    
    struct node *temp;
    int i = 0;
    
    if(nsn == NULL){
        temp = head;
    }else {
        temp = nsn;
    }
    
    for (i = 0; i<n; i++) {
        temp=temp->next;
    }
    
    nsn = temp;
}

int tokenizeAck(char *msg, char *delim)
{
    char *token;
    char *storage [20];
    int pos = 0;
    
    token = strtok(msg, delim);
    while( token != NULL ){
        storage[pos] = token;
        token = strtok(NULL, delim);
        pos++;
    }
    
    return strtoint(storage[1]);
    
}

void tokenizeConfig(char *config, char *delim)
{
    char *storage [30];
    char *token;
    char *value;
    char str[1024];
    int pos = 0, i = 0, tokNum = 0;
    
    /* get the first token */
    token = strtok(config, delim);
    
    /* walk through other tokens */
    while( token != NULL ){
        storage[pos] = token;
        //        printf( "tokens %s\n", token );//commented out printf checker
        pos++;
        tokNum++;
        token = strtok(NULL, delim);
    }
    storage[pos] = NULL;
    
    for (i=0; i<tokNum; i++) {
        //        printf("\n%s %s %s", storage[i],storage[i+1],storage[i+2]);
        if (strcmp(storage[i], "filename") == 0) {
            strcpy(filename,storage[i+1]);
            i++;
        }else if (strcmp(storage[i], "windowsize") == 0) {
            strcpy(windowsize,storage[i+1]);
            i++;
        }else if (strcmp(storage[i], "chunksize") == 0) {
            strcpy(chunksize,storage[i+1]);
            i++;
        }else if (strcmp(storage[i], "isn") == 0) {
            strcpy(isn,storage[i+1]);
            i++;
        }else if (strcmp(storage[i], "timeout") == 0) {
            strcpy(timeout,storage[i+1]);
            i++;
        }else{
            strcpy(addr,storage[i+1]);
            strcpy(port,storage[i+2]);
            i = i+2;
        }
        
    }
    
    windowsizeInt = strtoint(windowsize);
    chunksizeInt = strtoint(chunksize);
    isnInt = strtoint(isn);
    timeoutInt = strtoint(timeout);
    
    printf(" FILENAME: %s", filename);
    printf("\n CHUNK SIZE: %d", chunksizeInt);
    printf("\n WINDOW SIZE: %d", windowsizeInt);
    printf("\n INITIAL SEQ NUM: %d", isnInt);
    printf("\n TIMEOUT: %d", timeoutInt);
    printf("\n ADDRESS: %s", addr);
    printf("\n PORT: %s \n", port);
    
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
