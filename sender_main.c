#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define MAXBUFF 5000
#define PACKSIZE 1400

typedef struct packet_struct{
    char msg[PACKSIZE];
    int seqnum;
} packet;

typedef struct ack_struct{
    int seqnum;
} ack;

char    g_packets[MAXBUFF][PACKSIZE];
char*   g_file_buffer;

void    reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
void    readFile(char* filename, unsigned long long int bytesToTransfer);


int main(int argc, char** argv)
{
    unsigned short int udpPort;
    unsigned long long int numBytes;
    
    if(argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);
    
    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
} 

void readFile(char* filename, unsigned long long int bytesToTransfer) {
    // printf("YE\n");
    FILE *fp;
    fp = fopen(filename,"rb");
    long filesize;
    fseek(fp, 0L, SEEK_END);
    // printf("YE\n");
    filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    // printf("YE\n");
    int i=0;
    g_file_buffer = (char *)malloc(sizeof(char)*(filesize+1));
    char c;
    for(i = 0; i < bytesToTransfer; i++)
    {   
        c = fgetc(fp);
        g_file_buffer[i]=c;
    }
    g_file_buffer[i]='\0';
    fclose (fp);
}

void splitFile(unsigned long long int bytesToTransfer) {
    int i = 0;

    for(i = 0; bytesToTransfer > PACKSIZE; i++){
        strcpy(g_packets[i], g_file_buffer);
        bytesToTransfer -= PACKSIZE;
    }

    strcpy(g_packets[i], g_file_buffer);
    g_packets[i][bytesToTransfer] = EOF;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char* port;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    sprintf(port, "%d", hostUDPport);

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "sender: failed to bind socket\n");
        return 2;
    }

    readFile(filename, bytesToTransfer);
    splitFile(bytesToTransfer);

    if ((numbytes = sendto(sockfd, g_file_buffer, bytesToTransfer, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);
    
    close(sockfd);

    return 0;
}