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

#define MAXBUFF 50000
#define PACKSIZE 1400

typedef struct packet_struct{
    char msg[PACKSIZE];
    int seqnum;
} packet;

typedef struct ack_struct{
    int seqnum;
} ack;

int     g_packets_num = 0;
int     g_packets_lastsize = 0;
int     g_winsize = PACKSIZE;
int     g_ssthresh = 65536;
int     g_seqnum = 0;
int     g_seqnum_request = 0;
int     g_sockfd;
int     g_packet_recv[MAXBUFF];
int     g_dup = 0;
int     g_dup_counter = 0;
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

void binaryCopy(char *dest, char *source, int st, int len){
    int i, j;
    j = st; // if st==0 then copy from the beginning
    for(i = 0; i < len; i++){
        dest[i] = source[j];
        j++;
    }
}

int splitFile(unsigned long long int bytesToTransfer) {
    int i = 0;
    //比最大限制大的传输量
    // printf("YE\n");
    for(i = 0; bytesToTransfer > PACKSIZE; i++){
        binaryCopy(g_packets[i], g_file_buffer, i * PACKSIZE, PACKSIZE);
        bytesToTransfer -= PACKSIZE;
    }
    //比最大量小或最后一个剩余量
    if(bytesToTransfer > 0){
        g_packets_lastsize = bytesToTransfer;
        binaryCopy(g_packets[i], g_file_buffer, i * PACKSIZE, bytesToTransfer);
        i++;
    }
    free(g_file_buffer);
    return i;
}

void sendPacket(char * packet, int sockfd, int bytesToTransfer, struct addrinfo *p){
    int numbytes;
    if ((numbytes = sendto(sockfd, packet, bytesToTransfer, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    
    char port[5];
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
    g_packets_num = splitFile(bytesToTransfer);
    int i;
    for(i = 0; i < g_packets_num; i++){
        if(i < g_packets_num - 1){
            sendPacket(g_packets[i], sockfd, PACKSIZE, p);
        }
        else{
            sendPacket(g_packets[i], sockfd, g_packets_lastsize, p);
        }
    }

    freeaddrinfo(servinfo);
    
    close(sockfd);

    return 0;
}