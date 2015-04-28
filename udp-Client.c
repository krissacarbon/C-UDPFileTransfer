/*
        demo-udp-03: udp-send: a simple udp client (KRISSA: this will be the server)
	send udp messages
	This sends a sequence of messages (the # of messages is defined in MSGS)
	The messages are sent to a port defined in SERVICE_PORT

        usage:  udp-send

        Paul Krzyzanowski
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

void insertPacketsToLinkList(unsigned char data[], int start, int end, int chunk);
void display();
void tokenizePacket(char *data, char *delim, char *dataUntokened);
int strtoint_n(char* str, int n);
int strtoint(char* str);
void tostring(char str[], int num);
void delay(int milliseconds);

struct node
{
    unsigned char data[BUFLEN];
    int startSeqNumber;
    int endSeqNumber;
    struct node *next;
}*head, *tail;

char filename[BUFLEN], windowsize[BUFLEN], chunksize[BUFLEN], isn[BUFLEN], timeout[BUFLEN], delayAck[BUFLEN], addrPort[BUFLEN], config[BUFLEN];
int delayInMilSec;

int main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t fd, i, slen=sizeof(remaddr);
    char buf[BUFLEN];	/* message buffer */
    int recvlen, msgcnt=0, n=0, x=0;		/* # bytes in acknowledgement message */
	char *server = "127.0.0.1";	/* change this to use a different server */
    int isVerbose = 0, tempStartInt = 0;
    unsigned char *currData;
    char temp_start[11], temp_ack[11];
    int chunksizeInt = strtoint(chunksize);
    char temp_data[chunksizeInt];
    
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
    
    delayInMilSec = strtoint(delayAck)*1000;
    
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

	/* now let's send the configurations to server */

//	for (i=0; i < MSGS; i++) {
////		printf("Sending packet %d to %s port %d\n", i, server, SERVICE_PORT);
//        printf("Sending filename to server...");
////        sprintf(buf, filename);
//        strcpy(buf, config);
//		if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
//			perror("sendto");
//			exit(1);
//		}
//		/* now receive an acknowledgement from the server */
//		recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
//                if (recvlen >= 0) {
//                        buf[recvlen] = 0;	/* expect a printable string - terminate it */
//                        printf("server received filename: \"%s\"\n", buf);
//                }
//    }
    
    /* now let's send the configurations to server */
    strcpy(buf, config);
    if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
        perror("sendto");
    }
    
    for (;;){
        
        /*start listening*/
        recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
        
        /*process received data from Server*/
        if (recvlen >= 0) {
            buf[recvlen] = 0;	/* expect a printable string - terminate it */
            if (strstr(buf, "ssn") != NULL) {/*receiving data packets*/
                printf("received packet : %s \n", buf);
//                currData = buf;
                //TODO: tokenized received packet
                strncpy(temp_data, buf, strtoint(chunksize));
                tokenizePacket(buf,"-:",temp_data);
//                insertPacketsToLinkList(currData,0,0);
                
                
                strcpy(buf,"ACK-");
                
                if (tail != NULL) {
                    tempStartInt = tail->startSeqNumber+strtoint(chunksize);
                }else {
                    tempStartInt = head->startSeqNumber+strtoint(chunksize);
                }
                tostring(temp_start,tempStartInt);
                strcat(buf,temp_start);
                
                
                //TODO: delay here (for loop does the delay)
                for(x=0;x<1;x++)
                {
                    printf("%d\n",x+1);
                    delay(delayInMilSec);
                    printf("sending \"%s\" to server\n", buf);
                    
                    if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
                        perror("sendto");
                    }
                    
                }
                
                

            }else {
                printf("received complete message : %s \n", buf);
                
                display();
                
                close(fd);
            }
        }
        
//        if (recvlen > 0) {
//            buf[recvlen] = 0;
//            printf("received packet: \"%s\" (%d bytes)\n", buf, slen);
//        }
//        else{
//            printf("uh oh - something went wrong!\n");
//        }
//        
//        sprintf(buf, "ack %d", msgcnt++);
//        printf("sending response \"%s\"\n", buf);
//        if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
//            perror("sendto");
//            exit(1);
//        }

    
    }
    
//	close(fd);
	return 0;
}

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
